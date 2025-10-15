# frozen_string_literal: true

require_relative 'helper'

class IliosTest < Minitest::Test
  def test_ractor
    r = Ractor.new do
      cluster = Ilios::Cassandra::Cluster.new
      cluster.keyspace('ilios')
      cluster.hosts([CASSANDRA_HOST])
      session = cluster.connect

      statement = Ilios::Cassandra.session.prepare(<<~CQL)
        INSERT INTO ilios.test (
          id,
          tinyint,
          smallint,
          int,
          bigint,
          float,
          double,
          boolean,
          text,
          timestamp,
          uuid
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
      CQL

      statement.bind(
        {
          id: 1,
          tinyint: 2,
          smallint: 3,
          int: 4,
          bigint: 5,
          float: 6.78,
          double: 9.10,
          boolean: true,
          text: 'hello world',
          timestamp: Time.now
        }
      )
      session.execute(statement)
    end

    result =
      if r.respond_to?(:take)
        r.take
      else
        r.join
      end

    assert_kind_of(Ilios::Cassandra::Result, result)
  end
end
