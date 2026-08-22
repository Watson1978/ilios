# frozen_string_literal: true

require_relative 'helper'

class StatementTest < Minitest::Test
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
        uuid,
        list,
        "set",
        map,
        nested_list,
        nested_map,
        map_uuid_boolean,
        list_timestamp
      ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
    CQL
  end

  def test_bind
    # invalid value
    assert_raises(TypeError) { @insert_statement.bind(Object.new) }
    assert_raises(TypeError) { @insert_statement.bind({ 1 => 123 }) }
    assert_raises(Ilios::Cassandra::StatementError) { @insert_statement.bind({ foo: 123 }) }

    # valid values
    # rubocop:disable Style/StringHashKeys
    assert_kind_of(Ilios::Cassandra::Statement, @insert_statement.bind({ 'id' => 1 }))
    # rubocop:enable Style/StringHashKeys
    assert_kind_of(Ilios::Cassandra::Statement, @insert_statement.bind({ id: 1 }))
    assert_kind_of(
      Ilios::Cassandra::Statement,
      @insert_statement.bind(
        {
          id: 1,
          tinyint: 1,
          smallint: 1,
          int: 1,
          bigint: 1,
          float: 1,
          double: 1,
          boolean: true,
          text: 'hello',
          timestamp: Time.now,
          uuid: SecureRandom.uuid
        }
      )
    )

    key = Object.new
    def key.to_str
      'id'
    end

    assert_kind_of(Ilios::Cassandra::Statement, @insert_statement.bind({ key => 1 }))
  end

  def test_bind_null
    @insert_statement.bind(tinyint: 123)
    @insert_statement.bind(tinyint: nil)

    results = insert_and_get_results

    assert_nil(results.first['tinyint'])
  end

  def test_bind_tinyint
    # invalid value
    assert_raises(RangeError) { @insert_statement.bind(tinyint: -2**7 - 1) }
    assert_raises(RangeError) { @insert_statement.bind(tinyint: 2**7) }
    assert_raises(RangeError) { @insert_statement.bind(tinyint: 2**63) } # bignum
    assert_raises(TypeError) { @insert_statement.bind(tinyint: Object.new) }

    # valid values
    assert_kind_of(Ilios::Cassandra::Statement, @insert_statement.bind(tinyint: -2**7))
    assert_kind_of(Ilios::Cassandra::Statement, @insert_statement.bind(tinyint: (2**7) - 1))

    results = insert_and_get_results

    assert_equal((2**7) - 1, results.first['tinyint'])
  end

  def test_bind_smallint
    # invalid value
    assert_raises(RangeError) { @insert_statement.bind(smallint: -2**15 - 1) }
    assert_raises(RangeError) { @insert_statement.bind(smallint: 2**15) }
    assert_raises(RangeError) { @insert_statement.bind(smallint: 2**63) } # bignum
    assert_raises(TypeError) { @insert_statement.bind(smallint: Object.new) }

    # valid values
    assert_kind_of(Ilios::Cassandra::Statement, @insert_statement.bind(smallint: -2**15))
    assert_kind_of(Ilios::Cassandra::Statement, @insert_statement.bind(smallint: (2**15) - 1))

    results = insert_and_get_results

    assert_equal((2**15) - 1, results.first['smallint'])
  end

  def test_bind_int
    # invalid value
    assert_raises(RangeError) { @insert_statement.bind(int: -2**31 - 1) }
    assert_raises(RangeError) { @insert_statement.bind(int: 2**31) }
    assert_raises(RangeError) { @insert_statement.bind(int: 2**63) } # bignum
    assert_raises(TypeError) { @insert_statement.bind(int: Object.new) }

    # valid values
    assert_kind_of(Ilios::Cassandra::Statement, @insert_statement.bind(int: -2**31))
    assert_kind_of(Ilios::Cassandra::Statement, @insert_statement.bind(int: (2**31) - 1))

    results = insert_and_get_results

    assert_equal((2**31) - 1, results.first['int'])
  end

  def test_bind_bigint
    # invalid value
    assert_raises(RangeError) { @insert_statement.bind(bigint: 2**63) } # bignum
    assert_raises(TypeError) { @insert_statement.bind(bigint: Object.new) }

    # valid values
    assert_kind_of(Ilios::Cassandra::Statement, @insert_statement.bind(bigint: -2**62))
    assert_kind_of(Ilios::Cassandra::Statement, @insert_statement.bind(bigint: 2**62))

    results = insert_and_get_results

    assert_equal(2**62, results.first['bigint'])
  end

  def test_bind_float
    # invalid value
    assert_raises(RangeError) { @insert_statement.bind(float: -3.402820018375656e+39) }
    assert_raises(RangeError) { @insert_statement.bind(float: 3.402820018375656e+39) }
    assert_raises(TypeError) { @insert_statement.bind(float: Object.new) }

    # valid values
    assert_kind_of(Ilios::Cassandra::Statement, @insert_statement.bind(float: -3.402820018375656e+38))
    assert_kind_of(Ilios::Cassandra::Statement, @insert_statement.bind(float: 3.402820018375656e+38))

    results = insert_and_get_results

    assert_in_delta(3.402820018375656e+38, results.first['float'])
  end

  def test_bind_double
    # invalid value
    assert_raises(TypeError) { @insert_statement.bind(double: Object.new) }

    # valid values
    assert_kind_of(Ilios::Cassandra::Statement, @insert_statement.bind(double: -1.79769313486232e+307))
    assert_kind_of(Ilios::Cassandra::Statement, @insert_statement.bind(double: 1.79769313486232e+307))

    results = insert_and_get_results

    assert_in_delta(1.79769313486232e+307, results.first['double'])
  end

  def test_bind_boolean
    # valid values for true
    assert_kind_of(Ilios::Cassandra::Statement, @insert_statement.bind(boolean: true))

    results = insert_and_get_results

    assert(results.first['boolean'])

    # valid values for false
    assert_kind_of(Ilios::Cassandra::Statement, @insert_statement.bind(boolean: false))

    results = insert_and_get_results

    refute(results.first['boolean'])
  end

  def test_bind_text
    # invalid value
    assert_raises(TypeError) { @insert_statement.bind(text: Object.new) }

    # valid values
    assert_kind_of(Ilios::Cassandra::Statement, @insert_statement.bind(text: 'hello'))

    results = insert_and_get_results

    assert_equal('hello', results.first['text'])

    # valid values
    obj = Object.new
    def obj.to_str
      'hello'
    end

    assert_kind_of(Ilios::Cassandra::Statement, @insert_statement.bind(text: obj))

    results = insert_and_get_results

    assert_equal('hello', results.first['text'])
  end

  def test_bind_text_with_symbol
    assert_kind_of(Ilios::Cassandra::Statement, @insert_statement.bind(text: :hello))

    results = insert_and_get_results

    assert_equal('hello', results.first['text'])
  end

  def test_bind_timestamp
    # invalid value
    assert_raises(TypeError) { @insert_statement.bind(timestamp: Object.new) }

    # valid values
    time = Time.now.ceil

    assert_kind_of(Ilios::Cassandra::Statement, @insert_statement.bind(timestamp: time))

    results = insert_and_get_results

    assert_equal(time, results.first['timestamp'])

    # valid values
    date_time = DateTime.parse('2023-11-01T12:30:45')

    assert_kind_of(Ilios::Cassandra::Statement, @insert_statement.bind(timestamp: date_time))

    results = insert_and_get_results

    assert_equal(date_time.to_time, results.first['timestamp'])
  end

  def test_bind_uuid
    # invalid value
    assert_raises(TypeError) { @insert_statement.bind(uuid: Object.new) }
    assert_raises(Ilios::Cassandra::StatementError) { @insert_statement.bind(uuid: 'x') }
    assert_raises(Ilios::Cassandra::StatementError) { @insert_statement.bind(uuid: '') }
    assert_raises(Ilios::Cassandra::StatementError) do
      # 36 characters, but 'z' is not a hex digit
      @insert_statement.bind(uuid: 'zzzzzzzz-0000-0000-0000-000000000000')
    end

    # valid values
    uuid = SecureRandom.uuid

    assert_kind_of(Ilios::Cassandra::Statement, @insert_statement.bind(uuid: uuid))

    results = insert_and_get_results

    assert_equal(uuid, results.first['uuid'])
  end

  def test_page_size
    # invalid value
    assert_raises(TypeError) { @insert_statement.page_size = Object.new }

    # setup
    10.times do
      @insert_statement.bind(
        {
          id: Random.rand(2**60),
          tinyint: 1,
          smallint: 1,
          int: 1,
          bigint: 1,
          float: 1,
          double: 1,
          boolean: true,
          text: 'hello',
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
  end

  def test_idempotent
    assert_respond_to(@insert_statement, :idempotent=)
  end

  def test_bind_list
    # invalid values
    assert_raises(TypeError) { @insert_statement.bind(list: 'x') }
    assert_raises(TypeError) { @insert_statement.bind(list: { 1 => 2 }) }
    assert_raises(TypeError) { @insert_statement.bind(list: ['x']) }
    assert_raises(TypeError) { @insert_statement.bind(list: [nil]) }
    assert_raises(RangeError) { @insert_statement.bind(list: [2**31]) }

    # valid values
    assert_kind_of(Ilios::Cassandra::Statement, @insert_statement.bind(list: [1, 2, 3]))

    results = insert_and_get_results

    assert_equal([1, 2, 3], results.first['list'])
  end

  def test_bind_set
    # invalid values
    assert_raises(TypeError) { @insert_statement.bind(set: 123) }
    assert_raises(TypeError) { @insert_statement.bind(set: [1]) }

    # a Set subclass whose to_a returns a non-Array must raise, not crash
    broken_set = Class.new(Set) do
      def to_a
        'broken'
      end
    end

    assert_raises(TypeError) { @insert_statement.bind(set: broken_set.new(%w[a])) }

    # valid values: Array (duplicates removed by Cassandra)
    assert_kind_of(Ilios::Cassandra::Statement, @insert_statement.bind(set: %w[b a a]))

    results = insert_and_get_results

    assert_equal(Set['a', 'b'], results.first['set'])

    # valid values: Set
    assert_kind_of(Ilios::Cassandra::Statement, @insert_statement.bind(set: Set['x', 'y']))

    results = insert_and_get_results

    assert_equal(Set['x', 'y'], results.first['set'])
  end

  def test_bind_map
    # invalid values
    assert_raises(TypeError) { @insert_statement.bind(map: [1]) }
    # rubocop:disable Style/StringHashKeys
    assert_raises(TypeError) { @insert_statement.bind(map: { 'k' => 'v' }) }
    assert_raises(TypeError) { @insert_statement.bind(map: { 'k' => nil }) }

    # valid values
    assert_kind_of(Ilios::Cassandra::Statement, @insert_statement.bind(map: { 'k1' => 1, 'k2' => 2 }))

    results = insert_and_get_results

    assert_equal({ 'k1' => 1, 'k2' => 2 }, results.first['map'])
    # rubocop:enable Style/StringHashKeys
  end

  def test_bind_map_with_symbol_keys
    assert_kind_of(Ilios::Cassandra::Statement, @insert_statement.bind(map: { k1: 1, k2: 2 }))

    results = insert_and_get_results

    # rubocop:disable Style/StringHashKeys
    assert_equal({ 'k1' => 1, 'k2' => 2 }, results.first['map'])
    # rubocop:enable Style/StringHashKeys
  end

  def test_bind_nested_collections
    # inner sets accept both Set and Array
    # rubocop:disable Style/StringHashKeys
    @insert_statement.bind(
      {
        nested_list: [[1, 2], [3]],
        nested_map: { 'x' => Set[1, 2], 'y' => [3] }
      }
    )

    results = insert_and_get_results

    assert_equal([[1, 2], [3]], results.first['nested_list'])
    assert_equal({ 'x' => Set[1, 2], 'y' => Set[3] }, results.first['nested_map'])
    # rubocop:enable Style/StringHashKeys
  end

  def test_bind_collection_scalar_elements
    uuid = SecureRandom.uuid
    time = Time.now.ceil

    @insert_statement.bind(
      {
        map_uuid_boolean: { uuid => true },
        list_timestamp: [time]
      }
    )

    results = insert_and_get_results

    assert_equal({ uuid => true }, results.first['map_uuid_boolean'])
    assert_equal([time], results.first['list_timestamp'])
  end

  def test_bind_empty_collection_returns_nil
    # Cassandra stores empty non-frozen collections as null
    @insert_statement.bind(list: [], set: Set.new, map: {})

    results = insert_and_get_results

    assert_nil(results.first['list'])
    assert_nil(results.first['set'])
    assert_nil(results.first['map'])
  end

  def test_bind_collection_snapshot
    list = [1, 2]
    # rubocop:disable Style/StringHashKeys
    map = { 'k' => 1 }
    @insert_statement.bind(list: list, map: map)
    list << 3
    map['k2'] = 2

    results = insert_and_get_results

    assert_equal([1, 2], results.first['list'])
    assert_equal({ 'k' => 1 }, results.first['map'])
    # rubocop:enable Style/StringHashKeys
  end

  def test_bind_collection_element_shrinking_the_array
    shrinker = Class.new do
      def initialize(array)
        @array = array
      end

      def to_str
        @array.clear
        'a'
      end
    end

    array = []
    array << shrinker.new(array)
    100.times { array << 'b' }

    # snapshotting must stop at the shrunk length instead of reading past the reallocated buffer
    @insert_statement.bind(set: array)

    results = insert_and_get_results

    assert_equal(Set['a'], results.first['set'])
  end

  def test_bind_collection_element_growing_the_array
    grower = Class.new do
      def initialize(array)
        @array = array
      end

      def to_str
        @array << self.class.new(@array) if @array.size < 100
        'a'
      end
    end

    array = []
    array << grower.new(array)

    @insert_statement.bind(set: array)

    assert_equal(2, array.size)

    results = insert_and_get_results

    assert_equal(Set['a'], results.first['set'])
  end

  private

  def insert_and_get_results
    id = Random.rand(2**60)
    @insert_statement.bind({ id: id })
    Ilios::Cassandra.session.execute(@insert_statement)

    statement = Ilios::Cassandra.session.prepare(<<~CQL)
      SELECT * FROM ilios.test WHERE id = ?;
    CQL
    statement.bind({ id: id })
    Ilios::Cassandra.session.execute(statement)
  end
end
