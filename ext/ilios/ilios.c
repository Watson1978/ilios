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
VALUE sym_unsupported_column_type;
VALUE sym_keyspace;
VALUE sym_hosts;
VALUE sym_timeout_ms;
VALUE sym_constant_delay_ms;
VALUE sym_max_speculative_executions;
VALUE sym_page_size;

void Init_ilios(void)
{
    // Use Ruby's memory allocator in cassandra-cpp-driver in order to notify memory usage in library
    // and work Ruby's GC properly.
#if 0
    // Cause SEGV in cass_session_close()
    cass_alloc_set_functions(xmalloc, xrealloc, xfree);
#endif

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
}
