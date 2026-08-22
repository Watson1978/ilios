# frozen_string_literal: true

require_relative 'helper'

class ClusterTest < Minitest::Test
  def test_uninitialized_cluster
    assert_raises(RuntimeError) { Ilios::Cassandra::Cluster.allocate.port(9042) }
    assert_raises(RuntimeError) { Ilios::Cassandra::Cluster.allocate.hosts([CASSANDRA_HOST]) }
    assert_raises(RuntimeError) { Ilios::Cassandra::Cluster.allocate.keyspace('ilios') }
    assert_raises(RuntimeError) { Ilios::Cassandra::Cluster.allocate.connect }
    assert_raises(RuntimeError) { Ilios::Cassandra::Cluster.new.dup.port(9042) }
    assert_raises(RuntimeError) { Ilios::Cassandra::Cluster.new.clone.port(9042) }
  end

  def test_hosts
    cluster = Ilios::Cassandra::Cluster.new

    assert_raises(TypeError) { cluster.hosts(Object.new) }
    assert_raises(TypeError) { cluster.hosts([1]) }
    assert_raises(ArgumentError) { cluster.hosts([]) }
    assert_kind_of(Ilios::Cassandra::Cluster, cluster.hosts([CASSANDRA_HOST]))
  end

  def test_hosts_element_growing_the_array
    grower = Class.new do
      def initialize(array)
        @array = array
      end

      def to_str
        @array << self.class.new(@array) if @array.size < 100
        CASSANDRA_HOST
      end
    end

    array = []
    array << grower.new(array)

    cluster = Ilios::Cassandra::Cluster.new

    assert_kind_of(Ilios::Cassandra::Cluster, cluster.hosts(array))
    assert_equal(2, array.size)
  end

  def test_port
    cluster = Ilios::Cassandra::Cluster.new

    assert_raises(TypeError) { cluster.port(Object.new) }
    assert_raises(ArgumentError) { cluster.port(-1) }
    assert_raises(ArgumentError) { cluster.port(0) }
    assert_kind_of(Ilios::Cassandra::Cluster, cluster.port(9042))
  end

  def test_keyspace
    cluster = Ilios::Cassandra::Cluster.new

    assert_raises(TypeError) { cluster.keyspace(Object.new) }
    assert_kind_of(Ilios::Cassandra::Cluster, cluster.keyspace('ilios'))
  end

  def test_protocol_version
    cluster = Ilios::Cassandra::Cluster.new

    assert_raises(TypeError) { cluster.protocol_version(Object.new) }
    assert_raises(RangeError) { cluster.protocol_version(-1) }
    assert_kind_of(Ilios::Cassandra::Cluster, cluster.protocol_version(Ilios::Cassandra::Cluster::PROTOCOL_VERSION_V3))
    assert_kind_of(Ilios::Cassandra::Cluster, cluster.protocol_version(Ilios::Cassandra::Cluster::PROTOCOL_VERSION_V4))
    assert_kind_of(Ilios::Cassandra::Cluster, cluster.protocol_version(Ilios::Cassandra::Cluster::PROTOCOL_VERSION_DSEV1))
    assert_kind_of(Ilios::Cassandra::Cluster, cluster.protocol_version(Ilios::Cassandra::Cluster::PROTOCOL_VERSION_DSEV2))
  end

  def test_connect_timeout
    cluster = Ilios::Cassandra::Cluster.new

    assert_raises(TypeError) { cluster.connect_timeout(Object.new) }
    assert_kind_of(Ilios::Cassandra::Cluster, cluster.connect_timeout(10_000))
    assert_raises(RangeError) { cluster.connect_timeout(-1) }
  end

  def test_request_timeout
    cluster = Ilios::Cassandra::Cluster.new

    assert_raises(TypeError) { cluster.request_timeout(Object.new) }
    assert_kind_of(Ilios::Cassandra::Cluster, cluster.request_timeout(10_000))
    assert_raises(RangeError) { cluster.request_timeout(-1) }
  end

  def test_resolve_timeout
    cluster = Ilios::Cassandra::Cluster.new

    assert_raises(TypeError) { cluster.resolve_timeout(Object.new) }
    assert_kind_of(Ilios::Cassandra::Cluster, cluster.resolve_timeout(10_000))
    assert_raises(RangeError) { cluster.resolve_timeout(-1) }
  end

  def test_constant_speculative_execution_policy
    cluster = Ilios::Cassandra::Cluster.new

    assert_raises(TypeError) { cluster.constant_speculative_execution_policy(Object.new, Object.new) }
    assert_raises(ArgumentError) { cluster.constant_speculative_execution_policy(-1, 2) }
    assert_raises(ArgumentError) { cluster.constant_speculative_execution_policy(10_000, -1) }
    assert_kind_of(Ilios::Cassandra::Cluster, cluster.constant_speculative_execution_policy(10_000, 2))
  end

  def test_credentials
    cluster = Ilios::Cassandra::Cluster.new

    assert_raises(TypeError) { cluster.credentials(Object.new, 'password') }
    assert_raises(TypeError) { cluster.credentials('username', Object.new) }
    assert_kind_of(Ilios::Cassandra::Cluster, cluster.credentials('username', 'password'))

    # Credentials are ignored by a node using AllowAllAuthenticator;
    # setting them must not break the connection.
    cluster.hosts([CASSANDRA_HOST])

    assert_kind_of(Ilios::Cassandra::Session, cluster.connect)
  end

  def test_consistency
    cluster = Ilios::Cassandra::Cluster.new

    assert_raises(TypeError) { cluster.consistency(Object.new) }
    assert_raises(ArgumentError) { cluster.consistency(0xFFFF) }
    assert_raises(ArgumentError) { cluster.consistency(-1) }
    assert_raises(ArgumentError) { cluster.consistency(99) }
    assert_kind_of(Ilios::Cassandra::Cluster, cluster.consistency(Ilios::Cassandra::Cluster::CONSISTENCY_QUORUM))
    assert_raises(ArgumentError) { cluster.consistency(:foo) }
    assert_kind_of(Ilios::Cassandra::Cluster, cluster.consistency(:quorum))
  end

  def test_serial_consistency
    cluster = Ilios::Cassandra::Cluster.new

    assert_raises(TypeError) { cluster.serial_consistency(Object.new) }
    assert_raises(ArgumentError) { cluster.serial_consistency(0xFFFF) }
    assert_raises(ArgumentError) { cluster.serial_consistency(-1) }
    assert_raises(ArgumentError) { cluster.serial_consistency(99) }
    assert_raises(ArgumentError) { cluster.serial_consistency(:foo) }
    # Cassandra only accepts SERIAL / LOCAL_SERIAL as serial consistency
    # levels; anything else is rejected even though it is otherwise a
    # valid consistency level.
    assert_raises(ArgumentError) { cluster.serial_consistency(Ilios::Cassandra::Cluster::CONSISTENCY_QUORUM) }
    assert_raises(ArgumentError) { cluster.serial_consistency(:quorum) }
    assert_kind_of(Ilios::Cassandra::Cluster, cluster.serial_consistency(Ilios::Cassandra::Cluster::CONSISTENCY_SERIAL))
    assert_kind_of(Ilios::Cassandra::Cluster, cluster.serial_consistency(Ilios::Cassandra::Cluster::CONSISTENCY_LOCAL_SERIAL))
    assert_kind_of(Ilios::Cassandra::Cluster, cluster.serial_consistency(:serial))
    assert_kind_of(Ilios::Cassandra::Cluster, cluster.serial_consistency(:local_serial))
  end

  def test_speculative_execution_policy_converts_the_values_once
    counter = Class.new do
      attr_reader :calls

      def initialize
        @calls = 0
      end

      def to_int
        @calls += 1
        @calls == 1 ? 5 : -1
      end
    end

    cluster = Ilios::Cassandra::Cluster.new

    delay = counter.new
    cluster.constant_speculative_execution_policy(delay, 1)

    assert_equal(1, delay.calls)

    executions = counter.new
    cluster.constant_speculative_execution_policy(1, executions)

    assert_equal(1, executions.calls)
  end

  def test_integer_option_converts_the_value_once
    counter = Class.new do
      attr_reader :calls

      def initialize
        @calls = 0
      end

      def to_int
        @calls += 1
        @calls == 1 ? 5 : -1
      end
    end

    cluster = Ilios::Cassandra::Cluster.new

    heartbeat = counter.new
    cluster.connection_heartbeat_interval(heartbeat)

    assert_equal(1, heartbeat.calls)

    reconnect = counter.new
    cluster.constant_reconnect(reconnect)

    assert_equal(1, reconnect.calls)
  end

  def test_consistency_does_not_pin_unknown_symbols
    cluster = Ilios::Cassandra::Cluster.new

    GC.start
    before = Symbol.all_symbols.size
    1000.times do |i|
      cluster.consistency(:"unknown_consistency_#{i}")
    rescue ArgumentError
      nil
    end
    GC.start

    assert_operator(Symbol.all_symbols.size - before, :<, 100)
  end

  def test_connect_with_correct_credentials
    skip('auth-enabled Cassandra is not available') unless CASSANDRA_AUTH_AVAILABLE

    cluster = Ilios::Cassandra::Cluster.new
    cluster.hosts([CASSANDRA_AUTH_HOST])
    cluster.port(CASSANDRA_AUTH_PORT)
    cluster.credentials('cassandra', 'cassandra')

    assert_kind_of(Ilios::Cassandra::Session, cluster.connect)
  end

  def test_connect_with_wrong_credentials
    skip('auth-enabled Cassandra is not available') unless CASSANDRA_AUTH_AVAILABLE

    cluster = Ilios::Cassandra::Cluster.new
    cluster.hosts([CASSANDRA_AUTH_HOST])
    cluster.port(CASSANDRA_AUTH_PORT)
    cluster.credentials('cassandra', 'wrong-password')

    assert_raises(Ilios::Cassandra::ConnectError) { cluster.connect }
  end

  def test_connect_without_credentials_to_auth_node
    skip('auth-enabled Cassandra is not available') unless CASSANDRA_AUTH_AVAILABLE

    cluster = Ilios::Cassandra::Cluster.new
    cluster.hosts([CASSANDRA_AUTH_HOST])
    cluster.port(CASSANDRA_AUTH_PORT)

    assert_raises(Ilios::Cassandra::ConnectError) { cluster.connect }
  end

  def test_connect
    cluster = Ilios::Cassandra::Cluster.new

    assert_raises(Ilios::Cassandra::ConnectError) { cluster.connect }

    cluster.hosts([CASSANDRA_HOST])

    assert_kind_of(Ilios::Cassandra::Session, cluster.connect)
  end

  def test_num_threads_io
    cluster = Ilios::Cassandra::Cluster.new

    assert_raises(TypeError) { cluster.num_threads_io(Object.new) }
    assert_raises(RangeError) { cluster.num_threads_io(-1) }
    assert_raises(ArgumentError) { cluster.num_threads_io(0) }
    assert_kind_of(Ilios::Cassandra::Cluster, cluster.num_threads_io(4))
  end

  def test_queue_size_io
    cluster = Ilios::Cassandra::Cluster.new

    assert_raises(TypeError) { cluster.queue_size_io(Object.new) }
    assert_raises(RangeError) { cluster.queue_size_io(-1) }
    assert_raises(ArgumentError) { cluster.queue_size_io(0) }
    assert_kind_of(Ilios::Cassandra::Cluster, cluster.queue_size_io(8192))
  end

  def test_core_connections_per_host
    cluster = Ilios::Cassandra::Cluster.new

    assert_raises(TypeError) { cluster.core_connections_per_host(Object.new) }
    assert_raises(RangeError) { cluster.core_connections_per_host(-1) }
    assert_raises(ArgumentError) { cluster.core_connections_per_host(0) }
    assert_kind_of(Ilios::Cassandra::Cluster, cluster.core_connections_per_host(2))
  end

  def test_constant_reconnect
    cluster = Ilios::Cassandra::Cluster.new

    assert_raises(TypeError) { cluster.constant_reconnect(Object.new) }
    assert_raises(RangeError) { cluster.constant_reconnect(-1) }
    assert_kind_of(Ilios::Cassandra::Cluster, cluster.constant_reconnect(2000))
  end

  def test_exponential_reconnect
    cluster = Ilios::Cassandra::Cluster.new

    assert_raises(TypeError) { cluster.exponential_reconnect(Object.new, 60_000) }
    assert_raises(RangeError) { cluster.exponential_reconnect(-1, 60_000) }
    # the driver rejects base_delay_ms <= 1 and max_delay_ms < base_delay_ms
    assert_raises(ArgumentError) { cluster.exponential_reconnect(1, 60_000) }
    assert_raises(ArgumentError) { cluster.exponential_reconnect(60_000, 2000) }
    assert_kind_of(Ilios::Cassandra::Cluster, cluster.exponential_reconnect(2000, 60_000))
  end

  def test_tcp_nodelay
    cluster = Ilios::Cassandra::Cluster.new

    assert_kind_of(Ilios::Cassandra::Cluster, cluster.tcp_nodelay(true))
    assert_kind_of(Ilios::Cassandra::Cluster, cluster.tcp_nodelay(false))
  end

  def test_tcp_keepalive
    cluster = Ilios::Cassandra::Cluster.new

    assert_raises(TypeError) { cluster.tcp_keepalive(true, Object.new) }
    assert_raises(RangeError) { cluster.tcp_keepalive(true, -1) }
    assert_kind_of(Ilios::Cassandra::Cluster, cluster.tcp_keepalive(true, 60))
    assert_kind_of(Ilios::Cassandra::Cluster, cluster.tcp_keepalive(false, 0))
  end

  def test_connection_heartbeat_interval
    cluster = Ilios::Cassandra::Cluster.new

    assert_raises(TypeError) { cluster.connection_heartbeat_interval(Object.new) }
    assert_raises(RangeError) { cluster.connection_heartbeat_interval(-1) }
    assert_kind_of(Ilios::Cassandra::Cluster, cluster.connection_heartbeat_interval(30))
  end

  def test_connection_idle_timeout
    cluster = Ilios::Cassandra::Cluster.new

    assert_raises(TypeError) { cluster.connection_idle_timeout(Object.new) }
    assert_raises(RangeError) { cluster.connection_idle_timeout(-1) }
    assert_kind_of(Ilios::Cassandra::Cluster, cluster.connection_idle_timeout(60))
  end

  def test_load_balance_round_robin
    cluster = Ilios::Cassandra::Cluster.new

    assert_kind_of(Ilios::Cassandra::Cluster, cluster.load_balance_round_robin)
  end

  def test_load_balance_dc_aware
    cluster = Ilios::Cassandra::Cluster.new

    assert_raises(TypeError) { cluster.load_balance_dc_aware(Object.new) }
    assert_kind_of(Ilios::Cassandra::Cluster, cluster.load_balance_dc_aware('dc1'))
  end

  def test_token_aware_routing
    cluster = Ilios::Cassandra::Cluster.new

    assert_kind_of(Ilios::Cassandra::Cluster, cluster.token_aware_routing(true))
    assert_kind_of(Ilios::Cassandra::Cluster, cluster.token_aware_routing(false))
  end

  def test_latency_aware_routing
    cluster = Ilios::Cassandra::Cluster.new

    assert_kind_of(Ilios::Cassandra::Cluster, cluster.latency_aware_routing(true))
    assert_kind_of(Ilios::Cassandra::Cluster, cluster.latency_aware_routing(false))
  end

  def test_use_schema
    cluster = Ilios::Cassandra::Cluster.new

    assert_kind_of(Ilios::Cassandra::Cluster, cluster.use_schema(false))
    assert_kind_of(Ilios::Cassandra::Cluster, cluster.use_schema(true))
  end

  def test_options_combined_connect
    cluster = Ilios::Cassandra::Cluster.new
    cluster.hosts([CASSANDRA_HOST])
    cluster.consistency(Ilios::Cassandra::Cluster::CONSISTENCY_QUORUM)
    cluster.num_threads_io(2)
    cluster.queue_size_io(8192)
    cluster.core_connections_per_host(1)
    cluster.exponential_reconnect(2000, 60_000)
    cluster.tcp_nodelay(true)
    cluster.tcp_keepalive(true, 60)
    cluster.connection_heartbeat_interval(30)
    cluster.connection_idle_timeout(60)
    cluster.token_aware_routing(true)
    cluster.latency_aware_routing(false)
    cluster.use_schema(true)

    assert_kind_of(Ilios::Cassandra::Session, cluster.connect)
  end
end
