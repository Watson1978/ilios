#include "ilios.h"

VALUE mIlios;
VALUE mCassandra;
VALUE cSession;
VALUE cStatement;
VALUE cResult;
VALUE cFuture;
VALUE eConnectError;
VALUE eExecutionError;
VALUE eStatementError;

VALUE id_cvar_config;
VALUE id_shuffle;
VALUE id_to_time;
VALUE id_abort_on_exception_set;
VALUE sym_unsupported_column_type;
VALUE sym_keyspace;
VALUE sym_hosts;
VALUE sym_timeout_ms;
VALUE sym_constant_delay_ms;
VALUE sym_max_speculative_executions;
VALUE sym_page_size;

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
    mIlios = rb_define_module("Ilios");
    mCassandra = rb_define_module_under(mIlios, "Cassandra");
    cSession = rb_define_class_under(mCassandra, "Session", rb_cObject);
    cStatement = rb_define_class_under(mCassandra, "Statement", rb_cObject);
    cResult = rb_define_class_under(mCassandra, "Result", rb_cObject);
    cFuture = rb_define_class_under(mCassandra, "Future", rb_cObject);
    eConnectError = rb_define_class_under(mCassandra, "ConnectError", rb_eStandardError);
    eExecutionError = rb_define_class_under(mCassandra, "ExecutionError", rb_eStandardError);
    eStatementError = rb_define_class_under(mCassandra, "StatementError", rb_eStandardError);

    id_cvar_config = rb_intern("@@config");
    id_shuffle = rb_intern("shuffle");
    id_to_time = rb_intern("to_time");
    id_abort_on_exception_set = rb_intern("abort_on_exception=");
    sym_unsupported_column_type = ID2SYM(rb_intern("unsupported_column_type"));
    sym_keyspace = ID2SYM(rb_intern("keyspace"));
    sym_hosts = ID2SYM(rb_intern("hosts"));
    sym_timeout_ms = ID2SYM(rb_intern("timeout_ms"));
    sym_constant_delay_ms = ID2SYM(rb_intern("constant_delay_ms"));
    sym_max_speculative_executions = ID2SYM(rb_intern("max_speculative_executions"));
    sym_page_size = ID2SYM(rb_intern("page_size"));

    Init_cassandra();
    Init_session();
    Init_statement();
    Init_result();
    Init_future();

    cass_log_set_level(CASS_LOG_ERROR);

#if defined(HAVE_MALLOC_USABLE_SIZE) || defined(HAVE_MALLOC_SIZE)
    cass_alloc_set_functions(ilios_malloc, ilios_realloc, ilios_free);
#endif
}
