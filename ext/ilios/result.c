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
    RUBY_TYPED_FREE_IMMEDIATELY | RUBY_TYPED_FROZEN_SHAREABLE,
};

VALUE result_each(VALUE self)
{
    CassandraResult *cassandra_result;
    CassIterator *iterator;
    size_t column_count;

    RETURN_ENUMERATOR(self, 0, 0);

    TypedData_Get_Struct(self, CassandraResult, &cassandra_result_data_type, cassandra_result);

    iterator = cass_iterator_from_result(cassandra_result->result);
    column_count = cass_result_column_count(cassandra_result->result);
    while (cass_iterator_next(iterator)) {
        VALUE row_array = rb_ary_new();
        const CassRow *row = cass_iterator_get_row(iterator);
        for (size_t i = 0; i < column_count; i++) {
            const CassValue *value = cass_row_get_column(row, i);
            const CassValueType type = cass_value_type(value);

            switch (type) {
            case CASS_VALUE_TYPE_BIGINT:
                {
                    cass_int64_t output;
                    cass_value_get_int64(value, &output);
                    rb_ary_push(row_array, LL2NUM(output));
                }
                break;

            case CASS_VALUE_TYPE_INT:
                {
                    cass_int32_t output;
                    cass_value_get_int32(value, &output);
                    rb_ary_push(row_array, INT2NUM(output));
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
    cassandra_result->result = NULL;
    cassandra_result->future = NULL;
}

static size_t result_memsize(const void *ptr)
{
    return sizeof(CassandraResult);
}

void Init_result(void)
{
    rb_include_module(cResult, rb_mEnumerable);

    rb_define_method(cResult, "each", result_each, 0);
}
