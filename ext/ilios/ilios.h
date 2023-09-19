#ifndef ILIOS_H
#define ILIOS_H

#include <stdint.h>
#include <cassandra.h>
#include "ruby.h"
#include "ruby/thread.h"

typedef struct
{
    CassCluster* cluster;
    CassFuture* connect_future;
    CassSession* session;

} CassandraSession;

typedef struct
{
    CassStatement* statement;
    VALUE session_obj;
} CassandraStatement;

typedef struct
{
    const CassResult *result;
    CassFuture *future;
} CassandraResult;

extern const rb_data_type_t cassandra_session_data_type;
extern const rb_data_type_t cassandra_statement_data_type;
extern const rb_data_type_t cassandra_result_data_type;

extern VALUE mIlios;
extern VALUE mCassandra;
extern VALUE cSession;
extern VALUE cStatement;
extern VALUE cResult;
extern VALUE eConnectError;
extern VALUE eExecutionError;
extern VALUE eStatementError;

extern VALUE id_cvar_config;
extern VALUE id_shuffle;
extern VALUE sym_unsupported_column_type;
extern VALUE sym_keyspace;
extern VALUE sym_hosts;
extern VALUE sym_timeout_ms;
extern VALUE sym_constant_delay_ms;
extern VALUE sym_max_speculative_executions;

extern void Init_cassandra(void);
extern void Init_session(void);
extern void Init_statement(void);
extern void Init_result(void);

extern void nogvl_future_wait(CassFuture *future);
extern CassFuture *nogvl_session_prepare(CassSession* session, VALUE query);
extern CassFuture *nogvl_session_execute(CassSession* session, CassStatement* statement);

extern VALUE result_await(VALUE self);

#endif // ILIOS_H
