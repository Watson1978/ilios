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

void statement_default_config(CassandraStatement *cassandra_statement)
{
    cassandra_statement->bound_values = Qnil;
    cassandra_statement->page_size = DEFAULT_PAGE_SIZE;
    cassandra_statement->idempotent = idempotency_unset;
    cass_statement_set_paging_size(cassandra_statement->statement, DEFAULT_PAGE_SIZE);
}

static int hash_cb(VALUE key, VALUE value, VALUE arg)
{
    statement_bind_context *ctx = (statement_bind_context *)arg;
    const CassDataType* data_type;
    CassValueType value_type;
    CassError result;
    const char *name;

    if (SYMBOL_P(key)) {
        key = rb_sym2str(key);
    }
    name = StringValueCStr(key);

    data_type = cass_prepared_parameter_data_type_by_name(ctx->prepared, name);
    if (data_type == NULL) {
        rb_raise(eStatementError, "Invalid name %s was given.", name);
    }
    value_type = cass_data_type_type(data_type);

    if (NIL_P(value)) {
        result = cass_statement_bind_null_by_name(ctx->statement, name);
        goto result_check;
    }

    switch (value_type) {
    case CASS_VALUE_TYPE_TINY_INT:
        {
            long v = NUM2LONG(value);

            if (v < INT8_MIN || v > INT8_MAX) {
                rb_raise(rb_eRangeError, "Invalid value: %ld", v);
            }
            result = cass_statement_bind_int8_by_name(ctx->statement, name, (cass_int8_t)v);
        }
        break;

    case CASS_VALUE_TYPE_SMALL_INT:
        {
            long v = NUM2LONG(value);

            if (v < INT16_MIN || v > INT16_MAX) {
                rb_raise(rb_eRangeError, "Invalid value: %ld", v);
            }

            result = cass_statement_bind_int16_by_name(ctx->statement, name, (cass_int16_t)v);
        }
        break;

    case CASS_VALUE_TYPE_INT:
        {
            long v = NUM2LONG(value);

            if (v < INT32_MIN || v > INT32_MAX) {
                rb_raise(rb_eRangeError, "Invalid value: %ld", v);
            }

            result = cass_statement_bind_int32_by_name(ctx->statement, name, (cass_int32_t)v);
        }
        break;

    case CASS_VALUE_TYPE_BIGINT:
        result = cass_statement_bind_int64_by_name(ctx->statement, name, NUM2LONG(value));
        break;

    case CASS_VALUE_TYPE_FLOAT:
        {
            double v = NUM2DBL(value);

            if (!isnan(v) && !isinf(v) && (v < -FLT_MAX || v > FLT_MAX)) {
                rb_raise(rb_eRangeError, "Invalid value: %lf", v);
            }

            result = cass_statement_bind_float_by_name(ctx->statement, name, v);
        }
        break;

    case CASS_VALUE_TYPE_DOUBLE:
        result = cass_statement_bind_double_by_name(ctx->statement, name, NUM2DBL(value));
        break;

    case CASS_VALUE_TYPE_BOOLEAN:
        {
            cass_bool_t v = RTEST(value) ? cass_true : cass_false;
            result = cass_statement_bind_bool_by_name(ctx->statement, name, v);
        }
        break;

    case CASS_VALUE_TYPE_TEXT:
    case CASS_VALUE_TYPE_ASCII:
    case CASS_VALUE_TYPE_VARCHAR:
        result = cass_statement_bind_string_by_name(ctx->statement, name, StringValueCStr(value));
        break;

    case CASS_VALUE_TYPE_TIMESTAMP:
        if (rb_obj_class(value) != rb_cTime) {
            if (rb_respond_to(value, id_to_time)) {
                value = rb_funcall(value, id_to_time, 0);
            } else {
                rb_raise(rb_eTypeError, "no implicit conversion of %"PRIsVALUE" to Time", rb_obj_class(value));
            }
        }
        result = cass_statement_bind_int64_by_name(ctx->statement, name, (cass_int64_t)(NUM2DBL(rb_Float(value)) * 1000));
        break;

    case CASS_VALUE_TYPE_UUID:
        {
            CassUuid uuid = { 0, 0 };
            const char *uuid_string = StringValueCStr(value);

            result = cass_uuid_from_string(uuid_string, &uuid);
            if (result != CASS_OK) {
                rb_raise(eStatementError, "Invalid UUID was given: %s=%"PRIsVALUE"", name, value);
            }

            result = cass_statement_bind_uuid_by_name(ctx->statement, name, uuid);
        }
        break;

    default:
        rb_raise(rb_eTypeError, "Unsupported %"PRIsVALUE" type: %s=%"PRIsVALUE"", rb_obj_class(value), name, value);
    }

result_check:
    if (result != CASS_OK) {
        rb_raise(eStatementError, "Failed to bind value: %s", cass_error_desc(result));
    }

    if (!NIL_P(ctx->bound_values)) {
        if (RB_TYPE_P(value, T_STRING)) {
            // Snapshot the value so a later in-place mutation by the caller
            // doesn't change what gets bound at execution time.
            value = rb_str_new_frozen(value);
        }
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
