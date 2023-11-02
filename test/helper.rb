# frozen_string_literal: true

require 'date'
require 'ilios'
require 'minitest/autorun'
require 'securerandom'

$prepared_keyspace_and_table ||= false

def prepare_keyspace
  Ilios::Cassandra.config = {
    keyspace: '',
    hosts: ['127.0.0.1']
  }

  session = Ilios::Cassandra.connect
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
  Ilios::Cassandra.config = {
    keyspace: 'ilios',
    hosts: ['127.0.0.1']
  }

  session = Ilios::Cassandra.connect
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

unless $prepared_keyspace_and_table
  prepare_keyspace
  prepare_table

  $prepared_keyspace_and_table = true
end

Ilios::Cassandra.config = {
  keyspace: 'ilios',
  hosts: ['127.0.0.1']
}
