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

static void cluster_check_error(CassError error, const char *name)
{
    if (error != CASS_OK) {
        rb_raise(rb_eArgError, "Invalid %s: %s", name, cass_error_desc(error));
    }
}

// Casting a negative value to unsigned wraps it into a huge number, so check
// the range on the signed value NUM2LONG/NUM2LL already produced. Converting a
// second time would run to_int again, which may return a different value.
static unsigned cluster_value_to_uint(VALUE value, const char *name)
{
    long v = NUM2LONG(value);

    if (v < 0 || v > UINT_MAX) {
        rb_raise(rb_eRangeError, "Invalid %s: %ld", name, v);
    }
    return (unsigned)v;
}

static cass_uint64_t cluster_value_to_uint64(VALUE value, const char *name)
{
    long long v = NUM2LL(value);

    if (v < 0) {
        rb_raise(rb_eRangeError, "Invalid %s: %lld", name, v);
    }
    return (cass_uint64_t)v;
}

/**
 * Creates a new cluster.
 *
 * @return [Cassandra::Cluster] A new cluster.
 */
static VALUE cluster_initialize(VALUE self)
{
    CassandraCluster *cassandra_cluster;

    GET_UNINITIALIZED_CLUSTER(self, cassandra_cluster);
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
    CassFuture* connect_future;
    VALUE cassandra_session_obj;
    const char *keyspace = "";

    GET_CLUSTER(self, cassandra_cluster);
    if (cassandra_cluster->keyspace) {
        keyspace = StringValueCStr(cassandra_cluster->keyspace);
    }

    cassandra_session_obj = CREATE_SESSION(cassandra_session);
    cassandra_session->cluster_obj = self;
    cassandra_session->session = cass_session_new();
    connect_future = cass_session_connect_keyspace(cassandra_session->session, cassandra_cluster->cluster, keyspace);
    nogvl_future_wait(connect_future);

    if (cass_future_error_code(connect_future) != CASS_OK) {
        char error[4096] = { 0 };

        strncpy(error, cass_error_desc(cass_future_error_code(connect_future)), sizeof(error) - 1);
        cass_future_free(connect_future);
        rb_raise(eConnectError, "Unable to connect: %s", error);
        return Qnil;
    }
    cass_future_free(connect_future);

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
    long length;

    GET_CLUSTER(self, cassandra_cluster);

    Check_Type(hosts, T_ARRAY);
    length = RARRAY_LEN(hosts);
    if (length == 0) {
        rb_raise(rb_eArgError, "No host exists.");
    }

    for (long i = 0; i < length && i < RARRAY_LEN(hosts); i++) {
        VALUE host = rb_ary_entry(hosts, i);
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
 * @raise [ArgumentError] If the port is zero or negative.
 */
static VALUE cluster_port(VALUE self, VALUE port)
{
    CassandraCluster *cassandra_cluster;

    GET_CLUSTER(self, cassandra_cluster);
    cluster_check_error(cass_cluster_set_port(cassandra_cluster->cluster, NUM2INT(port)), "port");

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
    RB_OBJ_WRITE(self, &cassandra_cluster->keyspace, keyspace);

    return self;
}

/**
 * Sets the protocol version. The driver will automatically downgrade to the lowest supported protocol version.
 * Default is +PROTOCOL_VERSION_V4+, or +PROTOCOL_VERSION_DSEV1+ against DSE.
 *
 * The driver accepts +PROTOCOL_VERSION_V3+, +PROTOCOL_VERSION_V4+,
 * +PROTOCOL_VERSION_DSEV1+ and +PROTOCOL_VERSION_DSEV2+ only. It ignores
 * +PROTOCOL_VERSION_V1+ and +PROTOCOL_VERSION_V2+, which are below the lowest
 * version it supports, and +PROTOCOL_VERSION_V5+, which is a beta version it
 * enables through a separate setting instead. Each of those leaves the version
 * unchanged and is reported on the driver's error log.
 *
 * @param version [Integer] A protocol version.
 * @return [Cassandra::Cluster] self.
 * @raise [RangeError] If a negative value was given.
 */
static VALUE cluster_protocol_version(VALUE self, VALUE version)
{
    CassandraCluster *cassandra_cluster;
    int v = NUM2INT(version);

    if (v < 0) {
        rb_raise(rb_eRangeError, "Invalid protocol_version: %d", v);
    }

    GET_CLUSTER(self, cassandra_cluster);
    cass_cluster_set_protocol_version(cassandra_cluster->cluster, v);

    return self;
}

/**
 * Sets the timeout for connecting to a node.
 * Default is +5000+ milliseconds.
 *
 * @param timeout_ms [Integer] A connect timeout in milliseconds.
 * @return [Cassandra::Cluster] self.
 * @raise [RangeError] If a negative value was given.
 */
static VALUE cluster_connect_timeout(VALUE self, VALUE timeout_ms)
{
    CassandraCluster *cassandra_cluster;

    GET_CLUSTER(self, cassandra_cluster);
    cass_cluster_set_connect_timeout(cassandra_cluster->cluster, cluster_value_to_uint(timeout_ms, "connect_timeout"));

    return self;
}

/**
 * Sets the timeout for waiting for a response from a node.
 * Default is +12000+ milliseconds.
 *
 * @param timeout_ms [Integer] A request timeout in milliseconds.
 * @return [Cassandra::Cluster] self.
 * @raise [RangeError] If a negative value was given.
 */
static VALUE cluster_request_timeout(VALUE self, VALUE timeout_ms)
{
    CassandraCluster *cassandra_cluster;

    GET_CLUSTER(self, cassandra_cluster);
    cass_cluster_set_request_timeout(cassandra_cluster->cluster, cluster_value_to_uint(timeout_ms, "request_timeout"));

    return self;
}

/**
 * Sets the timeout for waiting for DNS name resolution.
 * Default is +2000+ milliseconds.
 *
 * @param timeout_ms [Integer] A request timeout in milliseconds.
 * @return [Cassandra::Cluster] self.
 * @raise [RangeError] If a negative value was given.
 */
static VALUE cluster_resolve_timeout(VALUE self, VALUE timeout_ms)
{
    CassandraCluster *cassandra_cluster;

    GET_CLUSTER(self, cassandra_cluster);
    cass_cluster_set_resolve_timeout(cassandra_cluster->cluster, cluster_value_to_uint(timeout_ms, "resolve_timeout"));

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
    long delay = NUM2LONG(constant_delay_ms);
    int max_executions = NUM2INT(max_speculative_executions);

    if (delay < 0 || max_executions < 0) {
        rb_raise(rb_eArgError, "Bad parameters.");
    }

    GET_CLUSTER(self, cassandra_cluster);
    cluster_check_error(cass_cluster_set_constant_speculative_execution_policy(cassandra_cluster->cluster, delay, max_executions), "constant_speculative_execution_policy");

    return self;
}

/**
 * Sets credentials for plain text authentication.
 *
 * @param username [String] A username.
 * @param password [String] A password.
 * @return [Cassandra::Cluster] self.
 */
static VALUE cluster_credentials(VALUE self, VALUE username, VALUE password)
{
    CassandraCluster *cassandra_cluster;

    GET_CLUSTER(self, cassandra_cluster);
    // The driver copies both strings into its own memory
    // (cluster_config.cpp: cass_cluster_set_credentials_n), so the Ruby
    // strings do not need to be retained.
    cass_cluster_set_credentials(cassandra_cluster->cluster, StringValueCStr(username), StringValueCStr(password));

    return self;
}

// The driver only rejects CASS_CONSISTENCY_UNKNOWN (0xFFFF) at set time; any
// other out-of-range value is accepted and later fails every query with an
// opaque protocol error, so validate the range here. This also keeps UNKNOWN
// away from the driver, which is why the setters ignore its return value.
static void cluster_check_consistency(long value, const char *name)
{
    if (value < CASS_CONSISTENCY_ANY || value > CASS_CONSISTENCY_LOCAL_ONE) {
        rb_raise(rb_eArgError, "Invalid %s: %ld", name, value);
    }
}

static struct {
    const char *name;
    CassConsistency consistency;
    ID id;
} cluster_consistency_symbols[] = {
    { "any", CASS_CONSISTENCY_ANY, 0 },
    { "one", CASS_CONSISTENCY_ONE, 0 },
    { "two", CASS_CONSISTENCY_TWO, 0 },
    { "three", CASS_CONSISTENCY_THREE, 0 },
    { "quorum", CASS_CONSISTENCY_QUORUM, 0 },
    { "all", CASS_CONSISTENCY_ALL, 0 },
    { "local_quorum", CASS_CONSISTENCY_LOCAL_QUORUM, 0 },
    { "each_quorum", CASS_CONSISTENCY_EACH_QUORUM, 0 },
    { "serial", CASS_CONSISTENCY_SERIAL, 0 },
    { "local_serial", CASS_CONSISTENCY_LOCAL_SERIAL, 0 },
    { "local_one", CASS_CONSISTENCY_LOCAL_ONE, 0 },
};

// Accepts either an Integer consistency constant or a Symbol such as
// +:quorum+/+:local_serial+ naming one of the CONSISTENCY_* constants.
static CassConsistency cluster_value_to_consistency(VALUE value, const char *name)
{
    long v;

    if (SYMBOL_P(value)) {
        // rb_sym2id would intern an unknown Symbol and leave it uncollectable, so
        // repeated invalid values grow memory with no way to reclaim it.
        VALUE name_value = value;
        ID id = rb_check_id(&name_value);

        for (size_t i = 0; id && i < sizeof(cluster_consistency_symbols) / sizeof(cluster_consistency_symbols[0]); i++) {
            if (id == cluster_consistency_symbols[i].id) {
                return cluster_consistency_symbols[i].consistency;
            }
        }
        rb_raise(rb_eArgError, "Invalid %s: %"PRIsVALUE"", name, value);
    }

    v = NUM2LONG(value);
    cluster_check_consistency(v, name);
    return (CassConsistency)v;
}

/**
 * Sets the default consistency level of the statement.
 * Default is +CONSISTENCY_LOCAL_ONE+.
 *
 * @param consistency [Integer, Symbol] A consistency level.
 *   Symbols such as +:quorum+ or +:local_serial+ are also accepted.
 * @return [Cassandra::Cluster] self.
 * @raise [ArgumentError] If an invalid consistency level was given.
 * @raise [RangeError] If the value does not fit in a C long.
 */
static VALUE cluster_consistency(VALUE self, VALUE consistency)
{
    CassandraCluster *cassandra_cluster;
    CassConsistency consistency_value = cluster_value_to_consistency(consistency, "consistency");

    GET_CLUSTER(self, cassandra_cluster);
    cass_cluster_set_consistency(cassandra_cluster->cluster, consistency_value);

    return self;
}

/**
 * Sets the default serial consistency level of the statement.
 * Default is +CONSISTENCY_ANY+.
 *
 * @param consistency [Integer, Symbol] A serial consistency level. Only
 *   +CONSISTENCY_SERIAL+/+CONSISTENCY_LOCAL_SERIAL+ (or +:serial+/+:local_serial+)
 *   are accepted.
 * @return [Cassandra::Cluster] self.
 * @raise [ArgumentError] If an invalid consistency level was given, or if it is
 *   not +CONSISTENCY_SERIAL+ or +CONSISTENCY_LOCAL_SERIAL+.
 * @raise [RangeError] If the value does not fit in a C long.
 */
static VALUE cluster_serial_consistency(VALUE self, VALUE consistency)
{
    CassandraCluster *cassandra_cluster;
    CassConsistency consistency_value = cluster_value_to_consistency(consistency, "serial_consistency");

    // Cassandra only accepts SERIAL/LOCAL_SERIAL as a serial consistency
    // level (used for lightweight transactions); any other value passes
    // the generic range check above but is later rejected by the server
    // at query time with an opaque error, so validate it here.
    if (consistency_value != CASS_CONSISTENCY_SERIAL && consistency_value != CASS_CONSISTENCY_LOCAL_SERIAL) {
        rb_raise(rb_eArgError, "Invalid serial_consistency: %"PRIsVALUE"", consistency);
    }

    GET_CLUSTER(self, cassandra_cluster);
    cass_cluster_set_serial_consistency(cassandra_cluster->cluster, consistency_value);

    return self;
}

/**
 * Sets the number of IO threads that will handle query requests.
 * Default is +1+.
 *
 * @param num_threads [Integer] A number of IO threads.
 * @return [Cassandra::Cluster] self.
 * @raise [ArgumentError] If zero was given.
 * @raise [RangeError] If a negative value was given.
 */
static VALUE cluster_num_threads_io(VALUE self, VALUE num_threads)
{
    CassandraCluster *cassandra_cluster;

    GET_CLUSTER(self, cassandra_cluster);
    cluster_check_error(cass_cluster_set_num_threads_io(cassandra_cluster->cluster, cluster_value_to_uint(num_threads, "num_threads_io")), "num_threads_io");

    return self;
}

/**
 * Sets the size of the fixed size queue that stores pending requests.
 * Default is +8192+.
 *
 * @param queue_size [Integer] A queue size.
 * @return [Cassandra::Cluster] self.
 * @raise [ArgumentError] If zero was given.
 * @raise [RangeError] If a negative value was given.
 */
static VALUE cluster_queue_size_io(VALUE self, VALUE queue_size)
{
    CassandraCluster *cassandra_cluster;

    GET_CLUSTER(self, cassandra_cluster);
    cluster_check_error(cass_cluster_set_queue_size_io(cassandra_cluster->cluster, cluster_value_to_uint(queue_size, "queue_size_io")), "queue_size_io");

    return self;
}

/**
 * Sets the number of connections made to each server in each IO thread.
 * Default is +1+.
 *
 * @param num_connections [Integer] A number of connections.
 * @return [Cassandra::Cluster] self.
 * @raise [ArgumentError] If zero was given.
 * @raise [RangeError] If a negative value was given.
 */
static VALUE cluster_core_connections_per_host(VALUE self, VALUE num_connections)
{
    CassandraCluster *cassandra_cluster;

    GET_CLUSTER(self, cassandra_cluster);
    cluster_check_error(cass_cluster_set_core_connections_per_host(cassandra_cluster->cluster, cluster_value_to_uint(num_connections, "core_connections_per_host")), "core_connections_per_host");

    return self;
}

/**
 * Configures the cluster to use a reconnection policy that waits a constant
 * time between each reconnection attempt.
 * The exponential policy is used unless this method is called.
 *
 * @param delay_ms [Integer] A delay in milliseconds.
 * @return [Cassandra::Cluster] self.
 * @raise [RangeError] If a negative value was given.
 */
static VALUE cluster_constant_reconnect(VALUE self, VALUE delay_ms)
{
    CassandraCluster *cassandra_cluster;

    GET_CLUSTER(self, cassandra_cluster);
    cass_cluster_set_constant_reconnect(cassandra_cluster->cluster, cluster_value_to_uint64(delay_ms, "constant_reconnect"));

    return self;
}

/**
 * Configures the cluster to use a reconnection policy that waits
 * exponentially longer between each reconnection attempt; however
 * will maintain a constant delay once the maximum delay is reached.
 * This is the default policy, with a base delay of +2000+ milliseconds and
 * a maximum delay of +60000+ milliseconds.
 *
 * @param base_delay_ms [Integer] A base delay in milliseconds.
 * @param max_delay_ms [Integer] A maximum delay in milliseconds.
 * @return [Cassandra::Cluster] self.
 * @raise [ArgumentError] If the base delay is 1 or less, the maximum delay is 1 or less,
 *   or the maximum delay is less than the base delay.
 * @raise [RangeError] If a negative value was given.
 */
static VALUE cluster_exponential_reconnect(VALUE self, VALUE base_delay_ms, VALUE max_delay_ms)
{
    CassandraCluster *cassandra_cluster;

    GET_CLUSTER(self, cassandra_cluster);
    cluster_check_error(cass_cluster_set_exponential_reconnect(cassandra_cluster->cluster, cluster_value_to_uint64(base_delay_ms, "exponential_reconnect base_delay_ms"), cluster_value_to_uint64(max_delay_ms, "exponential_reconnect max_delay_ms")), "exponential_reconnect");

    return self;
}

/**
 * Enables/Disables Nagle's algorithm on the connections.
 * Default is +true+ (disables Nagle's algorithm).
 *
 * @param enabled [Boolean] Whether to disable Nagle's algorithm.
 * @return [Cassandra::Cluster] self.
 */
static VALUE cluster_tcp_nodelay(VALUE self, VALUE enabled)
{
    CassandraCluster *cassandra_cluster;

    GET_CLUSTER(self, cassandra_cluster);
    cass_cluster_set_tcp_nodelay(cassandra_cluster->cluster, RTEST(enabled) ? cass_true : cass_false);

    return self;
}

/**
 * Enables/Disables TCP keep-alive.
 * Default is +false+ (disabled).
 *
 * @param enabled [Boolean] Whether to enable TCP keep-alive.
 * @param delay_secs [Integer] The initial delay in seconds. Ignored when disabled.
 * @return [Cassandra::Cluster] self.
 * @raise [RangeError] If a negative value was given.
 */
static VALUE cluster_tcp_keepalive(VALUE self, VALUE enabled, VALUE delay_secs)
{
    CassandraCluster *cassandra_cluster;

    GET_CLUSTER(self, cassandra_cluster);
    cass_cluster_set_tcp_keepalive(cassandra_cluster->cluster, RTEST(enabled) ? cass_true : cass_false, cluster_value_to_uint(delay_secs, "tcp_keepalive delay_secs"));

    return self;
}

/**
 * Sets the amount of time between heartbeat messages and controls the amount
 * of time the connection must be idle before sending heartbeat messages.
 * This is useful for preventing intermediate network devices from dropping connections.
 * Default is +30+ seconds.
 *
 * @param interval_secs [Integer] An interval in seconds. +0+ disables heartbeat messages.
 * @return [Cassandra::Cluster] self.
 * @raise [RangeError] If a negative value was given.
 */
static VALUE cluster_connection_heartbeat_interval(VALUE self, VALUE interval_secs)
{
    CassandraCluster *cassandra_cluster;

    GET_CLUSTER(self, cassandra_cluster);
    cass_cluster_set_connection_heartbeat_interval(cassandra_cluster->cluster, cluster_value_to_uint(interval_secs, "connection_heartbeat_interval"));

    return self;
}

/**
 * Sets the amount of time a connection is allowed to be without a successful
 * heartbeat response before being terminated and scheduled for reconnection.
 * Default is +60+ seconds.
 *
 * @param timeout_secs [Integer] A timeout in seconds.
 * @return [Cassandra::Cluster] self.
 * @raise [RangeError] If a negative value was given.
 */
static VALUE cluster_connection_idle_timeout(VALUE self, VALUE timeout_secs)
{
    CassandraCluster *cassandra_cluster;

    GET_CLUSTER(self, cassandra_cluster);
    cass_cluster_set_connection_idle_timeout(cassandra_cluster->cluster, cluster_value_to_uint(timeout_secs, "connection_idle_timeout"));

    return self;
}

/**
 * Configures the cluster to use round-robin load balancing.
 * The driver discovers all nodes in a cluster and cycles through them per request.
 *
 * @return [Cassandra::Cluster] self.
 */
static VALUE cluster_load_balance_round_robin(VALUE self)
{
    CassandraCluster *cassandra_cluster;

    GET_CLUSTER(self, cassandra_cluster);
    cass_cluster_set_load_balance_round_robin(cassandra_cluster->cluster);

    return self;
}

/**
 * Configures the cluster to use DC-aware load balancing.
 * For each query, all live nodes in a primary 'local' DC are tried first,
 * followed by any node from other DCs.
 *
 * @param local_dc [String] The primary data center to try first.
 * @return [Cassandra::Cluster] self.
 * @raise [ArgumentError] If an invalid data center name was given.
 */
static VALUE cluster_load_balance_dc_aware(VALUE self, VALUE local_dc)
{
    CassandraCluster *cassandra_cluster;

    GET_CLUSTER(self, cassandra_cluster);
    // The 3rd and 4th parameters (used_hosts_per_remote_dc and
    // allow_remote_dcs_for_local_cl) are deprecated in driver 2.16, so they
    // are fixed to 0/cass_false and not exposed to Ruby.
    cluster_check_error(cass_cluster_set_load_balance_dc_aware(cassandra_cluster->cluster, StringValueCStr(local_dc), 0, cass_false), "load_balance_dc_aware");

    return self;
}

/**
 * Configures the cluster to use token-aware request routing or not.
 * Default is +true+ (enabled).
 *
 * @param enabled [Boolean] Whether to enable token-aware routing.
 * @return [Cassandra::Cluster] self.
 */
static VALUE cluster_token_aware_routing(VALUE self, VALUE enabled)
{
    CassandraCluster *cassandra_cluster;

    GET_CLUSTER(self, cassandra_cluster);
    cass_cluster_set_token_aware_routing(cassandra_cluster->cluster, RTEST(enabled) ? cass_true : cass_false);

    return self;
}

/**
 * Configures the cluster to use latency-aware request routing or not.
 * Default is +false+ (disabled).
 *
 * @param enabled [Boolean] Whether to enable latency-aware routing.
 * @return [Cassandra::Cluster] self.
 */
static VALUE cluster_latency_aware_routing(VALUE self, VALUE enabled)
{
    CassandraCluster *cassandra_cluster;

    GET_CLUSTER(self, cassandra_cluster);
    cass_cluster_set_latency_aware_routing(cassandra_cluster->cluster, RTEST(enabled) ? cass_true : cass_false);

    return self;
}

/**
 * Enables/Disables retrieving and updating schema metadata. Disabling this
 * can be useful to improve startup performance, but it means that
 * 'Session#schema_metadata'-like features will not work.
 * Default is +true+ (enabled).
 *
 * @param enabled [Boolean] Whether to keep schema metadata synchronized.
 * @return [Cassandra::Cluster] self.
 */
static VALUE cluster_use_schema(VALUE self, VALUE enabled)
{
    CassandraCluster *cassandra_cluster;

    GET_CLUSTER(self, cassandra_cluster);
    cass_cluster_set_use_schema(cassandra_cluster->cluster, RTEST(enabled) ? cass_true : cass_false);

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
    for (size_t i = 0; i < sizeof(cluster_consistency_symbols) / sizeof(cluster_consistency_symbols[0]); i++) {
        cluster_consistency_symbols[i].id = rb_intern(cluster_consistency_symbols[i].name);
    }

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
    rb_define_method(cCluster, "credentials", cluster_credentials, 2);
    rb_define_method(cCluster, "consistency", cluster_consistency, 1);
    rb_define_method(cCluster, "serial_consistency", cluster_serial_consistency, 1);
    rb_define_method(cCluster, "num_threads_io", cluster_num_threads_io, 1);
    rb_define_method(cCluster, "queue_size_io", cluster_queue_size_io, 1);
    rb_define_method(cCluster, "core_connections_per_host", cluster_core_connections_per_host, 1);
    rb_define_method(cCluster, "constant_reconnect", cluster_constant_reconnect, 1);
    rb_define_method(cCluster, "exponential_reconnect", cluster_exponential_reconnect, 2);
    rb_define_method(cCluster, "tcp_nodelay", cluster_tcp_nodelay, 1);
    rb_define_method(cCluster, "tcp_keepalive", cluster_tcp_keepalive, 2);
    rb_define_method(cCluster, "connection_heartbeat_interval", cluster_connection_heartbeat_interval, 1);
    rb_define_method(cCluster, "connection_idle_timeout", cluster_connection_idle_timeout, 1);
    rb_define_method(cCluster, "load_balance_round_robin", cluster_load_balance_round_robin, 0);
    rb_define_method(cCluster, "load_balance_dc_aware", cluster_load_balance_dc_aware, 1);
    rb_define_method(cCluster, "token_aware_routing", cluster_token_aware_routing, 1);
    rb_define_method(cCluster, "latency_aware_routing", cluster_latency_aware_routing, 1);
    rb_define_method(cCluster, "use_schema", cluster_use_schema, 1);

    rb_define_const(cCluster, "PROTOCOL_VERSION_V1", INT2NUM(CASS_PROTOCOL_VERSION_V1));
    rb_define_const(cCluster, "PROTOCOL_VERSION_V2", INT2NUM(CASS_PROTOCOL_VERSION_V2));
    rb_define_const(cCluster, "PROTOCOL_VERSION_V3", INT2NUM(CASS_PROTOCOL_VERSION_V3));
    rb_define_const(cCluster, "PROTOCOL_VERSION_V4", INT2NUM(CASS_PROTOCOL_VERSION_V4));
    rb_define_const(cCluster, "PROTOCOL_VERSION_V5", INT2NUM(CASS_PROTOCOL_VERSION_V5));
    rb_define_const(cCluster, "PROTOCOL_VERSION_DSEV1", INT2NUM(CASS_PROTOCOL_VERSION_DSEV1));
    rb_define_const(cCluster, "PROTOCOL_VERSION_DSEV2", INT2NUM(CASS_PROTOCOL_VERSION_DSEV2));

    rb_define_const(cCluster, "CONSISTENCY_ANY", INT2NUM(CASS_CONSISTENCY_ANY));
    rb_define_const(cCluster, "CONSISTENCY_ONE", INT2NUM(CASS_CONSISTENCY_ONE));
    rb_define_const(cCluster, "CONSISTENCY_TWO", INT2NUM(CASS_CONSISTENCY_TWO));
    rb_define_const(cCluster, "CONSISTENCY_THREE", INT2NUM(CASS_CONSISTENCY_THREE));
    rb_define_const(cCluster, "CONSISTENCY_QUORUM", INT2NUM(CASS_CONSISTENCY_QUORUM));
    rb_define_const(cCluster, "CONSISTENCY_ALL", INT2NUM(CASS_CONSISTENCY_ALL));
    rb_define_const(cCluster, "CONSISTENCY_LOCAL_QUORUM", INT2NUM(CASS_CONSISTENCY_LOCAL_QUORUM));
    rb_define_const(cCluster, "CONSISTENCY_EACH_QUORUM", INT2NUM(CASS_CONSISTENCY_EACH_QUORUM));
    rb_define_const(cCluster, "CONSISTENCY_SERIAL", INT2NUM(CASS_CONSISTENCY_SERIAL));
    rb_define_const(cCluster, "CONSISTENCY_LOCAL_SERIAL", INT2NUM(CASS_CONSISTENCY_LOCAL_SERIAL));
    rb_define_const(cCluster, "CONSISTENCY_LOCAL_ONE", INT2NUM(CASS_CONSISTENCY_LOCAL_ONE));
}
