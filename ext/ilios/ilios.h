#ifndef ILIOS_H
#define ILIOS_H

#include <cassandra.h>
#include "ruby.h"

typedef struct
{
    CassCluster* cluster;
    CassFuture* connect_future;
    CassSession* session;

} CassandraSession;

typedef struct
{
    const CassPrepared* prepared;
    CassStatement* statement;
} CassandraStatement;

extern const rb_data_type_t cassandra_session_data_type;
extern const rb_data_type_t cassandra_statement_data_type;

extern VALUE mIlios;
extern VALUE mCassandra;
extern VALUE cSession;
extern VALUE cStatement;
extern VALUE eConnectError;
extern VALUE eExecutionError;

extern VALUE id_cvar_config;
extern VALUE sym_keyspace;
extern VALUE sym_hosts;
extern VALUE sym_timeout_ms;
extern VALUE sym_constant_delay_ms;
extern VALUE sym_max_speculative_executions;

extern void Init_cassandra(void);
extern void Init_session(void);

#endif // ILIOS_H
