#include "ilios.h"

#define THREAD_MAX 5

typedef struct
{
    uv_cond_t cond;
    uv_mutex_t mutex;
} future_thread_pool_condition;

typedef struct
{
    future_thread_pool_condition condition[THREAD_MAX];
    VALUE queue;
    uv_mutex_t queue_mutex;
    unsigned int current_number;
} future_thread_pool;

static future_thread_pool thread_prepare;
static future_thread_pool thread_execute;

typedef struct
{
    future_thread_pool *thread_pool;
    unsigned int thread_number;
} future_thread_arg;

static void future_mark(void *ptr);
static void future_destroy(void *ptr);
static size_t future_memsize(const void *ptr);
static VALUE future_result_yielder_thread(void *arg);

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


static future_thread_pool *thread_for_future(CassandraFuture *cassandra_future)
{
    switch (cassandra_future->kind) {
    case prepare_async:
        return &thread_prepare;
    case execute_async:
        return &thread_execute;
    }
}

static void future_thread_init(future_thread_pool *thread_pool)
{
    thread_pool->queue = rb_ary_new();
    uv_mutex_init(&thread_pool->queue_mutex);
    thread_pool->current_number = 0;

    for (unsigned int i = 0; i < THREAD_MAX; i++) {
        future_thread_arg *arg = malloc(sizeof(future_thread_arg));
        arg->thread_pool = thread_pool;
        arg->thread_number = i;

        uv_cond_init(&thread_pool->condition[i].cond);
        uv_mutex_init(&thread_pool->condition[i].mutex);
        rb_thread_create(future_result_yielder_thread, (void*)arg);
    }
}

static void future_thread_signal(future_thread_pool *thread_pool)
{
    nogvl_mutex_lock(&thread_pool->queue_mutex);
    uv_cond_signal(&thread_pool->condition[thread_pool->current_number].cond);
    thread_pool->current_number = (thread_pool->current_number + 1) % THREAD_MAX;
    uv_mutex_unlock(&thread_pool->queue_mutex);
}

static void future_thread_wait(future_thread_pool *thread_pool, unsigned int number)
{
    nogvl_cond_wait(&thread_pool->condition[number].cond, &thread_pool->condition[number].mutex);
}

static void future_queue_push(future_thread_pool *thread_pool, VALUE future)
{
    nogvl_mutex_lock(&thread_pool->queue_mutex);
    rb_ary_push(thread_pool->queue, future);
    uv_mutex_unlock(&thread_pool->queue_mutex);
}

static VALUE future_queue_pop(future_thread_pool *thread_pool)
{
    VALUE future;

    nogvl_mutex_lock(&thread_pool->queue_mutex);
    future = rb_ary_pop(thread_pool->queue);
    uv_mutex_unlock(&thread_pool->queue_mutex);

    return future;
}

static void future_result_success_yield(CassandraFuture *cassandra_future)
{
    if (cassandra_future->on_success_block) {
        VALUE obj;

        cassandra_future->proc_state = invoked;

        if (rb_proc_arity(cassandra_future->on_success_block)) {
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

            rb_proc_call_with_block(cassandra_future->on_success_block, 1, &obj, Qnil);
        } else {
            rb_proc_call_with_block(cassandra_future->on_success_block, 0, NULL, Qnil);
        }
    }
}

static void future_result_failure_yield(CassandraFuture *cassandra_future)
{
    if (cassandra_future->on_failure_block) {
        cassandra_future->proc_state = invoked;

        rb_proc_call_with_block(cassandra_future->on_failure_block, 0, NULL, Qnil);
    }
}

static VALUE future_result_yielder(VALUE arg)
{
    CassandraFuture *cassandra_future;

    GET_FUTURE(arg, cassandra_future);
    nogvl_mutex_lock(&cassandra_future->proc_mutex);

    nogvl_future_wait(cassandra_future->future);

    if (cass_future_error_code(cassandra_future->future) == CASS_OK) {
        future_result_success_yield(cassandra_future);
    } else {
        future_result_failure_yield(cassandra_future);
    }
    return Qnil;
}

static VALUE future_result_yielder_ensure(VALUE arg)
{
    CassandraFuture *cassandra_future;

    GET_FUTURE(arg, cassandra_future);
    uv_mutex_unlock(&cassandra_future->proc_mutex);
    return Qnil;
}

static VALUE future_result_yielder_thread(void *arg)
{
    future_thread_arg *thread_arg = (future_thread_arg *)arg;
    VALUE future;

    while (1) {
        future_thread_wait(thread_arg->thread_pool, thread_arg->thread_number);
        future = future_queue_pop(thread_arg->thread_pool);
        rb_ensure(future_result_yielder, future, future_result_yielder_ensure, future);

    }

    free(arg);
    return Qnil;
}

static VALUE future_on_success_body(VALUE future)
{
    CassandraFuture *cassandra_future;

    GET_FUTURE(future, cassandra_future);
    nogvl_mutex_lock(&cassandra_future->proc_mutex);

    cassandra_future->on_success_block = rb_block_proc();
    if (cass_future_ready(cassandra_future->future) && cassandra_future->proc_state == initial) {
        if (cass_future_error_code(cassandra_future->future) == CASS_OK) {
            future_result_success_yield(cassandra_future);
        }
    } else {
        future_queue_push(thread_for_future(cassandra_future), future);
        future_thread_signal(thread_for_future(cassandra_future));
    }
    return Qnil;
}

static VALUE future_on_success_ensure(VALUE future)
{
    CassandraFuture *cassandra_future;

    GET_FUTURE(future, cassandra_future);
    uv_mutex_unlock(&cassandra_future->proc_mutex);
    return Qnil;
}

static VALUE future_on_success(VALUE self)
{
    CassandraFuture *cassandra_future;

    GET_FUTURE(self, cassandra_future);

    if (rb_block_given_p()) {
        rb_ensure(future_on_success_body, self, future_on_success_ensure, self);
    }
    return self;
}

static VALUE future_on_failure_body(VALUE future)
{
    CassandraFuture *cassandra_future;

    GET_FUTURE(future, cassandra_future);
    nogvl_mutex_lock(&cassandra_future->proc_mutex);

    cassandra_future->on_failure_block = rb_block_proc();
    if (cass_future_ready(cassandra_future->future) && cassandra_future->proc_state == initial) {
        if (cass_future_error_code(cassandra_future->future) != CASS_OK) {
            future_result_failure_yield(cassandra_future);
        }
    } else {
        future_queue_push(thread_for_future(cassandra_future), future);
        future_thread_signal(thread_for_future(cassandra_future));
    }
    return Qnil;
}

static VALUE future_on_failure_ensure(VALUE future)
{
    CassandraFuture *cassandra_future;

    GET_FUTURE(future, cassandra_future);
    uv_mutex_unlock(&cassandra_future->proc_mutex);
    return Qnil;
}

static VALUE future_on_failure(VALUE self)
{
    CassandraFuture *cassandra_future;

    GET_FUTURE(self, cassandra_future);

    if (rb_block_given_p()) {
        rb_ensure(future_on_failure_body, self, future_on_failure_ensure, self);
    }
    return self;
}

static void future_mark(void *ptr)
{
    CassandraFuture *cassandra_future = (CassandraFuture *)ptr;
    rb_gc_mark(cassandra_future->session_obj);
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
    uv_mutex_destroy(&cassandra_future->proc_mutex);
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

    future_thread_init(&thread_prepare);
    future_thread_init(&thread_execute);
}
