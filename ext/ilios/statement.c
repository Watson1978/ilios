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

void statement_default_config(CassandraStatement *cassandra_statement)
{
    cass_statement_set_paging_size(cassandra_statement->statement, DEFAULT_PAGE_SIZE);
}

static int hash_cb(VALUE key, VALUE value, VALUE statement)
{
    CassandraStatement *cassandra_statement = (CassandraStatement *)statement;
    const CassDataType* data_type;
    CassValueType value_type;
    CassError result;
    const char *name;

    if (SYMBOL_P(key)) {
        key = rb_sym2str(key);
    }
    name = StringValueCStr(key);

    data_type = cass_prepared_parameter_data_type_by_name(cassandra_statement->prepared, name);
    if (data_type == NULL) {
        rb_raise(eStatementError, "Invalid name %s was given.", name);
    }
    value_type = cass_data_type_type(data_type);

    if (NIL_P(value)) {
        result = cass_statement_bind_null_by_name(cassandra_statement->statement, name);
        goto result_check;
    }

    switch (value_type) {
    case CASS_VALUE_TYPE_TINY_INT:
        {
            long v = NUM2LONG(value);

            if (v < INT8_MIN || v > INT8_MAX) {
                rb_raise(rb_eRangeError, "Invalid value: %ld", v);
            }
            result = cass_statement_bind_int8_by_name(cassandra_statement->statement, name, (cass_int8_t)v);
        }
        break;

    case CASS_VALUE_TYPE_SMALL_INT:
        {
            long v = NUM2LONG(value);

            if (v < INT16_MIN || v > INT16_MAX) {
                rb_raise(rb_eRangeError, "Invalid value: %ld", v);
            }

            result = cass_statement_bind_int16_by_name(cassandra_statement->statement, name, (cass_int16_t)v);
        }
        break;

    case CASS_VALUE_TYPE_INT:
        {
            long v = NUM2LONG(value);

            if (v < INT32_MIN || v > INT32_MAX) {
                rb_raise(rb_eRangeError, "Invalid value: %ld", v);
            }

            result = cass_statement_bind_int32_by_name(cassandra_statement->statement, name, (cass_int32_t)v);
        }
        break;

    case CASS_VALUE_TYPE_BIGINT:
        result = cass_statement_bind_int64_by_name(cassandra_statement->statement, name, NUM2LONG(value));
        break;

    case CASS_VALUE_TYPE_FLOAT:
        {
            double v = NUM2DBL(value);

            if (!isnan(v) && !isinf(v) && (v < -FLT_MAX || v > FLT_MAX)) {
                rb_raise(rb_eRangeError, "Invalid value: %lf", v);
            }

            result = cass_statement_bind_float_by_name(cassandra_statement->statement, name, v);
        }
        break;

    case CASS_VALUE_TYPE_DOUBLE:
        result = cass_statement_bind_double_by_name(cassandra_statement->statement, name, NUM2DBL(value));
        break;

    case CASS_VALUE_TYPE_BOOLEAN:
        {
            cass_bool_t v = RTEST(value) ? cass_true : cass_false;
            result = cass_statement_bind_bool_by_name(cassandra_statement->statement, name, v);
        }
        break;

    case CASS_VALUE_TYPE_TEXT:
    case CASS_VALUE_TYPE_ASCII:
    case CASS_VALUE_TYPE_VARCHAR:
        result = cass_statement_bind_string_by_name(cassandra_statement->statement, name, StringValueCStr(value));
        break;

    case CASS_VALUE_TYPE_TIMESTAMP:
        if (rb_obj_class(value) != rb_cTime) {
            if (rb_respond_to(value, id_to_time)) {
                value = rb_funcall(value, id_to_time, 0);
            } else {
                rb_raise(rb_eTypeError, "no implicit conversion of %"PRIsVALUE" to Time", rb_obj_class(value));
            }
        }
        result = cass_statement_bind_int64_by_name(cassandra_statement->statement, name, (cass_int64_t)(NUM2DBL(rb_Float(value)) * 1000));
        break;

    case CASS_VALUE_TYPE_UUID:
        {
            CassUuid uuid;
            const char *uuid_string = StringValueCStr(value);

            cass_uuid_from_string(uuid_string, &uuid);
            result = cass_statement_bind_uuid_by_name(cassandra_statement->statement, name, uuid);
        }
        break;

    default:
        rb_raise(rb_eTypeError, "Unsupported %"PRIsVALUE" type: %s=%"PRIsVALUE"", rb_obj_class(value), name, value);
    }

result_check:
    if (result != CASS_OK) {
        rb_raise(eStatementError, "Failed to bind value: %s", cass_error_desc(result));
    }

    return ST_CONTINUE;
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

    Check_Type(hash, T_HASH);
    TypedData_Get_Struct(self, CassandraStatement, &cassandra_statement_data_type, cassandra_statement);

    rb_hash_foreach(hash, hash_cb, (VALUE)cassandra_statement);
    return self;
}

/**
 * Sets the statement's page size.
 *
 * @param page_size [Integer] A page size.
 * @return [Cassandra::Statement] self.
 */
static VALUE statement_page_size(VALUE self, VALUE page_size)
{
    CassandraStatement *cassandra_statement;

    GET_STATEMENT(self, cassandra_statement);
    cass_statement_set_paging_size(cassandra_statement->statement, NUM2INT(page_size));
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
    cass_statement_set_is_idempotent(cassandra_statement->statement, RTEST(idempotent) ? cass_true : cass_false);
    return self;
}

static void statement_mark(void *ptr)
{
    CassandraStatement *cassandra_statement = (CassandraStatement *)ptr;
    rb_gc_mark_movable(cassandra_statement->session_obj);
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
}

void Init_statement(void)
{
    rb_undef_alloc_func(cStatement);

    rb_define_method(cStatement, "bind", statement_bind, 1);
    rb_define_method(cStatement, "page_size=", statement_page_size, 1);
    rb_define_method(cStatement, "idempotent=", statement_idempotent, 1);
}
