#include "ilios.h"

static void statement_mark(void *ptr);
static void statement_destroy(void *ptr);
static size_t statement_memsize(const void *ptr);
static void statement_compact(void *ptr);

const rb_data_type_t cassandra_statement_data_type = {
    "Ilios::Cassandra::Statement",
    {
        statement_mark,
        statement_destroy,
        statement_memsize,
        statement_compact,
    },
    0, 0,
    RUBY_TYPED_FREE_IMMEDIATELY | RUBY_TYPED_WB_PROTECTED | RUBY_TYPED_FROZEN_SHAREABLE,
};

typedef struct
{
    const CassPrepared *prepared;
    CassStatement *statement;
    VALUE bound_values;
} statement_bind_context;

typedef enum {
    bind_target_kind_statement,
    bind_target_kind_collection
} statement_bind_target_kind;

// Where a value is bound to: a named parameter of a statement, or an
// element of a collection being built. Keeps the type switch in
// statement_bind_value single so that top-level columns and collection
// elements always support exactly the same scalar types.
typedef struct
{
    statement_bind_target_kind kind;
    union {
        struct {
            CassStatement *statement;
            const char *name;
        } statement;
        CassCollection *collection;
    } as;
} statement_bind_target;

static CassError bind_target_int8(statement_bind_target *target, cass_int8_t value)
{
    if (target->kind == bind_target_kind_statement) {
        return cass_statement_bind_int8_by_name(target->as.statement.statement, target->as.statement.name, value);
    }
    return cass_collection_append_int8(target->as.collection, value);
}

static CassError bind_target_int16(statement_bind_target *target, cass_int16_t value)
{
    if (target->kind == bind_target_kind_statement) {
        return cass_statement_bind_int16_by_name(target->as.statement.statement, target->as.statement.name, value);
    }
    return cass_collection_append_int16(target->as.collection, value);
}

static CassError bind_target_int32(statement_bind_target *target, cass_int32_t value)
{
    if (target->kind == bind_target_kind_statement) {
        return cass_statement_bind_int32_by_name(target->as.statement.statement, target->as.statement.name, value);
    }
    return cass_collection_append_int32(target->as.collection, value);
}

static CassError bind_target_int64(statement_bind_target *target, cass_int64_t value)
{
    if (target->kind == bind_target_kind_statement) {
        return cass_statement_bind_int64_by_name(target->as.statement.statement, target->as.statement.name, value);
    }
    return cass_collection_append_int64(target->as.collection, value);
}

static CassError bind_target_float(statement_bind_target *target, cass_float_t value)
{
    if (target->kind == bind_target_kind_statement) {
        return cass_statement_bind_float_by_name(target->as.statement.statement, target->as.statement.name, value);
    }
    return cass_collection_append_float(target->as.collection, value);
}

static CassError bind_target_double(statement_bind_target *target, cass_double_t value)
{
    if (target->kind == bind_target_kind_statement) {
        return cass_statement_bind_double_by_name(target->as.statement.statement, target->as.statement.name, value);
    }
    return cass_collection_append_double(target->as.collection, value);
}

static CassError bind_target_bool(statement_bind_target *target, cass_bool_t value)
{
    if (target->kind == bind_target_kind_statement) {
        return cass_statement_bind_bool_by_name(target->as.statement.statement, target->as.statement.name, value);
    }
    return cass_collection_append_bool(target->as.collection, value);
}

static CassError bind_target_string(statement_bind_target *target, const char *value)
{
    if (target->kind == bind_target_kind_statement) {
        return cass_statement_bind_string_by_name(target->as.statement.statement, target->as.statement.name, value);
    }
    return cass_collection_append_string(target->as.collection, value);
}

static CassError bind_target_uuid(statement_bind_target *target, CassUuid value)
{
    if (target->kind == bind_target_kind_statement) {
        return cass_statement_bind_uuid_by_name(target->as.statement.statement, target->as.statement.name, value);
    }
    return cass_collection_append_uuid(target->as.collection, value);
}

static CassError bind_target_collection(statement_bind_target *target, const CassCollection *value)
{
    if (target->kind == bind_target_kind_statement) {
        return cass_statement_bind_collection_by_name(target->as.statement.statement, target->as.statement.name, value);
    }
    return cass_collection_append_collection(target->as.collection, value);
}

void statement_default_config(CassandraStatement *cassandra_statement)
{
    cassandra_statement->bound_values = Qnil;
    cassandra_statement->page_size = DEFAULT_PAGE_SIZE;
    cassandra_statement->idempotent = idempotency_unset;
    cass_statement_set_paging_size(cassandra_statement->statement, DEFAULT_PAGE_SIZE);
}

static void statement_bind_value(statement_bind_target *target, const CassDataType *data_type,
                                 VALUE value, VALUE key, const char *role);
static void statement_bind_collection(statement_bind_target *target, const CassDataType *data_type,
                                      VALUE value, VALUE key);

static void statement_bind_check(CassError result, VALUE key)
{
    if (result != CASS_OK) {
        rb_raise(eStatementError, "Failed to bind value of %"PRIsVALUE" column: %s", key, cass_error_desc(result));
    }
}

static void statement_bind_value(statement_bind_target *target, const CassDataType *data_type,
                                 VALUE value, VALUE key, const char *role)
{
    const CassValueType value_type = cass_data_type_type(data_type);
    CassError result;

    if (NIL_P(value)) {
        if (target->kind == bind_target_kind_collection) {
            // The driver has no cass_collection_append_null: Cassandra
            // collections cannot contain null elements.
            rb_raise(rb_eTypeError, "nil is not allowed as a collection %s in %"PRIsVALUE"", role, key);
        }
        statement_bind_check(cass_statement_bind_null_by_name(target->as.statement.statement, target->as.statement.name), key);
        return;
    }

    switch (value_type) {
    case CASS_VALUE_TYPE_TINY_INT:
        {
            long v = NUM2LONG(value);

            if (v < INT8_MIN || v > INT8_MAX) {
                rb_raise(rb_eRangeError, "Invalid value: %ld", v);
            }
            result = bind_target_int8(target, (cass_int8_t)v);
        }
        break;

    case CASS_VALUE_TYPE_SMALL_INT:
        {
            long v = NUM2LONG(value);

            if (v < INT16_MIN || v > INT16_MAX) {
                rb_raise(rb_eRangeError, "Invalid value: %ld", v);
            }

            result = bind_target_int16(target, (cass_int16_t)v);
        }
        break;

    case CASS_VALUE_TYPE_INT:
        {
            long v = NUM2LONG(value);

            if (v < INT32_MIN || v > INT32_MAX) {
                rb_raise(rb_eRangeError, "Invalid value: %ld", v);
            }

            result = bind_target_int32(target, (cass_int32_t)v);
        }
        break;

    case CASS_VALUE_TYPE_BIGINT:
        result = bind_target_int64(target, NUM2LONG(value));
        break;

    case CASS_VALUE_TYPE_FLOAT:
        {
            double v = NUM2DBL(value);

            if (!isnan(v) && !isinf(v) && (v < -FLT_MAX || v > FLT_MAX)) {
                rb_raise(rb_eRangeError, "Invalid value: %lf", v);
            }

            result = bind_target_float(target, v);
        }
        break;

    case CASS_VALUE_TYPE_DOUBLE:
        result = bind_target_double(target, NUM2DBL(value));
        break;

    case CASS_VALUE_TYPE_BOOLEAN:
        result = bind_target_bool(target, RTEST(value) ? cass_true : cass_false);
        break;

    case CASS_VALUE_TYPE_TEXT:
    case CASS_VALUE_TYPE_ASCII:
    case CASS_VALUE_TYPE_VARCHAR:
        if (SYMBOL_P(value)) {
            // rb_sym2str returns the Symbol's fstring; no allocation, so
            // converting on every execution is fine.
            value = rb_sym2str(value);
        }
        result = bind_target_string(target, StringValueCStr(value));
        break;

    case CASS_VALUE_TYPE_TIMESTAMP:
        if (rb_obj_class(value) != rb_cTime) {
            if (rb_respond_to(value, id_to_time)) {
                value = rb_funcall(value, id_to_time, 0);
            } else {
                rb_raise(rb_eTypeError, "no implicit conversion of %"PRIsVALUE" to Time", rb_obj_class(value));
            }
        }
        result = bind_target_int64(target, (cass_int64_t)(NUM2DBL(rb_Float(value)) * 1000));
        break;

    case CASS_VALUE_TYPE_UUID:
        {
            CassUuid uuid = { 0, 0 };
            const char *uuid_string = StringValueCStr(value);

            result = cass_uuid_from_string(uuid_string, &uuid);
            if (result != CASS_OK) {
                rb_raise(eStatementError, "Invalid UUID was given: %"PRIsVALUE"=%"PRIsVALUE"", key, value);
            }

            result = bind_target_uuid(target, uuid);
        }
        break;

    case CASS_VALUE_TYPE_LIST:
    case CASS_VALUE_TYPE_SET:
    case CASS_VALUE_TYPE_MAP:
        statement_bind_collection(target, data_type, value, key);
        return;

    default:
        rb_raise(rb_eTypeError, "Unsupported %"PRIsVALUE" type: %"PRIsVALUE"=%"PRIsVALUE"", rb_obj_class(value), key, value);
    }

    statement_bind_check(result, key);
}

typedef struct
{
    statement_bind_target target;
    const CassDataType *data_type;
    VALUE value;
    VALUE key;
} statement_bind_collection_args;

struct statement_bind_map_ctx {
    statement_bind_target *target;
    const CassDataType *key_type;
    const CassDataType *value_type;
    VALUE key;
};

static int statement_bind_map_cb(VALUE k, VALUE v, VALUE arg)
{
    struct statement_bind_map_ctx *ctx = (struct statement_bind_map_ctx *)arg;

    // A map is appended as alternating key, value pairs.
    statement_bind_value(ctx->target, ctx->key_type, k, ctx->key, "key");
    statement_bind_value(ctx->target, ctx->value_type, v, ctx->key, "value");
    return ST_CONTINUE;
}

static VALUE statement_bind_collection_body(VALUE arg)
{
    statement_bind_collection_args *args = (statement_bind_collection_args *)arg;

    if (cass_data_type_type(args->data_type) == CASS_VALUE_TYPE_MAP) {
        struct statement_bind_map_ctx ctx;

        ctx.target = &args->target;
        ctx.key_type = cass_data_type_sub_data_type(args->data_type, 0);
        ctx.value_type = cass_data_type_sub_data_type(args->data_type, 1);
        ctx.key = args->key;
        rb_hash_foreach(args->value, statement_bind_map_cb, (VALUE)&ctx);
    } else {
        const CassDataType *element_type = cass_data_type_sub_data_type(args->data_type, 0);
        long length = RARRAY_LEN(args->value);

        for (long i = 0; i < length; i++) {
            statement_bind_value(&args->target, element_type, RARRAY_AREF(args->value, i), args->key, "element");
        }
    }
    return Qnil;
}

/*
 * Builds a CassCollection from a snapshotted (frozen) Array or Hash and binds
 * it to the target. The value is guaranteed to be an Array (list/set) or Hash
 * (map) because statement_snapshot_value normalizes it in Statement#bind
 * before the first statement_bind_value call, and executions only replay
 * values stored in bound_values.
 */
static void statement_bind_collection(statement_bind_target *target, const CassDataType *data_type,
                                      VALUE value, VALUE key)
{
    const CassValueType type = cass_data_type_type(data_type);
    statement_bind_collection_args args;
    CassCollection *collection;
    size_t item_count;
    CassError result;
    int state = 0;

    if (type == CASS_VALUE_TYPE_MAP) {
        if (cass_data_type_sub_type_count(data_type) != 2) {
            rb_raise(eStatementError, "Invalid map type of %"PRIsVALUE" column", key);
        }
        Check_Type(value, T_HASH);
        item_count = (size_t)RHASH_SIZE(value);
    } else {
        if (cass_data_type_sub_type_count(data_type) != 1) {
            rb_raise(eStatementError, "Invalid collection type of %"PRIsVALUE" column", key);
        }
        Check_Type(value, T_ARRAY);
        item_count = (size_t)RARRAY_LEN(value);
    }

    // cass_collection_new_from_data_type (not cass_collection_new) so that
    // nested element types are propagated to the driver.
    collection = cass_collection_new_from_data_type(data_type, item_count);
    if (collection == NULL) {
        rb_raise(eStatementError, "Failed to create a collection for %"PRIsVALUE" column", key);
    }

    args.target.kind = bind_target_kind_collection;
    args.target.as.collection = collection;
    args.data_type = data_type;
    args.value = value;
    args.key = key;

    rb_protect(statement_bind_collection_body, (VALUE)&args, &state);
    if (state) {
        cass_collection_free(collection);
        rb_jump_tag(state);
    }

    result = bind_target_collection(target, collection);
    cass_collection_free(collection);
    statement_bind_check(result, key);
}

static VALUE statement_snapshot_value(const CassDataType *data_type, VALUE value);

struct statement_snapshot_map_ctx {
    const CassDataType *key_type;
    const CassDataType *value_type;
    VALUE snapshot;
};

static int statement_snapshot_map_cb(VALUE k, VALUE v, VALUE arg)
{
    struct statement_snapshot_map_ctx *ctx = (struct statement_snapshot_map_ctx *)arg;

    rb_hash_aset(ctx->snapshot,
                 statement_snapshot_value(ctx->key_type, k),
                 statement_snapshot_value(ctx->value_type, v));
    return ST_CONTINUE;
}

/*
 * Returns a deep-frozen snapshot of the value so a later in-place mutation by
 * the caller (or another thread) doesn't change what gets bound at execution
 * time. Executions then only walk frozen containers, which makes re-binding
 * on execute safe without re-validating.
 */
static VALUE statement_snapshot_value(const CassDataType *data_type, VALUE value)
{
    if (NIL_P(value)) {
        return Qnil;
    }

    switch (cass_data_type_type(data_type)) {
    case CASS_VALUE_TYPE_LIST:
    case CASS_VALUE_TYPE_SET:
        {
            const CassDataType *element_type = cass_data_type_sub_data_type(data_type, 0);
            VALUE array;
            VALUE snapshot;
            long length;

            if (element_type == NULL) {
                rb_raise(eStatementError, "Invalid collection type: missing element type");
            }

            if (RB_TYPE_P(value, T_ARRAY)) {
                array = value;
            } else if (rb_obj_is_kind_of(value, cSet)) {
                array = rb_funcall(value, id_to_a, 0);
                // A Set subclass may override to_a to return anything.
                Check_Type(array, T_ARRAY);
            } else {
                rb_raise(rb_eTypeError, "no implicit conversion of %"PRIsVALUE" into Array or Set", rb_obj_class(value));
            }

            length = RARRAY_LEN(array);
            snapshot = rb_ary_new_capa(length);
            for (long i = 0; i < length && i < RARRAY_LEN(array); i++) {
                rb_ary_push(snapshot, statement_snapshot_value(element_type, rb_ary_entry(array, i)));
            }
            return rb_ary_freeze(snapshot);
        }

    case CASS_VALUE_TYPE_MAP:
        {
            struct statement_snapshot_map_ctx ctx;

            Check_Type(value, T_HASH);
            ctx.key_type = cass_data_type_sub_data_type(data_type, 0);
            ctx.value_type = cass_data_type_sub_data_type(data_type, 1);
            if (ctx.key_type == NULL || ctx.value_type == NULL) {
                rb_raise(eStatementError, "Invalid collection type: missing map key or value type");
            }
            ctx.snapshot = rb_hash_new();
            rb_hash_foreach(value, statement_snapshot_map_cb, (VALUE)&ctx);
            return rb_hash_freeze(ctx.snapshot);
        }

    case CASS_VALUE_TYPE_TEXT:
    case CASS_VALUE_TYPE_ASCII:
    case CASS_VALUE_TYPE_VARCHAR:
        if (SYMBOL_P(value)) {
            // Symbols are immutable; no defensive copy needed.
            return value;
        }
        // Also converts to_str objects here so the converted String (not the
        // possibly mutable object) is what gets stored and re-bound.
        StringValue(value);
        return rb_str_new_frozen(value);

    default:
        if (RB_TYPE_P(value, T_STRING)) {
            return rb_str_new_frozen(value);
        }
        return value;
    }
}

static int hash_cb(VALUE key, VALUE value, VALUE arg)
{
    statement_bind_context *ctx = (statement_bind_context *)arg;
    const CassDataType* data_type;
    const char *name;
    statement_bind_target target;

    if (SYMBOL_P(key)) {
        key = rb_sym2str(key);
    }
    name = StringValueCStr(key);

    data_type = cass_prepared_parameter_data_type_by_name(ctx->prepared, name);
    if (data_type == NULL) {
        rb_raise(eStatementError, "Invalid name %s was given.", name);
    }

    target.kind = bind_target_kind_statement;
    target.as.statement.statement = ctx->statement;
    target.as.statement.name = name;

    if (!NIL_P(ctx->bound_values)) {
        // Snapshot before binding so the value bound now and the value
        // replayed by later executions are the same deep-frozen object.
        value = statement_snapshot_value(data_type, value);
    }

    statement_bind_value(&target, data_type, value, key, NULL);

    if (!NIL_P(ctx->bound_values)) {
        rb_hash_aset(ctx->bound_values, key, value);
    }

    return ST_CONTINUE;
}

typedef struct
{
    statement_bind_context ctx;
    VALUE hash;
} statement_rebind_args;

static VALUE statement_rebind_body(VALUE arg)
{
    statement_rebind_args *args = (statement_rebind_args *)arg;

    rb_hash_foreach(args->hash, hash_cb, (VALUE)&args->ctx);
    return Qnil;
}

/*
 * Builds a fresh CassStatement carrying the current configuration and bound
 * values for a single execution. The returned statement must not be mutated
 * once handed to the driver, and the caller owns it: it must stay alive until
 * the execution's future resolves and be freed exactly once afterwards.
 */
CassStatement *statement_build_for_execution(CassandraStatement *cassandra_statement)
{
    CassStatement *statement = cass_prepared_bind(cassandra_statement->prepared);

    cass_statement_set_paging_size(statement, cassandra_statement->page_size);
    if (cassandra_statement->idempotent != idempotency_unset) {
        cass_statement_set_is_idempotent(statement, cassandra_statement->idempotent == idempotency_true ? cass_true : cass_false);
    }

    if (!NIL_P(cassandra_statement->bound_values)) {
        statement_rebind_args args;
        int state = 0;

        args.ctx.prepared = cassandra_statement->prepared;
        args.ctx.statement = statement;
        args.ctx.bound_values = Qnil;
        args.hash = cassandra_statement->bound_values;

        rb_protect(statement_rebind_body, (VALUE)&args, &state);
        if (state) {
            cass_statement_free(statement);
            rb_jump_tag(state);
        }
    }
    return statement;
}

/**
 * Binds a specified column value to a query.
 * A hash object should be given with column name as key.
 *
 * @param hash [Hash] A hash object to bind.
 * @return [Cassandra::Statement] self.
 * @raise [RangeError] If an invalid range of values was given.
 * @raise [TypeError] If an invalid type of values was given.
 * @raise [Cassandra::StatementError] If an invalid column name was given.
 */
static VALUE statement_bind(VALUE self, VALUE hash)
{
    CassandraStatement *cassandra_statement;
    statement_bind_context ctx;
    VALUE bound_values;

    Check_Type(hash, T_HASH);
    TypedData_Get_Struct(self, CassandraStatement, &cassandra_statement_data_type, cassandra_statement);

    // Merge into a copy instead of mutating in place: the previous hash may be
    // shared with a frozen (Ractor-shareable) statement or be iterated by an
    // execution on another thread.
    if (NIL_P(cassandra_statement->bound_values)) {
        bound_values = rb_hash_new();
    } else {
        bound_values = rb_hash_dup(cassandra_statement->bound_values);
    }
    RB_OBJ_WRITE(self, &cassandra_statement->bound_values, bound_values);

    ctx.prepared = cassandra_statement->prepared;
    ctx.statement = cassandra_statement->statement;
    ctx.bound_values = bound_values;

    rb_hash_foreach(hash, hash_cb, (VALUE)&ctx);
    return self;
}

/**
 * Sets the statement's page size. The default is +10000+.
 *
 * @param page_size [Integer] A page size.
 * @return [Cassandra::Statement] self.
 */
static VALUE statement_page_size(VALUE self, VALUE page_size)
{
    CassandraStatement *cassandra_statement;

    GET_STATEMENT(self, cassandra_statement);
    cassandra_statement->page_size = NUM2INT(page_size);
    cass_statement_set_paging_size(cassandra_statement->statement, cassandra_statement->page_size);
    return self;
}

/**
 * Sets whether the statement is idempotent. Idempotent statements are able to be
 * automatically retried after timeouts/errors and can be speculatively executed.
 * The default is +false+.
 *
 * @param idempotent [Boolean] Whether the statement is idempotent.
 * @return [Cassandra::Statement] self.
 */
static VALUE statement_idempotent(VALUE self, VALUE idempotent)
{
    CassandraStatement *cassandra_statement;

    GET_STATEMENT(self, cassandra_statement);
    cassandra_statement->idempotent = RTEST(idempotent) ? idempotency_true : idempotency_false;
    cass_statement_set_is_idempotent(cassandra_statement->statement, cassandra_statement->idempotent == idempotency_true ? cass_true : cass_false);
    return self;
}

static void statement_mark(void *ptr)
{
    CassandraStatement *cassandra_statement = (CassandraStatement *)ptr;
    rb_gc_mark_movable(cassandra_statement->session_obj);
    rb_gc_mark_movable(cassandra_statement->bound_values);
}

static void statement_destroy(void *ptr)
{
    CassandraStatement *cassandra_statement = (CassandraStatement *)ptr;

    if (cassandra_statement->prepared) {
        cass_prepared_free(cassandra_statement->prepared);
    }
    if (cassandra_statement->statement) {
        cass_statement_free(cassandra_statement->statement);
    }
    xfree(cassandra_statement);
}

static size_t statement_memsize(const void *ptr)
{
    return sizeof(CassandraStatement);
}

static void statement_compact(void *ptr)
{
    CassandraStatement *cassandra_statement = (CassandraStatement *)ptr;

    cassandra_statement->session_obj = rb_gc_location(cassandra_statement->session_obj);
    cassandra_statement->bound_values = rb_gc_location(cassandra_statement->bound_values);
}

void Init_statement(void)
{
    rb_undef_alloc_func(cStatement);

    rb_define_method(cStatement, "bind", statement_bind, 1);
    rb_define_method(cStatement, "page_size=", statement_page_size, 1);
    rb_define_method(cStatement, "idempotent=", statement_idempotent, 1);
}
