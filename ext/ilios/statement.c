#include "ilios.h"

static void statement_mark(void *ptr);
static void statement_destroy(void *ptr);
static size_t statement_memsize(const void *ptr);

const rb_data_type_t cassandra_statement_data_type = {
    "Ilios::Cassandra::Statement",
    {
        statement_mark,
        statement_destroy,
        statement_memsize,
#ifdef HAVE_RB_GC_MARK_MOVABLE
        NULL,
#endif
    },
    0, 0,
    RUBY_TYPED_FREE_IMMEDIATELY | RUBY_TYPED_WB_PROTECTED | RUBY_TYPED_FROZEN_SHAREABLE,
};

static VALUE statement_bind_tinyint(VALUE self, VALUE idx, VALUE value)
{
    CassandraStatement *cassandra_statement;

    TypedData_Get_Struct(self, CassandraStatement, &cassandra_statement_data_type, cassandra_statement);
    cass_statement_bind_int8(cassandra_statement->statement, NUM2LONG(idx), NUM2INT(value));
    return self;
}

static VALUE statement_bind_smallint(VALUE self, VALUE idx, VALUE value)
{
    CassandraStatement *cassandra_statement;

    TypedData_Get_Struct(self, CassandraStatement, &cassandra_statement_data_type, cassandra_statement);
    cass_statement_bind_int16(cassandra_statement->statement, NUM2LONG(idx), NUM2INT(value));
    return self;
}

static VALUE statement_bind_int(VALUE self, VALUE idx, VALUE value)
{
    CassandraStatement *cassandra_statement;

    TypedData_Get_Struct(self, CassandraStatement, &cassandra_statement_data_type, cassandra_statement);
    cass_statement_bind_int32(cassandra_statement->statement, NUM2LONG(idx), NUM2INT(value));
    return self;
}

static VALUE statement_bind_bigint(VALUE self, VALUE idx, VALUE value)
{
    CassandraStatement *cassandra_statement;

    TypedData_Get_Struct(self, CassandraStatement, &cassandra_statement_data_type, cassandra_statement);
    cass_statement_bind_int64(cassandra_statement->statement, NUM2LONG(idx), NUM2LONG(value));
    return self;
}

static VALUE statement_bind_float(VALUE self, VALUE idx, VALUE value)
{
    CassandraStatement *cassandra_statement;

    TypedData_Get_Struct(self, CassandraStatement, &cassandra_statement_data_type, cassandra_statement);
    cass_statement_bind_float(cassandra_statement->statement, NUM2LONG(idx), NUM2DBL(value));
    return self;
}

static VALUE statement_bind_double(VALUE self, VALUE idx, VALUE value)
{
    CassandraStatement *cassandra_statement;

    TypedData_Get_Struct(self, CassandraStatement, &cassandra_statement_data_type, cassandra_statement);
    cass_statement_bind_double(cassandra_statement->statement, NUM2LONG(idx), NUM2DBL(value));
    return self;
}

static VALUE statement_bind_boolean(VALUE self, VALUE idx, VALUE value)
{
    CassandraStatement *cassandra_statement;
    cass_bool_t v = RTEST(value) ? cass_true : cass_false;

    TypedData_Get_Struct(self, CassandraStatement, &cassandra_statement_data_type, cassandra_statement);
    cass_statement_bind_bool(cassandra_statement->statement, NUM2LONG(idx), v);
    return self;
}

static VALUE statement_bind_text(VALUE self, VALUE idx, VALUE value)
{
    CassandraStatement *cassandra_statement;

    TypedData_Get_Struct(self, CassandraStatement, &cassandra_statement_data_type, cassandra_statement);
    cass_statement_bind_string(cassandra_statement->statement, NUM2LONG(idx), StringValueCStr(value));
    return self;
}

static VALUE statement_bind_timestamp(VALUE self, VALUE idx, VALUE value)
{
    CassandraStatement *cassandra_statement;

    TypedData_Get_Struct(self, CassandraStatement, &cassandra_statement_data_type, cassandra_statement);
    cass_statement_bind_int64(cassandra_statement->statement, NUM2LONG(idx), (cass_int64_t)(NUM2DBL(rb_Float(value)) * 1000));
    return self;
}

static VALUE statement_bind_uuid(VALUE self, VALUE idx, VALUE value)
{
    CassandraStatement *cassandra_statement;
    CassUuid uuid;
    const char *uuid_string = StringValueCStr(value);

    TypedData_Get_Struct(self, CassandraStatement, &cassandra_statement_data_type, cassandra_statement);
    cass_uuid_from_string(uuid_string, &uuid);
    cass_statement_bind_uuid(cassandra_statement->statement, NUM2LONG(idx), uuid);
    return self;
}

static void statement_mark(void *ptr)
{
    CassandraStatement *cassandra_statement = (CassandraStatement *)ptr;
    rb_gc_mark(cassandra_statement->session_obj);
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

    rb_define_method(cStatement, "bind_tinyint", statement_bind_tinyint, 2);
    rb_define_method(cStatement, "bind_smallint", statement_bind_smallint, 2);
    rb_define_method(cStatement, "bind_int", statement_bind_int, 2);
    rb_define_method(cStatement, "bind_bigint", statement_bind_bigint, 2);
    rb_define_method(cStatement, "bind_float", statement_bind_float, 2);
    rb_define_method(cStatement, "bind_double", statement_bind_double, 2);
    rb_define_method(cStatement, "bind_boolean", statement_bind_boolean, 2);
    rb_define_method(cStatement, "bind_text", statement_bind_text, 2);
    rb_define_method(cStatement, "bind_ascii", statement_bind_text, 2);
    rb_define_method(cStatement, "bind_varchar", statement_bind_text, 2);
    rb_define_method(cStatement, "bind_timestamp", statement_bind_timestamp, 2);
    rb_define_method(cStatement, "bind_uuid", statement_bind_uuid, 2);
}
