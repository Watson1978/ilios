#include "ilios.h"

/**
 * Connects a session to the keyspace specified in +config+ method.
 *
 * @return [Cassandra::Session] A new session object.
 * @raise [RuntimeError] If no host is specified to connect in +config+ method.
 * @raise [Cassandra::ConnectError] If the connection fails for any reason.
 */
static VALUE cassandra_connect(VALUE self)
{
    CassandraSession *cassandra_session;
    VALUE cassandra_session_obj;
    VALUE config;
    VALUE hosts;
    VALUE keyspace;

    cassandra_session_obj = CREATE_SESSION(cassandra_session);

    cassandra_session->cluster = cass_cluster_new();
    cass_cluster_set_protocol_version(cassandra_session->cluster, CASS_PROTOCOL_VERSION_V4);

    config = rb_cvar_get(self, id_cvar_config);
    cass_cluster_set_request_timeout(cassandra_session->cluster, NUM2UINT(rb_hash_aref(config, sym_timeout_ms)));
    cass_cluster_set_constant_speculative_execution_policy(cassandra_session->cluster, NUM2LONG(rb_hash_aref(config, sym_constant_delay_ms)), NUM2INT(rb_hash_aref(config, sym_max_speculative_executions)));

    keyspace = rb_hash_aref(config, sym_keyspace);
    hosts = rb_hash_aref(config, sym_hosts);

    Check_Type(hosts, T_ARRAY);
    if (RARRAY_LEN(hosts) == 0) {
        rb_raise(rb_eRuntimeError, "No hosts configured");
    }
    if (RARRAY_LEN(hosts) > 1) {
        // To distribute connections
        hosts = rb_funcall(hosts, id_shuffle, 0);
    }

    for (int i = 0; i < RARRAY_LEN(hosts); i++) {
        VALUE host = RARRAY_AREF(hosts, i);
        cass_cluster_set_contact_points(cassandra_session->cluster, StringValueCStr(host));
    }
    cassandra_session->session = cass_session_new();
    cassandra_session->connect_future =  cass_session_connect_keyspace(cassandra_session->session, cassandra_session->cluster, StringValueCStr(keyspace));
    nogvl_future_wait(cassandra_session->connect_future);

    if (cass_future_error_code(cassandra_session->connect_future) != CASS_OK) {
        char error[4096] = { 0 };

        strncpy(error, cass_error_desc(cass_future_error_code(cassandra_session->connect_future)), sizeof(error) - 1);
        cass_future_free(cassandra_session->connect_future);
        cass_session_free(cassandra_session->session);
        cass_cluster_free(cassandra_session->cluster);
        rb_raise(eConnectError, "Unable to connect: %s", error);
        return Qnil;
    }

    return cassandra_session_obj;
}

void Init_cassandra(void)
{
    rb_define_module_function(mCassandra, "connect", cassandra_connect, 0);
}
