#include "ilios.h"

VALUE mIlios;
VALUE mCassandra;
VALUE cCluster;
VALUE cSession;
VALUE cStatement;
VALUE cResult;
VALUE cFuture;
VALUE eConnectError;
VALUE eExecutionError;
VALUE eStatementError;

VALUE cQueue;

VALUE id_to_time;
VALUE id_new;
VALUE id_push;
VALUE id_pop;
VALUE sym_unsupported_column_type;

#if defined(HAVE_MALLOC_USABLE_SIZE)
#include <malloc.h>
#elif defined(HAVE_MALLOC_SIZE)
#include <malloc/malloc.h>
#endif

static ssize_t ilios_malloc_size(void *ptr)
{
#if defined(HAVE_MALLOC_USABLE_SIZE)
    return malloc_usable_size(ptr);
#elif defined(HAVE_MALLOC_SIZE)
    return malloc_size(ptr);
#endif
}

static void *ilios_malloc(size_t size)
{
    rb_gc_adjust_memory_usage(size);
    return malloc(size);
}

static void *ilios_realloc(void *ptr, size_t size)
{
    ssize_t before_size = ilios_malloc_size(ptr);
    rb_gc_adjust_memory_usage(size - before_size);
    return realloc(ptr, size);
}

static void ilios_free(void *ptr)
{
    if (ptr) {
        ssize_t size = ilios_malloc_size(ptr);
        rb_gc_adjust_memory_usage(-size);
        free(ptr);
    }
}

void Init_ilios(void)
{
    rb_ext_ractor_safe(true);

    mIlios = rb_define_module("Ilios");
    mCassandra = rb_define_module_under(mIlios, "Cassandra");
    cCluster = rb_define_class_under(mCassandra, "Cluster", rb_cObject);
    cSession = rb_define_class_under(mCassandra, "Session", rb_cObject);
    cStatement = rb_define_class_under(mCassandra, "Statement", rb_cObject);
    cResult = rb_define_class_under(mCassandra, "Result", rb_cObject);
    cFuture = rb_define_class_under(mCassandra, "Future", rb_cObject);
    eConnectError = rb_define_class_under(mCassandra, "ConnectError", rb_eStandardError);
    eExecutionError = rb_define_class_under(mCassandra, "ExecutionError", rb_eStandardError);
    eStatementError = rb_define_class_under(mCassandra, "StatementError", rb_eStandardError);

    cQueue = rb_const_get(rb_cThread, rb_intern("Queue"));

    id_to_time = rb_intern("to_time");
    id_new = rb_intern("new");
    id_push = rb_intern("push");
    id_pop = rb_intern("pop");
    sym_unsupported_column_type = ID2SYM(rb_intern("unsupported_column_type"));

    Init_cluster();
    Init_session();
    Init_statement();
    Init_result();
    Init_future();

    cass_log_set_level(CASS_LOG_ERROR);

#if defined(HAVE_MALLOC_USABLE_SIZE) || defined(HAVE_MALLOC_SIZE)
    cass_alloc_set_functions(ilios_malloc, ilios_realloc, ilios_free);
#endif
}
