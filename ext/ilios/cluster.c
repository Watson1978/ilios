#include "ilios.h"

static void cluster_mark(void *ptr);
static void cluster_destroy(void *ptr);
static size_t cluster_memsize(const void *ptr);
static void cluster_compact(void *ptr);

const rb_data_type_t cassandra_cluster_data_type = {
    "Ilios::Cassandra::Cluster",
    {
        cluster_mark,
        cluster_destroy,
        cluster_memsize,
        cluster_compact,
    },
    0, 0,
    RUBY_TYPED_FREE_IMMEDIATELY | RUBY_TYPED_WB_PROTECTED | RUBY_TYPED_FROZEN_SHAREABLE,
};

static VALUE cluster_allocator(VALUE klass)
{
    CassandraCluster *cassandra_cluster;
    return CREATE_CLUSTER(cassandra_cluster);
}

/**
 * Creates a new cluster.
 *
 * @return [Cassandra::Cluster] A new cluster.
 */
static VALUE cluster_initialize(VALUE self)
{
    CassandraCluster *cassandra_cluster;

    GET_CLUSTER(self, cassandra_cluster);
    cassandra_cluster->cluster = cass_cluster_new();

    return self;
}

/**
 * Connects a session.
 *
 * @return [Cassandra::Session] A new session object.
 * @raise [RuntimeError] If no host is specified to connect in +config+ method.
 * @raise [Cassandra::ConnectError] If the connection fails for any reason.
 */
static VALUE cluster_connect(VALUE self)
{
    CassandraSession *cassandra_session;
    CassandraCluster *cassandra_cluster;
    VALUE cassandra_session_obj;
    const char *keyspace = "";

    GET_CLUSTER(self, cassandra_cluster);
    if (cassandra_cluster->keyspace) {
        keyspace = StringValueCStr(cassandra_cluster->keyspace);
    }

    cassandra_session_obj = CREATE_SESSION(cassandra_session);
    cassandra_session->cluster_obj = self;
    cassandra_session->session = cass_session_new();
    cassandra_session->connect_future = cass_session_connect_keyspace(cassandra_session->session, cassandra_cluster->cluster, keyspace);
    nogvl_future_wait(cassandra_session->connect_future);

    if (cass_future_error_code(cassandra_session->connect_future) != CASS_OK) {
        char error[4096] = { 0 };

        strncpy(error, cass_error_desc(cass_future_error_code(cassandra_session->connect_future)), sizeof(error) - 1);
        rb_raise(eConnectError, "Unable to connect: %s", error);
        return Qnil;
    }

    return cassandra_session_obj;
}

/**
 * Sets the contact points.
 *
 * @param hosts [Array<String>] An array of contact points.
 * @return [Cassandra::Cluster] self.
 */
static VALUE cluster_hosts(VALUE self, VALUE hosts)
{
    CassandraCluster *cassandra_cluster;

    GET_CLUSTER(self, cassandra_cluster);

    Check_Type(hosts, T_ARRAY);
    if (RARRAY_LEN(hosts) == 0) {
        rb_raise(rb_eArgError, "No host exists.");
    }

    for (int i = 0; i < RARRAY_LEN(hosts); i++) {
        VALUE host = RARRAY_AREF(hosts, i);
        cass_cluster_set_contact_points(cassandra_cluster->cluster, StringValueCStr(host));
    }

    return self;
}

/**
 * Sets the port number.
 * Default is +9042+.
 *
 * @param port [Integer] A port number.
 * @return [Cassandra::Cluster] self.
 */
static VALUE cluster_port(VALUE self, VALUE port)
{
    CassandraCluster *cassandra_cluster;

    GET_CLUSTER(self, cassandra_cluster);
    cass_cluster_set_port(cassandra_cluster->cluster, NUM2INT(port));

    return self;
}

/**
 * Sets the keyspace.
 *
 * @param keyspace [Integer] A keyspace.
 * @return [Cassandra::Cluster] self.
 */
static VALUE cluster_keyspace(VALUE self, VALUE keyspace)
{
    CassandraCluster *cassandra_cluster;

    StringValue(keyspace);

    GET_CLUSTER(self, cassandra_cluster);
    cassandra_cluster->keyspace = keyspace;

    return self;
}

/**
 * Sets the timeout for connecting to a node.
 * Default is +5000+ milliseconds.
 *
 * @param timeout_ms [Integer] A connect timeout in milliseconds.
 * @return [Cassandra::Cluster] self.
 */
static VALUE cluster_connect_timeout(VALUE self, VALUE timeout_ms)
{
    CassandraCluster *cassandra_cluster;

    GET_CLUSTER(self, cassandra_cluster);
    cass_cluster_set_connect_timeout(cassandra_cluster->cluster, NUM2UINT(timeout_ms));

    return self;
}

/**
 * Sets the protocol version. The driver will automatically downgrade to the lowest supported protocol version.
 * Default is +PROTOCOL_VERSION_V4+.
 *
 * @param timeout_ms [Integer] A connect timeout in milliseconds.
 * @return [Cassandra::Cluster] self.
 */
static VALUE cluster_protocol_version(VALUE self, VALUE version)
{
    CassandraCluster *cassandra_cluster;

    GET_CLUSTER(self, cassandra_cluster);
    cass_cluster_set_protocol_version(cassandra_cluster->cluster, NUM2INT(version));

    return self;
}

/**
 * Sets the timeout for waiting for a response from a node.
 * Default is +12000+ milliseconds.
 *
 * @param timeout_ms [Integer] A request timeout in milliseconds.
 * @return [Cassandra::Cluster] self.
 */
static VALUE cluster_request_timeout(VALUE self, VALUE timeout_ms)
{
    CassandraCluster *cassandra_cluster;

    GET_CLUSTER(self, cassandra_cluster);
    cass_cluster_set_request_timeout(cassandra_cluster->cluster, NUM2UINT(timeout_ms));

    return self;
}

/**
 * Sets the timeout for waiting for DNS name resolution.
 * Default is +2000+ milliseconds.
 *
 * @param timeout_ms [Integer] A request timeout in milliseconds.
 * @return [Cassandra::Cluster] self.
 */
static VALUE cluster_resolve_timeout(VALUE self, VALUE timeout_ms)
{
    CassandraCluster *cassandra_cluster;

    GET_CLUSTER(self, cassandra_cluster);
    cass_cluster_set_resolve_timeout(cassandra_cluster->cluster, NUM2UINT(timeout_ms));

    return self;
}

/**
 * Enable constant speculative executions with the supplied settings.
 *
 * @param constant_delay_ms [Integer]
 * @param max_speculative_executions [Integer]
 * @return [Cassandra::Cluster] self.
 */
static VALUE cluster_constant_speculative_execution_policy(VALUE self, VALUE constant_delay_ms, VALUE max_speculative_executions)
{
    CassandraCluster *cassandra_cluster;

    if (NUM2LONG(constant_delay_ms) < 0 || NUM2INT(max_speculative_executions) < 0) {
        rb_raise(rb_eArgError, "Bad parameters.");
    }

    GET_CLUSTER(self, cassandra_cluster);
    cass_cluster_set_constant_speculative_execution_policy(cassandra_cluster->cluster, NUM2LONG(constant_delay_ms), NUM2INT(max_speculative_executions));

    return self;
}

static void cluster_mark(void *ptr)
{
    CassandraCluster *cassandra_cluster = (CassandraCluster *)ptr;
    rb_gc_mark_movable(cassandra_cluster->keyspace);
}

static void cluster_destroy(void *ptr)
{
    CassandraCluster *cassandra_cluster = (CassandraCluster *)ptr;

    if (cassandra_cluster->cluster) {
        cass_cluster_free(cassandra_cluster->cluster);
    }
    xfree(cassandra_cluster);
}

static size_t cluster_memsize(const void *ptr)
{
    return sizeof(CassandraCluster);
}

static void cluster_compact(void *ptr)
{
    CassandraCluster *cassandra_cluster = (CassandraCluster *)ptr;

    cassandra_cluster->keyspace = rb_gc_location(cassandra_cluster->keyspace);
}

void Init_cluster(void)
{
    rb_define_alloc_func(cCluster, cluster_allocator);
    rb_define_method(cCluster, "initialize", cluster_initialize, 0);
    rb_define_method(cCluster, "connect", cluster_connect, 0);
    rb_define_method(cCluster, "hosts", cluster_hosts, 1);
    rb_define_method(cCluster, "port", cluster_port, 1);
    rb_define_method(cCluster, "keyspace", cluster_keyspace, 1);
    rb_define_method(cCluster, "protocol_version", cluster_protocol_version, 1);
    rb_define_method(cCluster, "connect_timeout", cluster_connect_timeout, 1);
    rb_define_method(cCluster, "request_timeout", cluster_request_timeout, 1);
    rb_define_method(cCluster, "resolve_timeout", cluster_resolve_timeout, 1);
    rb_define_method(cCluster, "constant_speculative_execution_policy", cluster_constant_speculative_execution_policy, 2);

    rb_define_const(cCluster, "PROTOCOL_VERSION_V1", INT2NUM(CASS_PROTOCOL_VERSION_V1));
    rb_define_const(cCluster, "PROTOCOL_VERSION_V2", INT2NUM(CASS_PROTOCOL_VERSION_V2));
    rb_define_const(cCluster, "PROTOCOL_VERSION_V3", INT2NUM(CASS_PROTOCOL_VERSION_V3));
    rb_define_const(cCluster, "PROTOCOL_VERSION_V4", INT2NUM(CASS_PROTOCOL_VERSION_V4));
    rb_define_const(cCluster, "PROTOCOL_VERSION_V5", INT2NUM(CASS_PROTOCOL_VERSION_V5));
    rb_define_const(cCluster, "PROTOCOL_VERSION_DSEV1", INT2NUM(CASS_PROTOCOL_VERSION_DSEV1));
    rb_define_const(cCluster, "PROTOCOL_VERSION_DSEV2", INT2NUM(CASS_PROTOCOL_VERSION_DSEV2));
}
