#include "ilios.h"

static void session_destroy(void *ptr);
static size_t session_memsize(const void *ptr);

const rb_data_type_t cassandra_session_data_type = {
    "Ilios::Cassandra::Session",
    {
        NULL,
        session_destroy,
        session_memsize,
#ifdef HAVE_RB_GC_MARK_MOVABLE
        NULL,
#endif
    },
    0, 0,
    RUBY_TYPED_FREE_IMMEDIATELY | RUBY_TYPED_WB_PROTECTED | RUBY_TYPED_FROZEN_SHAREABLE,
};

static VALUE session_prepare(VALUE self, VALUE query)
{
    CassandraSession *cassandra_session;
    CassandraStatement *cassandra_statement;
    CassFuture *prepare_future;
    VALUE cassandra_statement_obj;

    TypedData_Get_Struct(self, CassandraSession, &cassandra_session_data_type, cassandra_session);

    prepare_future = nogvl_session_prepare(cassandra_session->session, query);
    nogvl_future_wait(prepare_future);

    if (cass_future_error_code(prepare_future) != CASS_OK) {
        char error[4096] = { 0 };

        strncpy(error, cass_error_desc(cass_future_error_code(prepare_future)), sizeof(error));
        cass_future_free(prepare_future);
        rb_raise(eExecutionError, "Unable to prepare query: %s", error);
    }

    cassandra_statement_obj = TypedData_Make_Struct(cStatement, CassandraStatement, &cassandra_statement_data_type, cassandra_statement);

    cassandra_statement->prepared = cass_future_get_prepared(prepare_future);
    cassandra_statement->statement = cass_prepared_bind(cassandra_statement->prepared);
    cassandra_statement->session_obj = self;
    cass_future_free(prepare_future);

    return cassandra_statement_obj;
}

static VALUE session_execute(VALUE self, VALUE statement)
{
    CassandraSession *cassandra_session;
    CassandraStatement *cassandra_statement;
    CassandraResult *cassandra_result;
    CassFuture *result_future;
    VALUE cassandra_result_obj;

    TypedData_Get_Struct(self, CassandraSession, &cassandra_session_data_type, cassandra_session);
    TypedData_Get_Struct(statement, CassandraStatement, &cassandra_statement_data_type, cassandra_statement);

    result_future = nogvl_session_execute(cassandra_session->session, cassandra_statement->statement);
    nogvl_future_wait(result_future);

    if (cass_future_error_code(result_future) != CASS_OK) {
        char error[4096] = { 0 };

        strncpy(error, cass_error_desc(cass_future_error_code(result_future)), sizeof(error));
        cass_future_free(result_future);
        rb_raise(eExecutionError, "Unable to execute: %s", error);
    }

    cassandra_result_obj = TypedData_Make_Struct(cResult, CassandraResult, &cassandra_result_data_type, cassandra_result);
    cassandra_result->result = cass_future_get_result(result_future);
    cassandra_result->future = result_future;

    return cassandra_result_obj;
}

static void session_destroy(void *ptr)
{
    CassandraSession *cassandra_session = (CassandraSession *)ptr;

    if (cassandra_session->session) {
        CassFuture *close_future = cass_session_close(cassandra_session->session);
        cass_future_wait(close_future);
        cass_future_free(close_future);
        cass_session_free(cassandra_session->session);
    }
    if (cassandra_session->connect_future) {
        cass_future_free(cassandra_session->connect_future);
    }
    if (cassandra_session->cluster) {
        cass_cluster_free(cassandra_session->cluster);
    }
    xfree(cassandra_session);
}

static size_t session_memsize(const void *ptr)
{
    return sizeof(CassandraSession);
}

void Init_session(void)
{
    rb_undef_alloc_func(cSession);

    rb_define_method(cSession, "prepare", session_prepare, 1);
    rb_define_method(cSession, "execute", session_execute, 1);
}
