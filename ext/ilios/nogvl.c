#include "ilios.h"

typedef struct {
    CassSession* session;
    const char *query;
} nogvl_session_prepare_args;

typedef struct {
    CassSession* session;
    CassStatement* statement;
} nogvl_session_execute_args;

static void *nogvl_future_wait_cb(void *ptr)
{
    CassFuture *future = (CassFuture *)ptr;
    cass_future_wait(future);
    return NULL;
}

void nogvl_future_wait(CassFuture *future)
{
    rb_thread_call_without_gvl(nogvl_future_wait_cb, future, RUBY_UBF_PROCESS, 0);
}

static void *nogvl_session_prepare_cb(void *ptr)
{
    nogvl_session_prepare_args *args = (nogvl_session_prepare_args *)ptr;
    CassFuture *prepare_future;

    prepare_future = cass_session_prepare(args->session, args->query);
    return (void *)prepare_future;
}

CassFuture *nogvl_session_prepare(CassSession* session, VALUE query)
{
    nogvl_session_prepare_args args = { session, StringValueCStr(query) };
    return (CassFuture *)rb_thread_call_without_gvl(nogvl_session_prepare_cb, &args, RUBY_UBF_PROCESS, 0);
}

static void *nogvl_session_execute_cb(void *ptr)
{
    nogvl_session_execute_args *args = (nogvl_session_execute_args *)ptr;
    CassFuture *result_future;

    result_future = cass_session_execute(args->session, args->statement);
    return (void *)result_future;
}

CassFuture *nogvl_session_execute(CassSession* session, CassStatement* statement)
{
    nogvl_session_execute_args args = { session, statement };
    return (CassFuture *)rb_thread_call_without_gvl(nogvl_session_execute_cb, &args, RUBY_UBF_PROCESS, 0);
}

static void *nogvl_sem_wait_cb(void *sem)
{
    uv_sem_wait((uv_sem_t *)sem);
    return NULL;
}

void nogvl_sem_wait(uv_sem_t *sem_thread)
{
    // When using uv_sem_wait, it need to release GVL due to switch to another thread
    rb_thread_call_without_gvl(nogvl_sem_wait_cb, sem_thread, RUBY_UBF_PROCESS, 0);
}

static void *nogvl_mutex_lock_cb(void *mutex)
{
    uv_mutex_lock((uv_mutex_t *)mutex);
    return NULL;
}

void nogvl_mutex_lock(uv_mutex_t *mutex)
{
    // When using uv_mutex_lock, it need to release GVL due to switch to another thread
    rb_thread_call_without_gvl(nogvl_mutex_lock_cb, mutex, RUBY_UBF_PROCESS, 0);
}
