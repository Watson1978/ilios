#include "ilios.h"

static void result_mark(void *ptr);
static void result_destroy(void *ptr);
static size_t result_memsize(const void *ptr);
static void result_compact(void *ptr);

const rb_data_type_t cassandra_result_data_type = {
    "Ilios::Cassandra::Result",
    {
        result_mark,
        result_destroy,
        result_memsize,
        result_compact,
    },
    0, 0,
    RUBY_TYPED_FREE_IMMEDIATELY | RUBY_TYPED_WB_PROTECTED | RUBY_TYPED_FROZEN_SHAREABLE,
};

void result_await(CassandraResult *cassandra_result)
{
    nogvl_future_wait(cassandra_result->future);

    if (cass_future_error_code(cassandra_result->future) != CASS_OK) {
        char error[4096] = { 0 };

        strncpy(error, cass_error_desc(cass_future_error_code(cassandra_result->future)), sizeof(error) - 1);
        rb_raise(eExecutionError, "Unable to wait executing: %s", error);
    }

    if (cassandra_result->result == NULL) {
        cassandra_result->result = cass_future_get_result(cassandra_result->future);
    }
}

/**
 * Loads next page synchronously
 *
 * @return [Cassandra::Result, nil] returns self or +nil+ if last page.
 * @raise [Cassandra::ExecutionError] If the query is invalid or there is something wrong with the session.
 */
static VALUE result_next_page(VALUE self)
{
    CassandraResult *cassandra_result;
    CassandraStatement *cassandra_statement;
    CassandraSession *cassandra_session;
    CassFuture *result_future;
    CassError error_code;

    GET_RESULT(self, cassandra_result);

    if (cass_result_has_more_pages(cassandra_result->result) == cass_false) {
        return Qnil;
    }

    GET_STATEMENT(cassandra_result->statement_obj, cassandra_statement);
    GET_SESSION(cassandra_statement->session_obj, cassandra_session);

    // Reuse this result's own executed statement: it is not shared with other
    // executions and its previous request has already completed, so setting
    // the paging state cannot race with the driver's IO thread.
    cass_statement_set_paging_state(cassandra_result->executed_statement, cassandra_result->result);

    result_future = nogvl_session_execute(cassandra_session->session, cassandra_result->executed_statement);
    nogvl_future_wait(result_future);

    error_code = cass_future_error_code(result_future);
    if (error_code != CASS_OK) {
        cass_future_free(result_future);
        rb_raise(eExecutionError, "Unable to wait executing: %s", cass_error_desc(error_code));
    }

    cass_result_free(cassandra_result->result);
    if (cassandra_result->future) {
        cass_future_free(cassandra_result->future);
    }
    cassandra_result->result = cass_future_get_result(result_future);
    cassandra_result->future = result_future;

    return self;
}

static void result_check_value(CassError error_code, VALUE key)
{
    if (error_code != CASS_OK) {
        rb_raise(eExecutionError, "Unable to get value of %"PRIsVALUE" column: %s", key, cass_error_desc(error_code));
    }
}

static VALUE result_convert_value(const CassValue *value, VALUE key);

struct result_convert_collection_arg {
    CassIterator *iterator;
    CassValueType type;
    VALUE key;
};

static VALUE result_convert_collection_body(VALUE a)
{
    struct result_convert_collection_arg *args = (struct result_convert_collection_arg *)a;

    if (args->type == CASS_VALUE_TYPE_MAP) {
        VALUE hash = rb_hash_new();

        while (cass_iterator_next(args->iterator)) {
            VALUE k = result_convert_value(cass_iterator_get_map_key(args->iterator), args->key);
            VALUE v = result_convert_value(cass_iterator_get_map_value(args->iterator), args->key);
            rb_hash_aset(hash, k, v);
        }
        return hash;
    } else {
        VALUE array = rb_ary_new();

        while (cass_iterator_next(args->iterator)) {
            rb_ary_push(array, result_convert_value(cass_iterator_get_value(args->iterator), args->key));
        }
        if (args->type == CASS_VALUE_TYPE_SET) {
            return rb_funcall(cSet, id_new, 1, array);
        }
        return array;
    }
}

static VALUE result_convert_collection_ensure(VALUE a)
{
    cass_iterator_free((CassIterator *)a);
    return Qnil;
}

static VALUE result_convert_collection(const CassValue *value, CassValueType type, VALUE key)
{
    struct result_convert_collection_arg args;
    CassIterator *iterator;

    if (type == CASS_VALUE_TYPE_MAP) {
        iterator = cass_iterator_from_map(value);
    } else {
        iterator = cass_iterator_from_collection(value);
    }
    if (iterator == NULL) {
        // NULL collections are filtered out by cass_value_is_null in
        // result_convert_value; this is a safety net.
        return Qnil;
    }

    args.iterator = iterator;
    args.type = type;
    args.key = key;
    return rb_ensure(result_convert_collection_body, (VALUE)&args, result_convert_collection_ensure, (VALUE)iterator);
}

static VALUE result_convert_value(const CassValue *value, VALUE key)
{
    const CassValueType type = cass_value_type(value);

    if (cass_value_is_null(value)) {
        return Qnil;
    }

    switch (type) {
    case CASS_VALUE_TYPE_TINY_INT:
        {
            cass_int8_t output = 0;
            result_check_value(cass_value_get_int8(value, &output), key);
            return INT2NUM(output);
        }

    case CASS_VALUE_TYPE_SMALL_INT:
        {
            cass_int16_t output = 0;
            result_check_value(cass_value_get_int16(value, &output), key);
            return INT2NUM(output);
        }

    case CASS_VALUE_TYPE_INT:
        {
            cass_int32_t output = 0;
            result_check_value(cass_value_get_int32(value, &output), key);
            return INT2NUM(output);
        }

    case CASS_VALUE_TYPE_BIGINT:
        {
            cass_int64_t output = 0;
            result_check_value(cass_value_get_int64(value, &output), key);
            return LL2NUM(output);
        }

    case CASS_VALUE_TYPE_FLOAT:
        {
            cass_float_t output = 0;
            result_check_value(cass_value_get_float(value, &output), key);
            return DBL2NUM(output);
        }

    case CASS_VALUE_TYPE_DOUBLE:
        {
            cass_double_t output = 0;
            result_check_value(cass_value_get_double(value, &output), key);
            return DBL2NUM(output);
        }

    case CASS_VALUE_TYPE_BOOLEAN:
        {
            cass_bool_t output = cass_false;
            result_check_value(cass_value_get_bool(value, &output), key);
            return output == cass_true ? Qtrue : Qfalse;
        }

    case CASS_VALUE_TYPE_TEXT:
    case CASS_VALUE_TYPE_ASCII:
    case CASS_VALUE_TYPE_VARCHAR:
        {
            const char* s = NULL;
            size_t s_length = 0;
            result_check_value(cass_value_get_string(value, &s, &s_length), key);
            return rb_str_new(s, s_length);
        }

    case CASS_VALUE_TYPE_TIMESTAMP:
        {
            cass_int64_t output = 0;
            result_check_value(cass_value_get_int64(value, &output), key);
            return rb_time_new(output / 1000, output % 1000 * 1000);
        }

    case CASS_VALUE_TYPE_UUID:
        {
            CassUuid output = { 0, 0 };
            char uuid[40];
            result_check_value(cass_value_get_uuid(value, &output), key);
            cass_uuid_string(output, uuid);
            return rb_str_new2(uuid);
        }

    case CASS_VALUE_TYPE_LIST:
    case CASS_VALUE_TYPE_SET:
    case CASS_VALUE_TYPE_MAP:
        return result_convert_collection(value, type, key);

    default:
        rb_warn("Unsupported type: %d", type);
        return sym_unsupported_column_type;
    }
}

static VALUE result_convert_row(const CassResult *result, const CassRow *row, size_t column_count)
{
    VALUE key;
    const char *name;
    size_t name_length;
    VALUE hash = rb_hash_new();

    for (size_t i = 0; i < column_count; i++) {
        const CassValue *value = cass_row_get_column(row, i);

        cass_result_column_name(result, i, &name, &name_length);
        key = rb_enc_interned_str(name, name_length, rb_utf8_encoding());
        rb_hash_aset(hash, key, result_convert_value(value, key));
    }

    return hash;
}

struct result_each_arg {
    CassandraResult *cassandra_result;
    CassIterator *iterator;
};

static VALUE result_each_body(VALUE a)
{
    struct result_each_arg *args = (struct result_each_arg *)a;
    size_t column_count = cass_result_column_count(args->cassandra_result->result);

    while (cass_iterator_next(args->iterator)) {
        const CassRow *row = cass_iterator_get_row(args->iterator);
        rb_yield(result_convert_row(args->cassandra_result->result, row, column_count));
    }
    return Qnil;
}

static VALUE result_each_ensure(VALUE a)
{
    CassIterator *iterator = (CassIterator *)a;
    cass_iterator_free(iterator);
    return Qnil;
}

/**
 * Yield the row of result into a block.
 *
 * @return [Cassandra::Result, Enumerator] returns self or +Enumerator+ if block is not given.
 */
static VALUE result_each(VALUE self)
{
    CassandraResult *cassandra_result;
    CassIterator *iterator;
    struct result_each_arg args;

    RETURN_ENUMERATOR(self, 0, 0);

    GET_RESULT(self, cassandra_result);

    iterator = cass_iterator_from_result(cassandra_result->result);
    args.cassandra_result = cassandra_result;
    args.iterator = iterator;
    rb_ensure(result_each_body, (VALUE)&args, result_each_ensure, (VALUE)iterator);

    return self;
}

static void result_mark(void *ptr)
{
    CassandraResult *cassandra_result = (CassandraResult *)ptr;
    rb_gc_mark_movable(cassandra_result->statement_obj);
}

static void result_destroy(void *ptr)
{
    CassandraResult *cassandra_result = (CassandraResult *)ptr;

    if (cassandra_result->result) {
        cass_result_free(cassandra_result->result);
    }
    if (cassandra_result->future) {
        cass_future_free(cassandra_result->future);
    }
    if (cassandra_result->executed_statement) {
        cass_statement_free(cassandra_result->executed_statement);
    }
    xfree(cassandra_result);
}

static size_t result_memsize(const void *ptr)
{
    return sizeof(CassandraResult);
}

static void result_compact(void *ptr)
{
    CassandraResult *cassandra_result = (CassandraResult *)ptr;

    cassandra_result->statement_obj = rb_gc_location(cassandra_result->statement_obj);
}

void Init_result(void)
{
    rb_undef_alloc_func(cResult);

    rb_include_module(cResult, rb_mEnumerable);

    rb_define_method(cResult, "next_page", result_next_page, 0);
    rb_define_method(cResult, "each", result_each, 0);
}
