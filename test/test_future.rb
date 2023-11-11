# frozen_string_literal: true

require_relative 'helper'

class FutureTest < Minitest::Test
  def test_await
    count = 0

    prepare_future = Ilios::Cassandra.session.prepare_async(<<~CQL)
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

    prepare_future.on_success do |statement|
      futures = []

      50.times do |i|
        statement.bind(
          {
            id: i,
            tinyint: i,
            smallint: i,
            int: i,
            bigint: i,
            float: i,
            double: i,
            boolean: true,
            text: 'hello',
            timestamp: Time.now,
            uuid: SecureRandom.uuid
          }
        )
        result_future = Ilios::Cassandra.session.execute_async(statement)
        result_future.on_success do
          count += 1
        end
        futures << result_future
      end
      futures.each(&:await)
    end
    prepare_future.await

    assert_equal(50, count)
  end
end
