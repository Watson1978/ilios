# frozen_string_literal: true

require 'date'
require 'ilios'
require 'minitest/autorun'
require 'securerandom'

CASSANDRA_HOST = ENV['CASSANDRA_HOST'] || '127.0.0.1'

module Ilios
  module Cassandra
    def self.session
      Thread.current[:ilios_cassandra_session] ||= begin
        cluster = Ilios::Cassandra::Cluster.new
        cluster.keyspace('ilios')
        cluster.hosts([CASSANDRA_HOST])
        cluster.connect
      end
    end
  end
end

def prepare_keyspace
  cluster = Ilios::Cassandra::Cluster.new
  cluster.hosts([CASSANDRA_HOST])

  session = cluster.connect
  statement = session.prepare(<<~CQL)
    CREATE KEYSPACE IF NOT EXISTS ilios
    WITH REPLICATION = {
      'class' : 'SimpleStrategy',
      'replication_factor' : 1
    };
  CQL
  session.execute(statement)
end

def prepare_table
  cluster = Ilios::Cassandra::Cluster.new
  cluster.keyspace('ilios')
  cluster.hosts([CASSANDRA_HOST])

  session = cluster.connect
  statement = session.prepare(<<~CQL)
    CREATE TABLE IF NOT EXISTS ilios.test (
      id bigint,

      tinyint tinyint,
      smallint smallint,
      int int,
      bigint bigint,
      float float,
      double double,
      boolean boolean,
      text text,
      timestamp timestamp,
      uuid uuid,
      PRIMARY KEY (id)
    ) WITH compaction = { 'class' : 'LeveledCompactionStrategy' }
    AND gc_grace_seconds = 691200;
  CQL
  session.execute(statement)
end

prepare_keyspace
prepare_table
