# frozen_string_literal: true

require_relative "ilios/version"
require 'ilios.so'

module Ilios
  module Cassandra
    @@config = {
      keyspace: 'ilios',
      hosts: ['127.0.0.1'],
      timeout_ms: 5_000,
      constant_delay_ms: 15_000,
      max_speculative_executions: 2,
    }

    def self.config=(config)
      @@config = @@config.merge(config)
    end
  end
end
