# frozen_string_literal: true

require 'ilios.so'
require 'ilios/version'

module Ilios
  module Cassandra
    @@config = {
      keyspace: 'ilios',
      hosts: ['127.0.0.1'],
      timeout_ms: 5_000,
      constant_delay_ms: 15_000,
      max_speculative_executions: 2,
      page_size: 10_000
    }

    def self.config=(config)
      @@config = @@config.merge(config)
    end

    def self.session
      Thread.current[:ilios_cassandra_session] ||= connect
    end
  end
end
