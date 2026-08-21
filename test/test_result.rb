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

  def test_each_with_short_value
    # setup
    statement = Ilios::Cassandra.session.prepare(<<~CQL)
      CREATE TABLE IF NOT EXISTS ilios.short_value (id bigint, int int, PRIMARY KEY (id));
    CQL
    Ilios::Cassandra.session.execute(statement)

    # Cassandra accepts a zero-length value for a fixed-width column, and it is
    # not null. The driver cannot decode it, so the value must not be returned.
    statement = Ilios::Cassandra.session.prepare(<<~CQL)
      INSERT INTO ilios.short_value (id, int) VALUES (?, blobAsInt(textAsBlob('')));
    CQL
    statement.bind(id: 1)
    Ilios::Cassandra.session.execute(statement)

    statement = Ilios::Cassandra.session.prepare(<<~CQL)
      SELECT * FROM ilios.short_value;
    CQL
    results = Ilios::Cassandra.session.execute(statement)

    # the driver logs the decoding failure at ERROR level
    Ilios::Cassandra.log_level(Ilios::Cassandra::LOG_DISABLED)
    begin
      assert_raises(Ilios::Cassandra::ExecutionError) { results.to_a }
    ensure
      # rubocop:disable Style/GlobalVars
      Ilios::Cassandra.log_level($default_test_log_level)
      # rubocop:enable Style/GlobalVars
    end

    # teardown
    statement = Ilios::Cassandra.session.prepare(<<~CQL)
      DROP TABLE ilios.short_value;
    CQL
    Ilios::Cassandra.session.execute(statement)
  end

  def test_each_with_collections
    statement = Ilios::Cassandra.session.prepare(<<~CQL)
      INSERT INTO ilios.test (id, list, "set", map, nested_list, nested_map)
      VALUES (?, [1, 2, 3], {'a', 'b'}, {'k1': 1, 'k2': 2}, [[1, 2], [3]], {'x': {1, 2}});
    CQL
    id = Random.rand(2**60)
    statement.bind(id: id)
    Ilios::Cassandra.session.execute(statement)

    select_statement = Ilios::Cassandra.session.prepare(<<~CQL)
      SELECT * FROM ilios.test WHERE id = ?;
    CQL
    select_statement.bind(id: id)
    row = Ilios::Cassandra.session.execute(select_statement).first

    # rubocop:disable Style/StringHashKeys
    assert_equal([1, 2, 3], row['list'])
    assert_equal(Set['a', 'b'], row['set'])
    assert_equal({ 'k1' => 1, 'k2' => 2 }, row['map'])
    assert_equal([[1, 2], [3]], row['nested_list'])
    assert_equal({ 'x' => Set[1, 2] }, row['nested_map'])
    # rubocop:enable Style/StringHashKeys
  end

  def test_each_with_null_collections
    statement = Ilios::Cassandra.session.prepare(<<~CQL)
      INSERT INTO ilios.test (id) VALUES (?);
    CQL
    id = Random.rand(2**60)
    statement.bind(id: id)
    Ilios::Cassandra.session.execute(statement)

    select_statement = Ilios::Cassandra.session.prepare(<<~CQL)
      SELECT * FROM ilios.test WHERE id = ?;
    CQL
    select_statement.bind(id: id)
    row = Ilios::Cassandra.session.execute(select_statement).first

    assert_nil(row['list'])
    assert_nil(row['set'])
    assert_nil(row['map'])
  end
end
