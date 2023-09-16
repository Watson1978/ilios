#include "ilios.h"

static void statement_destroy(void *ptr);
static size_t statement_memsize(const void *ptr);

const rb_data_type_t cassandra_statement_data_type = {
    "Ilios::Cassandra::Statement",
    {
        NULL,
        statement_destroy,
        statement_memsize,
#ifdef HAVE_RB_GC_MARK_MOVABLE
        NULL,
#endif
    },
    0, 0,
    RUBY_TYPED_FREE_IMMEDIATELY | RUBY_TYPED_WB_PROTECTED | RUBY_TYPED_FROZEN_SHAREABLE,
};

static int hash_cb(VALUE key, VALUE value, VALUE statement)
{
    CassandraStatement *cassandra_statement = (CassandraStatement *)statement;
    const char *name;

    if (SYMBOL_P(key)) {
        key = rb_sym2str(key);
    }
    name = StringValueCStr(key);

    switch (rb_type(value)) {
    case T_NIL:
        cass_statement_bind_null_by_name(cassandra_statement->statement, name);
        break;

    case T_TRUE:
        cass_statement_bind_bool_by_name(cassandra_statement->statement, name, cass_true);

    case T_FALSE:
        cass_statement_bind_bool_by_name(cassandra_statement->statement, name, cass_false);

    case T_FIXNUM:
        cass_statement_bind_int64_by_name(cassandra_statement->statement, name, NUM2LONG(value));
        break;

    case T_FLOAT:
        cass_statement_bind_double_by_name(cassandra_statement->statement, name, NUM2DBL(value));
        break;

    case T_STRING:
        cass_statement_bind_string_by_name(cassandra_statement->statement, name, StringValueCStr(value));
        break;

    case T_SYMBOL:
        value = rb_sym2str(value);
        cass_statement_bind_string_by_name(cassandra_statement->statement, name, StringValueCStr(value));
        break;

    default:
        rb_raise(rb_eTypeError, "Unsupported %"PRIsVALUE" type: %s=%"PRIsVALUE"", rb_obj_class(value), name, value);
    }

    return ST_CONTINUE;
}

static VALUE statement_bind(VALUE self, VALUE hash)
{
    CassandraStatement *cassandra_statement;

    Check_Type(hash, T_HASH);
    TypedData_Get_Struct(self, CassandraStatement, &cassandra_statement_data_type, cassandra_statement);

    rb_hash_foreach(hash, hash_cb, (VALUE)cassandra_statement);
    return self;
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

void Init_statement(void)
{
    rb_undef_alloc_func(cStatement);

    rb_define_method(cStatement, "bind", statement_bind, 1);
}
