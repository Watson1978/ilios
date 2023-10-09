#include "ilios.h"

#define THREAD_MAX 4

uv_sem_t sem_thread;

static void future_mark(void *ptr);
static void future_destroy(void *ptr);
static size_t future_memsize(const void *ptr);

const rb_data_type_t cassandra_future_data_type = {
    "Ilios::Cassandra::Future",
    {
        future_mark,
        future_destroy,
        future_memsize,
#ifdef HAVE_RB_GC_MARK_MOVABLE
        NULL,
#endif
    },
    0, 0,
    RUBY_TYPED_FREE_IMMEDIATELY | RUBY_TYPED_WB_PROTECTED | RUBY_TYPED_FROZEN_SHAREABLE,
};

static void future_result_success_yield(CassandraFuture *cassandra_future)
{
    if (cassandra_future->on_success_block) {
        if (rb_proc_arity(cassandra_future->on_success_block)) {
            CassandraResult *cassandra_result;
            VALUE cassandra_result_obj;

            cassandra_result_obj = CREATE_RESULT(cassandra_result);
            cassandra_result->result = cass_future_get_result(cassandra_future->future);
            cassandra_result->statement_obj = cassandra_future->statement_obj;

            rb_proc_call_with_block(cassandra_future->on_success_block, 1, &cassandra_result_obj, Qnil);
        } else {
            rb_proc_call_with_block(cassandra_future->on_success_block, 0, NULL, Qnil);
        }
    }
}

static void future_result_failure_yield(CassandraFuture *cassandra_future)
{
    if (cassandra_future->on_failure_block) {
        rb_proc_call_with_block(cassandra_future->on_failure_block, 0, NULL, Qnil);
    }
}

VALUE future_result_yielder(void *arg)
{
    CassandraFuture *cassandra_future = (CassandraFuture *)arg;

    if (cass_future_error_code(cassandra_future->future) == CASS_OK) {
        future_result_success_yield(cassandra_future);
    } else {
        future_result_failure_yield(cassandra_future);
    }

    uv_sem_post(&sem_thread);
    return Qnil;
}

static VALUE future_on_success(VALUE self)
{
    CassandraFuture *cassandra_future;

    GET_FUTURE(self, cassandra_future);

    if (rb_block_given_p()) {
        cassandra_future->on_success_block = rb_block_proc();
        if (cass_future_ready(cassandra_future->future)) {
            if (cass_future_error_code(cassandra_future->future) == CASS_OK) {
                future_result_success_yield(cassandra_future);
            }
        } else {
            if (!cassandra_future->thread_obj) {
                uv_sem_wait(&sem_thread);
                cassandra_future->thread_obj = rb_thread_create(future_result_yielder, (void*)cassandra_future);
            }
        }
    }
    return self;
}

static VALUE future_on_failure(VALUE self)
{
    CassandraFuture *cassandra_future;

    GET_FUTURE(self, cassandra_future);

    if (rb_block_given_p()) {
        cassandra_future->on_failure_block = rb_block_proc();
        if (cass_future_ready(cassandra_future->future)) {
            if (cass_future_error_code(cassandra_future->future) != CASS_OK) {
                future_result_failure_yield(cassandra_future);
            }
        } else {
            if (!cassandra_future->thread_obj) {
                uv_sem_wait(&sem_thread);
                cassandra_future->thread_obj = rb_thread_create(future_result_yielder, (void*)cassandra_future);
            }
        }
    }
    return self;
}

static void future_mark(void *ptr)
{
    CassandraFuture *cassandra_future = (CassandraFuture *)ptr;
    rb_gc_mark(cassandra_future->statement_obj);
    rb_gc_mark(cassandra_future->thread_obj);
    rb_gc_mark(cassandra_future->on_success_block);
    rb_gc_mark(cassandra_future->on_failure_block);
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

void Init_future(void)
{
    rb_undef_alloc_func(cFuture);

    rb_define_method(cFuture, "on_success", future_on_success, 0);
    rb_define_method(cFuture, "on_failure", future_on_failure, 0);

    uv_sem_init(&sem_thread, THREAD_MAX);
}
