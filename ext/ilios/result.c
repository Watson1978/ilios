#include "ilios.h"

static void result_destroy(void *ptr);
static size_t result_memsize(const void *ptr);

const rb_data_type_t cassandra_result_data_type = {
    "Ilios::Cassandra::Result",
    {
        NULL,
        result_destroy,
        result_memsize,
#ifdef HAVE_RB_GC_MARK_MOVABLE
        NULL,
#endif
    },
    0, 0,
    RUBY_TYPED_FREE_IMMEDIATELY | RUBY_TYPED_WB_PROTECTED | RUBY_TYPED_FROZEN_SHAREABLE,
};

VALUE result_await(VALUE self)
{
    CassandraResult *cassandra_result;

    GET_RESULT(self, cassandra_result);

    nogvl_future_wait(cassandra_result->future);

    if (cass_future_error_code(cassandra_result->future) != CASS_OK) {
        char error[4096] = { 0 };

        strncpy(error, cass_error_desc(cass_future_error_code(cassandra_result->future)), sizeof(error));
        cass_future_free(cassandra_result->future);
        rb_raise(eExecutionError, "Unable to wait executing: %s", error);
    }

    if (cassandra_result->result == NULL) {
        cassandra_result->result = cass_future_get_result(cassandra_result->future);
    }

    return self;
}

VALUE result_each(VALUE self)
{
    CassandraResult *cassandra_result;
    CassIterator *iterator;
    size_t column_count;

    RETURN_ENUMERATOR(self, 0, 0);

    result_await(self);

    GET_RESULT(self, cassandra_result);

    iterator = cass_iterator_from_result(cassandra_result->result);
    column_count = cass_result_column_count(cassandra_result->result);
    while (cass_iterator_next(iterator)) {
        VALUE row_array = rb_ary_new2(column_count);
        const CassRow *row = cass_iterator_get_row(iterator);
        for (size_t i = 0; i < column_count; i++) {
            const CassValue *value = cass_row_get_column(row, i);
            const CassValueType type = cass_value_type(value);

            if (cass_value_is_null(value)) {
                rb_ary_push(row_array, Qnil);
                continue;
            }

            switch (type) {
            case CASS_VALUE_TYPE_TINY_INT:
                {
                    cass_int8_t output;
                    cass_value_get_int8(value, &output);
                    rb_ary_push(row_array, INT2NUM(output));
                }
                break;

            case CASS_VALUE_TYPE_SMALL_INT:
                {
                    cass_int16_t output;
                    cass_value_get_int16(value, &output);
                    rb_ary_push(row_array, INT2NUM(output));
                }
                break;

            case CASS_VALUE_TYPE_INT:
                {
                    cass_int32_t output;
                    cass_value_get_int32(value, &output);
                    rb_ary_push(row_array, INT2NUM(output));
                }
                break;

            case CASS_VALUE_TYPE_BIGINT:
                {
                    cass_int64_t output;
                    cass_value_get_int64(value, &output);
                    rb_ary_push(row_array, LL2NUM(output));
                }
                break;

            case CASS_VALUE_TYPE_FLOAT:
                {
                    cass_float_t output;
                    cass_value_get_float(value, &output);
                    rb_ary_push(row_array, DBL2NUM(output));
                }
                break;

            case CASS_VALUE_TYPE_DOUBLE:
                {
                    cass_double_t output;
                    cass_value_get_double(value, &output);
                    rb_ary_push(row_array, DBL2NUM(output));
                }
                break;

            case CASS_VALUE_TYPE_BOOLEAN:
                {
                    cass_bool_t output;
                    cass_value_get_bool(value, &output);
                    rb_ary_push(row_array, output == cass_true ? Qtrue : Qfalse);
                }
                break;

            case CASS_VALUE_TYPE_TEXT:
            case CASS_VALUE_TYPE_ASCII:
            case CASS_VALUE_TYPE_VARCHAR:
                {
                    const char* s;
                    size_t s_length;
                    cass_value_get_string(value, &s, &s_length);
                    rb_ary_push(row_array, rb_str_new(s, s_length));
                }
                break;

            case CASS_VALUE_TYPE_TIMESTAMP:
                {
                    cass_int64_t output;
                    cass_value_get_int64(value, &output);
                    rb_ary_push(row_array, rb_time_new(output / 1000, output % 1000 * 1000));
                }
                break;

            case CASS_VALUE_TYPE_UUID:
                {
                    CassUuid output;
                    char uuid[40];
                    cass_value_get_uuid(value, &output);
                    cass_uuid_string(output, uuid);
                    rb_ary_push(row_array, rb_str_new2(uuid));
                }
                break;

            default:
                rb_warn("Unsupported type: %d", type);
                rb_ary_push(row_array, sym_unsupported_column_type);
            }
        }

        rb_yield(row_array);
    }
    cass_iterator_free(iterator);

    return self;
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
    xfree(cassandra_result);
}

static size_t result_memsize(const void *ptr)
{
    return sizeof(CassandraResult);
}

void Init_result(void)
{
    rb_undef_alloc_func(cResult);

    rb_include_module(cResult, rb_mEnumerable);

    rb_define_method(cResult, "await", result_await, 0);
    rb_define_method(cResult, "each", result_each, 0);
}
