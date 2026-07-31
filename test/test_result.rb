# frozen_string_literal: true

require_relative 'helper'

class ResultTest < Minitest::Test
  def setup
    @insert_statement = Ilios::Cassandra.session.prepare(<<~CQL)
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
  end

  def test_each
    # setup
    3.times do |i|
      @insert_statement.bind(
        {
          id: i,
          tinyint: i,
          smallint: i,
          int: i,
          bigint: i,
          float: i,
          double: i,
          boolean: true,
          text: "hello #{i}",
          timestamp: Time.now,
          uuid: SecureRandom.uuid
        }
      )
      Ilios::Cassandra.session.execute(@insert_statement)
    end

    statement = Ilios::Cassandra.session.prepare(<<~CQL)
      SELECT * FROM ilios.test WHERE id IN (0, 1, 2);
    CQL
    results = Ilios::Cassandra.session.execute(statement)

    results.each.with_index do |row, index|
      assert_kind_of(Hash, row)
      assert_equal(index, row['id'])
      assert_equal(index, row['tinyint'])
      assert_equal(index, row['smallint'])
      assert_equal(index, row['int'])
      assert_equal(index, row['bigint'])
      assert_equal(index, row['float'])
      assert_equal(index, row['double'])
      assert(row['boolean'])
      assert_equal("hello #{index}", row['text'])
      assert_kind_of(Time, row['timestamp'])
      assert_kind_of(String, row['uuid'])
    end

    assert_kind_of(Enumerator, results.each)
  end

  def test_next_page
    # setup
    10.times do |i|
      @insert_statement.bind(
        {
          id: i,
          tinyint: i,
          smallint: i,
          int: i,
          bigint: i,
          float: i,
          double: i,
          boolean: true,
          text: "hello #{i}",
          timestamp: Time.now,
          uuid: SecureRandom.uuid
        }
      )
      Ilios::Cassandra.session.execute(@insert_statement)
    end

    # specify page_size
    statement = Ilios::Cassandra.session.prepare(<<~CQL)
      SELECT * FROM ilios.test;
    CQL
    statement.page_size = 5

    results = Ilios::Cassandra.session.execute(statement)

    assert_equal(5, results.to_a.size)
    assert_kind_of(Ilios::Cassandra::Result, results.next_page)
    assert_equal(5, results.to_a.size)

    statement = Ilios::Cassandra.session.prepare(<<~CQL)
      SELECT * FROM ilios.test WHERE id = -1;
    CQL
    statement.page_size = 5

    results = Ilios::Cassandra.session.execute(statement)

    assert_equal(0, results.to_a.size)
    assert_nil(results.next_page) # no more pages
  end

  def test_next_page_failure
    # setup
    statement = Ilios::Cassandra.session.prepare(<<~CQL)
      CREATE TABLE IF NOT EXISTS ilios.paging_failure (id bigint, PRIMARY KEY (id));
    CQL
    Ilios::Cassandra.session.execute(statement)

    insert_statement = Ilios::Cassandra.session.prepare(<<~CQL)
      INSERT INTO ilios.paging_failure (id) VALUES (?);
    CQL
    10.times do |i|
      insert_statement.bind(id: i)
      Ilios::Cassandra.session.execute(insert_statement)
    end

    statement = Ilios::Cassandra.session.prepare(<<~CQL)
      SELECT * FROM ilios.paging_failure;
    CQL
    statement.page_size = 5

    results = Ilios::Cassandra.session.execute(statement)

    assert_equal(5, results.to_a.size)

    # dropping the table makes the paging query fail
    statement = Ilios::Cassandra.session.prepare(<<~CQL)
      DROP TABLE ilios.paging_failure;
    CQL
    Ilios::Cassandra.session.execute(statement)

    assert_raises(Ilios::Cassandra::ExecutionError) { results.next_page }

    # the page fetched before the failure is still readable
    assert_equal(5, results.to_a.size)
  end
end
