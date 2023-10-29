#include "ilios.h"

static void future_mark(void *ptr);
static void future_destroy(void *ptr);
static size_t future_memsize(const void *ptr);
static void future_compact(void *ptr);

const rb_data_type_t cassandra_future_data_type = {
    "Ilios::Cassandra::Future",
    {
        future_mark,
        future_destroy,
        future_memsize,
        future_compact,
    },
    0, 0,
    RUBY_TYPED_FREE_IMMEDIATELY | RUBY_TYPED_WB_PROTECTED | RUBY_TYPED_FROZEN_SHAREABLE,
};

static VALUE future_result_success_yield(RB_BLOCK_CALL_FUNC_ARGLIST(_, future))
{
    CassandraFuture *cassandra_future;
    VALUE obj = Qnil;

    GET_FUTURE(future, cassandra_future);

    nogvl_future_wait(cassandra_future->future);

    if (cass_future_error_code(cassandra_future->future) == CASS_OK) {
        VALUE on_success_block = rb_block_proc();

        if (rb_proc_arity(on_success_block)) {
            switch (cassandra_future->kind) {
            case prepare_async:
                {
                    CassandraStatement *cassandra_statement;
                    VALUE cassandra_statement_obj;

                    cassandra_statement_obj = CREATE_STATEMENT(cassandra_statement);
                    cassandra_statement->prepared = cass_future_get_prepared(cassandra_future->future);
                    cassandra_statement->statement = cass_prepared_bind(cassandra_statement->prepared);
                    cassandra_statement->session_obj = cassandra_future->session_obj;

                    statement_default_config(cassandra_statement);

                    obj = cassandra_statement_obj;
                }
                break;
            case execute_async:
                {
                    CassandraResult *cassandra_result;
                    VALUE cassandra_result_obj;

                    cassandra_result_obj = CREATE_RESULT(cassandra_result);
                    cassandra_result->result = cass_future_get_result(cassandra_future->future);
                    cassandra_result->statement_obj = cassandra_future->statement_obj;

                    obj = cassandra_result_obj;
                }
                break;
            }
        }
    }
    rb_fiber_yield(1, &obj);
    return Qnil;
}

static VALUE future_result_failure_yield(RB_BLOCK_CALL_FUNC_ARGLIST(_, future))
{
    CassandraFuture *cassandra_future;
    VALUE obj = Qnil;

    GET_FUTURE(future, cassandra_future);

    nogvl_future_wait(cassandra_future->future);
    if (cass_future_error_code(cassandra_future->future) != CASS_OK) {
        obj = Qtrue;
    }
    rb_fiber_yield(1, &obj);
    return Qnil;
}

static VALUE future_on_success(VALUE self)
{
    if (rb_block_given_p()) {
        VALUE fiber = rb_fiber_new(future_result_success_yield, self);
        VALUE result = rb_fiber_resume(fiber, 0, NULL);
        rb_yield(result);
    }
    return self;
}

static VALUE future_on_failure(VALUE self)
{
    if (rb_block_given_p()) {
        VALUE fiber = rb_fiber_new(future_result_failure_yield, self);
        if (RTEST(rb_fiber_resume(fiber, 0, NULL))) {
            rb_yield(Qnil);
        }
    }
    return self;
}

static void future_mark(void *ptr)
{
    CassandraFuture *cassandra_future = (CassandraFuture *)ptr;
    rb_gc_mark_movable(cassandra_future->session_obj);
    rb_gc_mark_movable(cassandra_future->statement_obj);
}

static void future_destroy(void *ptr)
{
    CassandraFuture *cassandra_future = (CassandraFuture *)ptr;

    if (cassandra_future->future) {
        cass_future_free(cassandra_future->future);
    }
    xfree(cassandra_future);
}

static size_t future_memsize(const void *ptr)
{
    return sizeof(CassandraFuture);
}

static void future_compact(void *ptr)
{
    CassandraFuture *cassandra_future = (CassandraFuture *)ptr;

    cassandra_future->session_obj = rb_gc_location(cassandra_future->session_obj);
    cassandra_future->statement_obj = rb_gc_location(cassandra_future->statement_obj);
}

void Init_future(void)
{
    rb_undef_alloc_func(cFuture);

    rb_define_method(cFuture, "on_success", future_on_success, 0);
    rb_define_method(cFuture, "on_failure", future_on_failure, 0);
}
