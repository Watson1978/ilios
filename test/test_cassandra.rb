# frozen_string_literal: true

require_relative 'helper'

class CassandraTest < Minitest::Test
  def test_connect
    session = Ilios::Cassandra.connect

    assert_instance_of(Ilios::Cassandra::Session, session)

    session1 = Ilios::Cassandra.connect
    session2 = Ilios::Cassandra.connect

    refute_same(session1, session2)
  end

  def test_session
    session1 = Ilios::Cassandra.session
    session2 = Ilios::Cassandra.session

    assert_same(session1, session2)
  end
end
