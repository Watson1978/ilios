#include "ilios.h"

#define THREAD_MAX 5
#define QUEUE_MAX 100

typedef struct
{
    VALUE thread[THREAD_MAX];
    VALUE queue;
} future_thread_pool;

static future_thread_pool thread_pool_prepare;
static future_thread_pool thread_pool_execute;

static VALUE future_result_yielder_thread(void *arg);
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

static void future_thread_pool_init(future_thread_pool *pool)
{
    pool->queue = rb_funcall(cSizedQueue, id_new, 1, INT2NUM(QUEUE_MAX));
    rb_gc_register_mark_object(pool->queue);
}

static void future_thread_pool_prepare_thread(future_thread_pool *pool)
{
    VALUE status;

    for (int i = 0; i < THREAD_MAX; i++) {
        status = Qfalse;

        if (pool->thread[i]) {
            status = rb_funcall(pool->thread[i], id_alive, 0);
        }
        if (!pool->thread[i] || !RTEST(status)) {
            pool->thread[i] = rb_thread_create(future_result_yielder_thread, (void*)pool);
            rb_funcall(pool->thread[i], id_report_on_exception, 1, Qtrue);
            rb_gc_register_address(&pool->thread[i]);
        }
    }
}

static inline future_thread_pool *future_thread_pool_get(CassandraFuture *cassandra_future)
{
    future_thread_pool *pool = NULL;

    switch (cassandra_future->kind) {
    case prepare_async:
        pool = &thread_pool_prepare;
        break;
    case execute_async:
        pool = &thread_pool_execute;
        break;
    }
    return pool;
}

static inline void future_queue_push(future_thread_pool *pool, VALUE future)
{
    rb_funcall(pool->queue, id_push, 1, future);
    future_thread_pool_prepare_thread(pool);
}

static inline VALUE future_queue_pop(future_thread_pool *pool)
{
    return rb_funcall(pool->queue, id_pop, 0);
}

static void future_result_success_yield(CassandraFuture *cassandra_future)
{
    VALUE obj;

    if (cassandra_future->on_success_block) {
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
        rb_proc_call_with_block(cassandra_future->on_failure_block, 0, NULL, Qnil);
    }
}

static VALUE future_result_yielder_synchronize(VALUE future)
{
    CassandraFuture *cassandra_future;

    GET_FUTURE(future, cassandra_future);

    if (cass_future_error_code(cassandra_future->future) == CASS_OK) {
        future_result_success_yield(cassandra_future);
    } else {
        future_result_failure_yield(cassandra_future);
    }
    cassandra_future->already_waited = true;
    return Qnil;
}

static VALUE future_result_yielder_body(VALUE future)
{
    CassandraFuture *cassandra_future;

    GET_FUTURE(future, cassandra_future);

    nogvl_future_wait(cassandra_future->future);
    return rb_mutex_synchronize(cassandra_future->proc_mutex, future_result_yielder_synchronize, future);
}

static VALUE future_result_yielder_ensure(VALUE future)
{
    CassandraFuture *cassandra_future;

    GET_FUTURE(future, cassandra_future);
    uv_sem_post(&cassandra_future->sem);
    return Qnil;
}

static VALUE future_result_yielder(VALUE future)
{
    return rb_ensure(future_result_yielder_body, future, future_result_yielder_ensure, future);
}

static VALUE future_result_yielder_thread(void *arg)
{
    future_thread_pool *pool = (future_thread_pool *)arg;

    while (1) {
        VALUE future = future_queue_pop(pool);
        future_result_yielder(future);
    }
    return Qnil;
}

VALUE future_create(CassFuture *future, VALUE session, future_kind kind)
{
    CassandraFuture *cassandra_future;
    VALUE cassandra_future_obj;

    cassandra_future_obj = CREATE_FUTURE(cassandra_future);
    cassandra_future->kind = kind;
    cassandra_future->future = future;
    cassandra_future->session_obj = session;
    cassandra_future->proc_mutex = rb_mutex_new();
    uv_sem_init(&cassandra_future->sem, 0);
    cassandra_future->already_waited = false;

    return cassandra_future_obj;
}

static VALUE future_on_success_synchronize(VALUE future)
{
    CassandraFuture *cassandra_future;
    bool wakeup_thread = false;

    GET_FUTURE(future, cassandra_future);

    if (!cassandra_future->on_failure_block) {
        // Invoke the callback with thread pool only once
        wakeup_thread = true;
    }

    cassandra_future->on_success_block = rb_block_proc();

    if (cass_future_ready(cassandra_future->future)) {
        uv_sem_post(&cassandra_future->sem);
        if (cass_future_error_code(cassandra_future->future) == CASS_OK) {
            future_result_success_yield(cassandra_future);
        }
        return future;
    }

    if (wakeup_thread) {
        future_queue_push(future_thread_pool_get(cassandra_future), future);
    }

    return future;
}

/**
 * Run block when future resolves to a value.
 *
 * @yieldparam value [Cassandra::Statement, Cassandra::Result] A value.
 *   Yields +Cassandra::Statement+ object when future was created by +Cassandra::Session#prepare_async+.
 *   Yields +Cassandra::Result+ object when future was created by +Cassandra::Session#execute_async+.
 * @return [Cassandra::Future] self.
 * @raise [Cassandra::ExecutionError] If this method will be called twice.
 * @raise [ArgumentError] If no block was given.
 */
static VALUE future_on_success(VALUE self)
{
    CassandraFuture *cassandra_future;

    GET_FUTURE(self, cassandra_future);

    if (cassandra_future->on_success_block) {
        rb_raise(eExecutionError, "It should not call twice");
    }
    if (!rb_block_given_p()) {
        rb_raise(rb_eArgError, "no block given");
    }

    return rb_mutex_synchronize(cassandra_future->proc_mutex, future_on_success_synchronize, self);
}

static VALUE future_on_failure_synchronize(VALUE future)
{
    CassandraFuture *cassandra_future;
    bool wakeup_thread = false;

    GET_FUTURE(future, cassandra_future);

    if (!cassandra_future->on_success_block) {
        // Invoke the callback with thread pool only once
        wakeup_thread = true;
    }

    cassandra_future->on_failure_block = rb_block_proc();

    if (cass_future_ready(cassandra_future->future)) {
        uv_sem_post(&cassandra_future->sem);
        if (cass_future_error_code(cassandra_future->future) != CASS_OK) {
            future_result_failure_yield(cassandra_future);
        }
        return future;
    }

    if (wakeup_thread) {
        future_queue_push(future_thread_pool_get(cassandra_future), future);
    }

    return future;
}

/**
 * Run block when future resolves to error.
 *
 * @return [Cassandra::Future] self.
 * @raise [Cassandra::ExecutionError] If this method will be called twice.
 * @raise [ArgumentError] If no block was given.
 */
static VALUE future_on_failure(VALUE self)
{
    CassandraFuture *cassandra_future;

    GET_FUTURE(self, cassandra_future);

    if (cassandra_future->on_failure_block) {
        rb_raise(eExecutionError, "It should not call twice");
    }
    if (!rb_block_given_p()) {
        rb_raise(rb_eArgError, "no block given");
    }

    return rb_mutex_synchronize(cassandra_future->proc_mutex, future_on_failure_synchronize, self);
}

/**
 * Wait to complete a future's statement.
 *
 * @return [Cassandra::Future] self.
 */
static VALUE future_await(VALUE self)
{
    CassandraFuture *cassandra_future;

    GET_FUTURE(self, cassandra_future);

    rb_mutex_lock(cassandra_future->proc_mutex);
    if (cassandra_future->already_waited) {
        rb_mutex_unlock(cassandra_future->proc_mutex);
        return self;
    }
    cassandra_future->already_waited = true;
    rb_mutex_unlock(cassandra_future->proc_mutex);

    nogvl_future_wait(cassandra_future->future);
    if (cassandra_future->on_success_block || cassandra_future->on_failure_block) {
        nogvl_sem_wait(&cassandra_future->sem);
    }
    return self;
}

static void future_mark(void *ptr)
{
    CassandraFuture *cassandra_future = (CassandraFuture *)ptr;
    rb_gc_mark_movable(cassandra_future->session_obj);
    rb_gc_mark_movable(cassandra_future->statement_obj);
    rb_gc_mark_movable(cassandra_future->on_success_block);
    rb_gc_mark_movable(cassandra_future->on_failure_block);
    rb_gc_mark_movable(cassandra_future->proc_mutex);
}

static void future_destroy(void *ptr)
{
    CassandraFuture *cassandra_future = (CassandraFuture *)ptr;

    if (cassandra_future->future) {
        cass_future_free(cassandra_future->future);
    }
    uv_sem_destroy(&cassandra_future->sem);
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
    cassandra_future->on_success_block = rb_gc_location(cassandra_future->on_success_block);
    cassandra_future->on_failure_block = rb_gc_location(cassandra_future->on_failure_block);
    cassandra_future->proc_mutex = rb_gc_location(cassandra_future->proc_mutex);
}

void Init_future(void)
{
    rb_undef_alloc_func(cFuture);

    rb_define_method(cFuture, "on_success", future_on_success, 0);
    rb_define_method(cFuture, "on_failure", future_on_failure, 0);
    rb_define_method(cFuture, "await", future_await, 0);

    future_thread_pool_init(&thread_pool_prepare);
    future_thread_pool_init(&thread_pool_execute);
}
