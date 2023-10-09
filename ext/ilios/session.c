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
    VALUE config;

    GET_SESSION(self, cassandra_session);

    prepare_future = nogvl_session_prepare(cassandra_session->session, query);
    nogvl_future_wait(prepare_future);

    if (cass_future_error_code(prepare_future) != CASS_OK) {
        char error[4096] = { 0 };

        strncpy(error, cass_error_desc(cass_future_error_code(prepare_future)), sizeof(error) - 1);
        cass_future_free(prepare_future);
        rb_raise(eExecutionError, "Unable to prepare query: %s", error);
    }

    cassandra_statement_obj = CREATE_STATEMENT(cassandra_statement);

    cassandra_statement->prepared = cass_future_get_prepared(prepare_future);
    cassandra_statement->statement = cass_prepared_bind(cassandra_statement->prepared);
    cassandra_statement->session_obj = self;
    cass_future_free(prepare_future);

    config = rb_cvar_get(mCassandra, id_cvar_config);
    cass_statement_set_paging_size(cassandra_statement->statement, NUM2INT(rb_hash_aref(config, sym_page_size)));
    return cassandra_statement_obj;
}

static VALUE session_execute_async(VALUE self, VALUE statement)
{
    CassandraSession *cassandra_session;
    CassandraStatement *cassandra_statement;
    CassandraFuture *cassandra_future;
    CassFuture *result_future;
    VALUE cassandra_future_obj;

    GET_SESSION(self, cassandra_session);
    GET_STATEMENT(statement, cassandra_statement);

    result_future = nogvl_session_execute(cassandra_session->session, cassandra_statement->statement);

    cassandra_future_obj = CREATE_FUTURE(cassandra_future);
    cassandra_future->future = result_future;
    cassandra_future->statement_obj = statement;

    return cassandra_future_obj;
}

static VALUE session_execute(VALUE self, VALUE statement)
{
    CassandraSession *cassandra_session;
    CassandraStatement *cassandra_statement;
    CassandraResult *cassandra_result;
    CassFuture *result_future;
    VALUE cassandra_result_obj;

    GET_SESSION(self, cassandra_session);
    GET_STATEMENT(statement, cassandra_statement);

    result_future = nogvl_session_execute(cassandra_session->session, cassandra_statement->statement);

    cassandra_result_obj = CREATE_RESULT(cassandra_result);
    cassandra_result->future = result_future;
    cassandra_result->statement_obj = statement;

    result_await(cassandra_result_obj);
    return cassandra_result_obj;
}

static void session_destroy(void *ptr)
{
    CassandraSession *cassandra_session = (CassandraSession *)ptr;

    if (cassandra_session->session) {
        CassFuture *close_future = cass_session_close(cassandra_session->session);
        nogvl_future_wait(close_future);
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
    rb_define_method(cSession, "execute_async", session_execute_async, 1);
    rb_define_method(cSession, "execute", session_execute, 1);
}
