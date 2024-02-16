# frozen_string_literal: true

require_relative 'helper'

class CassandraTest < Minitest::Test
  def teardown
    # rubocop:disable Style/GlobalVars
    Ilios::Cassandra.log_level($default_test_log_level)
    # rubocop:enable Style/GlobalVars
  end

  def test_log_level
    assert_raises(TypeError) { Ilios::Cassandra.log_level(Object.new) }

    Ilios::Cassandra.log_level(Ilios::Cassandra::LOG_DEBUG)
    pass
  end
end
