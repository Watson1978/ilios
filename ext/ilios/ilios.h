#ifndef ILIOS_H
#define ILIOS_H

#include <stdint.h>
#include <float.h>
#include <cassandra.h>
#include <uv.h>
#include "ruby.h"
#include "ruby/thread.h"

#define GET_SESSION(obj, var)   TypedData_Get_Struct(obj, CassandraSession, &cassandra_session_data_type, var)
#define GET_STATEMENT(obj, var) TypedData_Get_Struct(obj, CassandraStatement, &cassandra_statement_data_type, var)
#define GET_RESULT(obj, var)    TypedData_Get_Struct(obj, CassandraResult, &cassandra_result_data_type, var)
#define GET_FUTURE(obj, var)    TypedData_Get_Struct(obj, CassandraFuture, &cassandra_future_data_type, var)
#define CREATE_SESSION(var)     TypedData_Make_Struct(cSession, CassandraSession, &cassandra_session_data_type, var)
#define CREATE_STATEMENT(var)   TypedData_Make_Struct(cStatement, CassandraStatement, &cassandra_statement_data_type, var)
#define CREATE_RESULT(var)      TypedData_Make_Struct(cResult, CassandraResult, &cassandra_result_data_type, var)
#define CREATE_FUTURE(var)      TypedData_Make_Struct(cFuture, CassandraFuture, &cassandra_future_data_type, var)

typedef enum {
  prepare_async,
  execute_async
} future_kind;

typedef struct
{
    CassCluster* cluster;
    CassFuture* connect_future;
    CassSession* session;

} CassandraSession;

typedef struct
{
    CassStatement* statement;
    const CassPrepared* prepared;
    VALUE session_obj;
} CassandraStatement;

typedef struct
{
    const CassResult *result;
    CassFuture *future;
    VALUE statement_obj;
} CassandraResult;

typedef struct
{
    CassFuture *future;
    future_kind kind;

    VALUE session_obj;
    VALUE statement_obj;
    VALUE thread_obj;
    VALUE on_success_block;
    VALUE on_failure_block;
} CassandraFuture;

extern const rb_data_type_t cassandra_session_data_type;
extern const rb_data_type_t cassandra_statement_data_type;
extern const rb_data_type_t cassandra_result_data_type;
extern const rb_data_type_t cassandra_future_data_type;

extern VALUE mIlios;
extern VALUE mCassandra;
extern VALUE cSession;
extern VALUE cStatement;
extern VALUE cResult;
extern VALUE cFuture;
extern VALUE eConnectError;
extern VALUE eExecutionError;
extern VALUE eStatementError;

extern VALUE id_cvar_config;
extern VALUE id_shuffle;
extern VALUE id_to_time;
extern VALUE sym_unsupported_column_type;
extern VALUE sym_keyspace;
extern VALUE sym_hosts;
extern VALUE sym_timeout_ms;
extern VALUE sym_constant_delay_ms;
extern VALUE sym_max_speculative_executions;
extern VALUE sym_page_size;

extern void Init_cassandra(void);
extern void Init_session(void);
extern void Init_statement(void);
extern void Init_result(void);
extern void Init_future(void);

extern void nogvl_future_wait(CassFuture *future);
extern CassFuture *nogvl_session_prepare(CassSession* session, VALUE query);
extern CassFuture *nogvl_session_execute(CassSession* session, CassStatement* statement);
extern void nogvl_sem_wait(uv_sem_t *sem_thread);

extern void statement_default_config(CassandraStatement *cassandra_statement);
extern void result_await(CassandraResult *cassandra_result);


#endif // ILIOS_H
