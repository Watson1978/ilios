#include "ilios.h"

static void session_mark(void *ptr);
static void session_destroy(void *ptr);
static size_t session_memsize(const void *ptr);
static void session_compact(void *ptr);

const rb_data_type_t cassandra_session_data_type = {
    "Ilios::Cassandra::Session",
    {
        session_mark,
        session_destroy,
        session_memsize,
        session_compact,
    },
    0, 0,
    RUBY_TYPED_FREE_IMMEDIATELY | RUBY_TYPED_WB_PROTECTED | RUBY_TYPED_FROZEN_SHAREABLE,
};

/**
 * Prepares a given query asynchronously and returns a future prepared statement.
 *
 * @param query [String] A query to prepare.
 * @return [Cassandra::Future] A future prepared statement.
 * @raise [TypeError] If the query is not a string.
 */
static VALUE session_prepare_async(VALUE self, VALUE query)
{
    CassandraSession *cassandra_session;
    CassFuture *prepare_future;

    GET_SESSION(self, cassandra_session);

    prepare_future = nogvl_session_prepare(cassandra_session->session, query);
    return future_create(prepare_future, self, prepare_async);
}

/**
 * Prepares a given query.
 *
 * @param query [String] A query to prepare.
 * @return [Cassandra::Statement] A statement to execute.
 * @raise [Cassandra::ExecutionError] If the query is invalid or there is something wrong with the session.
 * @raise [TypeError] If the query is not a string.
 */
static VALUE session_prepare(VALUE self, VALUE query)
{
    CassandraSession *cassandra_session;
    CassandraStatement *cassandra_statement;
    CassFuture *prepare_future;
    VALUE cassandra_statement_obj;

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

    statement_default_config(cassandra_statement);
    return cassandra_statement_obj;
}

/**
 * Executes a given statement asynchronously and returns a future result.
 *
 * @param statement [Cassandra::Statement] A statement to execute.
 * @return [Cassandra::Future] A future for result.
 * @raise [TypeError] If the invalid object is given.
 */
static VALUE session_execute_async(VALUE self, VALUE statement)
{
    CassandraSession *cassandra_session;
    CassandraStatement *cassandra_statement;
    CassFuture *result_future;

    GET_SESSION(self, cassandra_session);
    GET_STATEMENT(statement, cassandra_statement);

    result_future = nogvl_session_execute(cassandra_session->session, cassandra_statement->statement);
    return future_create(result_future, self, execute_async);
}

/**
 * Executes a given statement.
 *
 * @param statement [Cassandra::Statement] A statement to execute.
 * @return [Cassandra::Result] A result.
 * @raise [Cassandra::ExecutionError] If there is something wrong with the session.
 * @raise [TypeError] If the invalid object is given.
 */
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

    result_await(cassandra_result);
    return cassandra_result_obj;
}

static void session_mark(void *ptr)
{
    CassandraSession *cassandra_session = (CassandraSession *)ptr;
    rb_gc_mark_movable(cassandra_session->cluster_obj);
}

static void session_destroy(void *ptr)
{
    CassandraSession *cassandra_session = (CassandraSession *)ptr;

    if (cassandra_session->session) {
        cass_session_free(cassandra_session->session);
    }
    xfree(cassandra_session);
}

static size_t session_memsize(const void *ptr)
{
    return sizeof(CassandraSession);
}

static void session_compact(void *ptr)
{
    CassandraSession *cassandra_session = (CassandraSession *)ptr;

    cassandra_session->cluster_obj = rb_gc_location(cassandra_session->cluster_obj);
}

void Init_session(void)
{
    rb_undef_alloc_func(cSession);

    rb_define_method(cSession, "prepare_async", session_prepare_async, 1);
    rb_define_method(cSession, "prepare", session_prepare, 1);
    rb_define_method(cSession, "execute_async", session_execute_async, 1);
    rb_define_method(cSession, "execute", session_execute, 1);
}
