# frozen_string_literal: true

require_relative 'helper'

class ClusterTest < Minitest::Test
  def test_hosts
    cluster = Ilios::Cassandra::Cluster.new

    assert_raises(TypeError) { cluster.hosts(Object.new) }
    assert_raises(TypeError) { cluster.hosts([1]) }
    assert_raises(ArgumentError) { cluster.hosts([]) }
    assert_kind_of(Ilios::Cassandra::Cluster, cluster.hosts([CASSANDRA_HOST]))
  end

  def test_port
    cluster = Ilios::Cassandra::Cluster.new

    assert_raises(TypeError) { cluster.port(Object.new) }
    assert_kind_of(Ilios::Cassandra::Cluster, cluster.port(9042))
  end

  def test_keyspace
    cluster = Ilios::Cassandra::Cluster.new

    assert_raises(TypeError) { cluster.keyspace(Object.new) }
    assert_kind_of(Ilios::Cassandra::Cluster, cluster.keyspace('ilios'))
  end

  def test_protocol_version
    cluster = Ilios::Cassandra::Cluster.new

    assert_raises(TypeError) { cluster.keyspace(Object.new) }
    assert_kind_of(Ilios::Cassandra::Cluster, cluster.protocol_version(Ilios::Cassandra::Cluster::PROTOCOL_VERSION_V4))
  end

  def test_connect_timeout
    cluster = Ilios::Cassandra::Cluster.new

    assert_raises(TypeError) { cluster.connect_timeout(Object.new) }
    assert_kind_of(Ilios::Cassandra::Cluster, cluster.connect_timeout(10_000))
  end

  def test_request_timeout
    cluster = Ilios::Cassandra::Cluster.new

    assert_raises(TypeError) { cluster.request_timeout(Object.new) }
    assert_kind_of(Ilios::Cassandra::Cluster, cluster.request_timeout(10_000))
  end

  def test_resolve_timeout
    cluster = Ilios::Cassandra::Cluster.new

    assert_raises(TypeError) { cluster.resolve_timeout(Object.new) }
    assert_kind_of(Ilios::Cassandra::Cluster, cluster.resolve_timeout(10_000))
  end

  def test_constant_speculative_execution_policy
    cluster = Ilios::Cassandra::Cluster.new

    assert_raises(TypeError) { cluster.constant_speculative_execution_policy(Object.new, Object.new) }
    assert_raises(ArgumentError) { cluster.constant_speculative_execution_policy(-1, 2) }
    assert_raises(ArgumentError) { cluster.constant_speculative_execution_policy(10_000, -1) }
    assert_kind_of(Ilios::Cassandra::Cluster, cluster.constant_speculative_execution_policy(10_000, 2))
  end

  def test_connect
    cluster = Ilios::Cassandra::Cluster.new

    assert_raises(Ilios::Cassandra::ConnectError) { cluster.connect }

    cluster.hosts([CASSANDRA_HOST])

    assert_kind_of(Ilios::Cassandra::Session, cluster.connect)
  end
end
