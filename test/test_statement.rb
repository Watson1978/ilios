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
        uuid
      ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
    CQL
  end

  def test_bind_null
    # invalid index
    assert_raises(Ilios::Cassandra::StatementError) { @insert_statement.bind_null(-1) }
    assert_raises(Ilios::Cassandra::StatementError) { @insert_statement.bind_null(20) }
    assert_raises(TypeError) { @insert_statement.bind_null(Object.new) }

    # valid values
    @insert_statement.bind_tinyint(1, 127)
    @insert_statement.bind_null(1)

    results = insert_and_get_results

    assert_nil(results.first['tinyint'])
  end

  def test_bind_tinyint
    # invalid index
    assert_raises(Ilios::Cassandra::StatementError) { @insert_statement.bind_tinyint(-1, 1) }
    assert_raises(Ilios::Cassandra::StatementError) { @insert_statement.bind_tinyint(20, 1) }
    assert_raises(TypeError) { @insert_statement.bind_tinyint(Object.new, 1) }

    # invalid value
    assert_raises(Ilios::Cassandra::StatementError) { @insert_statement.bind_tinyint(1, -2**7 - 1) }
    assert_raises(Ilios::Cassandra::StatementError) { @insert_statement.bind_tinyint(1, 2**7) }
    assert_raises(RangeError) { @insert_statement.bind_tinyint(1, 2**63) } # bignum
    assert_raises(TypeError) { @insert_statement.bind_tinyint(1, Object.new) }

    # valid values
    assert(@insert_statement.bind_tinyint(1, -2**7))
    assert(@insert_statement.bind_tinyint(1, (2**7) - 1))

    results = insert_and_get_results

    assert_equal((2**7) - 1, results.first['tinyint'])
  end

  def test_bind_smallint
    # invalid index
    assert_raises(Ilios::Cassandra::StatementError) { @insert_statement.bind_smallint(-1, 1) }
    assert_raises(Ilios::Cassandra::StatementError) { @insert_statement.bind_smallint(20, 1) }
    assert_raises(TypeError) { @insert_statement.bind_smallint(Object.new, 1) }

    # invalid value
    assert_raises(Ilios::Cassandra::StatementError) { @insert_statement.bind_smallint(2, -2**15 - 1) }
    assert_raises(Ilios::Cassandra::StatementError) { @insert_statement.bind_smallint(2, 2**15) }
    assert_raises(RangeError) { @insert_statement.bind_smallint(2, 2**63) } # bignum
    assert_raises(TypeError) { @insert_statement.bind_smallint(2, Object.new) }

    # valid values
    assert(@insert_statement.bind_smallint(2, -2**15))
    assert(@insert_statement.bind_smallint(2, (2**15) - 1))

    results = insert_and_get_results

    assert_equal((2**15) - 1, results.first['smallint'])
  end

  def test_bind_int
    # invalid index
    assert_raises(Ilios::Cassandra::StatementError) { @insert_statement.bind_int(-1, 1) }
    assert_raises(Ilios::Cassandra::StatementError) { @insert_statement.bind_int(20, 1) }
    assert_raises(TypeError) { @insert_statement.bind_int(Object.new, 1) }

    # invalid value
    assert_raises(Ilios::Cassandra::StatementError) { @insert_statement.bind_int(3, -2**31 - 1) }
    assert_raises(Ilios::Cassandra::StatementError) { @insert_statement.bind_int(3, 2**31) }
    assert_raises(RangeError) { @insert_statement.bind_int(3, 2**63) } # bignum
    assert_raises(TypeError) { @insert_statement.bind_int(3, Object.new) }

    # valid values
    assert(@insert_statement.bind_int(3, -2**31))
    assert(@insert_statement.bind_int(3, (2**31) - 1))

    results = insert_and_get_results

    assert_equal((2**31) - 1, results.first['int'])
  end

  def test_bind_bigint
    # invalid index
    assert_raises(Ilios::Cassandra::StatementError) { @insert_statement.bind_bigint(-1, 1) }
    assert_raises(Ilios::Cassandra::StatementError) { @insert_statement.bind_bigint(20, 1) }
    assert_raises(TypeError) { @insert_statement.bind_bigint(Object.new, 1) }

    # invalid value
    assert_raises(RangeError) { @insert_statement.bind_bigint(4, 2**63) } # bignum
    assert_raises(TypeError) { @insert_statement.bind_bigint(4, Object.new) }

    # valid values
    assert(@insert_statement.bind_bigint(4, -2**62))
    assert(@insert_statement.bind_bigint(4, 2**62))

    results = insert_and_get_results

    assert_equal(2**62, results.first['bigint'])
  end

  def test_bind_float
    # invalid index
    assert_raises(Ilios::Cassandra::StatementError) { @insert_statement.bind_float(-1, 1) }
    assert_raises(Ilios::Cassandra::StatementError) { @insert_statement.bind_float(20, 1) }
    assert_raises(TypeError) { @insert_statement.bind_float(Object.new, 1) }

    # invalid value
    assert_raises(Ilios::Cassandra::StatementError) { @insert_statement.bind_float(5, -3.402820018375656e+39) }
    assert_raises(Ilios::Cassandra::StatementError) { @insert_statement.bind_float(5, 3.402820018375656e+39) }
    assert_raises(TypeError) { @insert_statement.bind_float(5, Object.new) }

    # valid values
    assert(@insert_statement.bind_float(5, -3.402820018375656e+38))
    assert(@insert_statement.bind_float(5, 3.402820018375656e+38))

    results = insert_and_get_results

    assert_in_delta(3.402820018375656e+38, results.first['float'])
  end

  def test_bind_double
    # invalid index
    assert_raises(Ilios::Cassandra::StatementError) { @insert_statement.bind_double(-1, 1) }
    assert_raises(Ilios::Cassandra::StatementError) { @insert_statement.bind_double(20, 1) }
    assert_raises(TypeError) { @insert_statement.bind_double(Object.new, 1) }

    # invalid value
    assert_raises(TypeError) { @insert_statement.bind_double(6, Object.new) }

    # valid values
    assert(@insert_statement.bind_double(6, -1.79769313486232e+307))
    assert(@insert_statement.bind_double(6, 1.79769313486232e+307))

    results = insert_and_get_results

    assert_in_delta(1.79769313486232e+307, results.first['double'])
  end

  def test_bind_boolean
    # invalid index
    assert_raises(Ilios::Cassandra::StatementError) { @insert_statement.bind_boolean(-1, 1) }
    assert_raises(Ilios::Cassandra::StatementError) { @insert_statement.bind_boolean(20, 1) }
    assert_raises(TypeError) { @insert_statement.bind_boolean(Object.new, 1) }

    # valid values for true
    assert(@insert_statement.bind_boolean(7, true))

    results = insert_and_get_results

    assert(results.first['boolean'])

    # valid values for false
    assert(@insert_statement.bind_boolean(7, false))

    results = insert_and_get_results

    refute(results.first['boolean'])
  end

  def test_bind_text
    # invalid index
    assert_raises(Ilios::Cassandra::StatementError) { @insert_statement.bind_text(-1, 'hello') }
    assert_raises(Ilios::Cassandra::StatementError) { @insert_statement.bind_text(20, 'hello') }
    assert_raises(TypeError) { @insert_statement.bind_text(Object.new, 'hello') }
    assert_raises(TypeError) { @insert_statement.bind_text(8, :hello) }

    # invalid value
    assert_raises(TypeError) { @insert_statement.bind_text(8, Object.new) }

    # valid values
    assert(@insert_statement.bind_text(8, 'hello'))

    results = insert_and_get_results

    assert_equal('hello', results.first['text'])

    # valid values
    obj = Object.new
    def obj.to_str
      'hello'
    end

    assert(@insert_statement.bind_text(8, obj))

    results = insert_and_get_results

    assert_equal('hello', results.first['text'])
  end

  def test_bind_timestamp
    # invalid index
    assert_raises(Ilios::Cassandra::StatementError) { @insert_statement.bind_timestamp(-1, Time.now) }
    assert_raises(Ilios::Cassandra::StatementError) { @insert_statement.bind_timestamp(20, Time.now) }
    assert_raises(TypeError) { @insert_statement.bind_timestamp(Object.new, Time.now) }

    # invalid value
    assert_raises(TypeError) { @insert_statement.bind_timestamp(9, Object.new) }

    # valid values
    time = Time.now.ceil

    assert(@insert_statement.bind_timestamp(9, time))

    results = insert_and_get_results

    assert_equal(time, results.first['timestamp'])

    # valid values
    date_time = DateTime.parse('2023-11-01T12:30:45')

    assert(@insert_statement.bind_timestamp(9, date_time))

    results = insert_and_get_results

    assert_equal(date_time.to_time, results.first['timestamp'])
  end

  def test_bind_uuid
    # invalid index
    assert_raises(Ilios::Cassandra::StatementError) { @insert_statement.bind_uuid(-1, SecureRandom.uuid) }
    assert_raises(Ilios::Cassandra::StatementError) { @insert_statement.bind_uuid(20, SecureRandom.uuid) }
    assert_raises(TypeError) { @insert_statement.bind_uuid(Object.new, SecureRandom.uuid) }

    # invalid value
    assert_raises(TypeError) { @insert_statement.bind_uuid(10, Object.new) }

    # valid values
    uuid = SecureRandom.uuid

    assert(@insert_statement.bind_uuid(10, uuid))

    results = insert_and_get_results

    assert_equal(uuid, results.first['uuid'])
  end

  def test_bind
    # invalid value
    assert_raises(TypeError) { @insert_statement.bind(Object.new) }
    assert_raises(Ilios::Cassandra::StatementError) { @insert_statement.bind({ foo: 123 }) }

    # valid values
    assert(
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
  end

  def test_bind_bind_null
    @insert_statement.bind(tinyint: nil)

    results = insert_and_get_results

    assert_nil(results.first['tinyint'])
  end

  def test_bind_bind_tinyint
    # invalid value
    assert_raises(Ilios::Cassandra::StatementError) { @insert_statement.bind(tinyint: -2**7 - 1) }
    assert_raises(Ilios::Cassandra::StatementError) { @insert_statement.bind(tinyint: 2**7) }
    assert_raises(RangeError) { @insert_statement.bind(tinyint: 2**63) } # bignum
    assert_raises(TypeError) { @insert_statement.bind(tinyint: Object.new) }

    # valid values
    assert(@insert_statement.bind(tinyint: -2**7))
    assert(@insert_statement.bind(tinyint: (2**7) - 1))

    results = insert_and_get_results

    assert_equal((2**7) - 1, results.first['tinyint'])
  end

  def test_bind_bind_smallint
    # invalid value
    assert_raises(Ilios::Cassandra::StatementError) { @insert_statement.bind(smallint: -2**15 - 1) }
    assert_raises(Ilios::Cassandra::StatementError) { @insert_statement.bind(smallint: 2**15) }
    assert_raises(RangeError) { @insert_statement.bind(smallint: 2**63) } # bignum
    assert_raises(TypeError) { @insert_statement.bind(smallint: Object.new) }

    # valid values
    assert(@insert_statement.bind(smallint: -2**15))
    assert(@insert_statement.bind(smallint: (2**15) - 1))

    results = insert_and_get_results

    assert_equal((2**15) - 1, results.first['smallint'])
  end

  def test_bind_bind_int
    # invalid value
    assert_raises(Ilios::Cassandra::StatementError) { @insert_statement.bind(int: -2**31 - 1) }
    assert_raises(Ilios::Cassandra::StatementError) { @insert_statement.bind(int: 2**31) }
    assert_raises(RangeError) { @insert_statement.bind(int: 2**63) } # bignum
    assert_raises(TypeError) { @insert_statement.bind(int: Object.new) }

    # valid values
    assert(@insert_statement.bind(int: -2**31))
    assert(@insert_statement.bind(int: (2**31) - 1))

    results = insert_and_get_results

    assert_equal((2**31) - 1, results.first['int'])
  end

  def test_bind_bind_bigint
    # invalid value
    assert_raises(RangeError) { @insert_statement.bind(bigint: 2**63) } # bignum
    assert_raises(TypeError) { @insert_statement.bind(bigint: Object.new) }

    # valid values
    assert(@insert_statement.bind(bigint: -2**62))
    assert(@insert_statement.bind(bigint: 2**62))

    results = insert_and_get_results

    assert_equal(2**62, results.first['bigint'])
  end

  def test_bind_bind_float
    # invalid value
    assert_raises(Ilios::Cassandra::StatementError) { @insert_statement.bind(float: -3.402820018375656e+39) }
    assert_raises(Ilios::Cassandra::StatementError) { @insert_statement.bind(float: 3.402820018375656e+39) }
    assert_raises(TypeError) { @insert_statement.bind(float: Object.new) }

    # valid values
    assert(@insert_statement.bind(float: -3.402820018375656e+38))
    assert(@insert_statement.bind(float: 3.402820018375656e+38))

    results = insert_and_get_results

    assert_in_delta(3.402820018375656e+38, results.first['float'])
  end

  def test_bind_bind_double
    # invalid value
    assert_raises(TypeError) { @insert_statement.bind(double: Object.new) }

    # valid values
    assert(@insert_statement.bind(double: -1.79769313486232e+307))
    assert(@insert_statement.bind(double: 1.79769313486232e+307))

    results = insert_and_get_results

    assert_in_delta(1.79769313486232e+307, results.first['double'])
  end

  def test_bind_bind_boolean
    # valid values for true
    assert(@insert_statement.bind(boolean: true))

    results = insert_and_get_results

    assert(results.first['boolean'])

    # valid values for false
    assert(@insert_statement.bind(boolean: false))

    results = insert_and_get_results

    refute(results.first['boolean'])
  end

  def test_bind_bind_text
    # invalid value
    assert_raises(TypeError) { @insert_statement.bind(text: Object.new) }

    # valid values
    assert(@insert_statement.bind(text: 'hello'))

    results = insert_and_get_results

    assert_equal('hello', results.first['text'])

    # valid values
    obj = Object.new
    def obj.to_str
      'hello'
    end

    assert(@insert_statement.bind(text: obj))

    results = insert_and_get_results

    assert_equal('hello', results.first['text'])
  end

  def test_bind_bind_timestamp
    # invalid value
    assert_raises(TypeError) { @insert_statement.bind(timestamp: Object.new) }

    # valid values
    time = Time.now.ceil

    assert(@insert_statement.bind(timestamp: time))

    results = insert_and_get_results

    assert_equal(time, results.first['timestamp'])

    # valid values
    date_time = DateTime.parse('2023-11-01T12:30:45')

    assert(@insert_statement.bind(timestamp: date_time))

    results = insert_and_get_results

    assert_equal(date_time.to_time, results.first['timestamp'])
  end

  def test_bind_bind_uuid
    # invalid value
    assert_raises(TypeError) { @insert_statement.bind(uuid: Object.new) }

    # valid values
    uuid = SecureRandom.uuid

    assert(@insert_statement.bind(uuid: uuid))

    results = insert_and_get_results

    assert_equal(uuid, results.first['uuid'])
  end

  private

  def insert_and_get_results
    id = Random.rand(2**60)
    @insert_statement.bind_bigint(0, id)
    Ilios::Cassandra.session.execute(@insert_statement)

    statement = Ilios::Cassandra.session.prepare(<<~CQL)
      SELECT * FROM ilios.test WHERE id = ?;
    CQL
    statement.bind({ id: id })
    Ilios::Cassandra.session.execute(statement)
  end
end
