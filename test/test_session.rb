# frozen_string_literal: true

require_relative 'helper'

class SessionTest < Minitest::Test
  def test_prepare
    # invalid query
    assert_raises(Ilios::Cassandra::ExecutionError) { Ilios::Cassandra.session.prepare('foo') }
    assert_raises(TypeError) { Ilios::Cassandra.session.prepare(Object.new) }

    # valid query
    assert_kind_of(Ilios::Cassandra::Statement, Ilios::Cassandra.session.prepare('SELECT * FROM ilios.test;'))
  end

  def test_execute
    # invalid statement
    assert_raises(TypeError) { Ilios::Cassandra.session.execute(Object.new) }

    # valid statement
    statement = Ilios::Cassandra.session.prepare('SELECT * FROM ilios.test;')

    assert_kind_of(Ilios::Cassandra::Result, Ilios::Cassandra.session.execute(statement))
  end

  def test_prepare_async
    # invalid query
    assert_raises(TypeError) { Ilios::Cassandra.session.prepare_async(Object.new) }

    # valid query
    future = Ilios::Cassandra.session.prepare_async('SELECT * FROM ilios.test;')

    assert_kind_of(Ilios::Cassandra::Future, future)

    success_count = 0
    failure_count = 0
    future.on_success do |statement|
      assert_kind_of(Ilios::Cassandra::Statement, statement)
      success_count += 1
    end
    future.on_failure do
      failure_count += 1
    end

    sleep(0.1)

    assert_equal(1, success_count)
    assert_equal(0, failure_count)

    # invalid query
    future = Ilios::Cassandra.session.prepare_async('foo')

    success_count = 0
    failure_count = 0
    future.on_success do |_statement|
      success_count += 1
    end
    future.on_failure do
      failure_count += 1
    end

    sleep(0.1)

    assert_equal(0, success_count)
    assert_equal(1, failure_count)
  end

  def test_execute_async
    # invalid statement
    assert_raises(TypeError) { Ilios::Cassandra.session.execute_async(Object.new) }

    # valid statement
    statement = Ilios::Cassandra.session.prepare('SELECT * FROM ilios.test;')
    future = Ilios::Cassandra.session.execute_async(statement)

    assert_kind_of(Ilios::Cassandra::Future, future)

    success_count = 0
    failure_count = 0
    future.on_success do |result|
      assert_kind_of(Ilios::Cassandra::Result, result)
      success_count += 1
    end
    future.on_failure do
      failure_count += 1
    end

    sleep(0.1)

    assert_equal(1, success_count)
    assert_equal(0, failure_count)
  end

  def test_async
    success_count = 0

    prepare_future = Ilios::Cassandra.session.prepare_async('SELECT * FROM ilios.test;')
    prepare_future.on_success do |statement|
      result_future = Ilios::Cassandra.session.execute_async(statement)
      result_future.on_success do |result|
        assert_kind_of(Ilios::Cassandra::Result, result)
        success_count += 1
      end
    end

    sleep(0.1)

    assert_equal(1, success_count)
  end
end
