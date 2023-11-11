# frozen_string_literal: true

require 'ilios.so'
require 'ilios/version'

module Ilios
  module Cassandra
    # The default options.
    @@config = {
      keyspace: 'ilios',
      hosts: ['127.0.0.1'],
      timeout_ms: 5_000,
      constant_delay_ms: 15_000,
      max_speculative_executions: 2,
      page_size: 10_000
    }

    #
    # Configures the option for connection or execution.
    # The default value will be overridden by the specified option.
    #
    # @param config [Hash] A hash object contained the options.
    #
    def self.config=(config)
      @@config = @@config.merge(config)
    end

    #
    # Connects a session to the keyspace specified in +config+ method.
    # The session object will be memorized by each threads.
    #
    # @return [Cassandra::Session] The session object.
    # @raise [RuntimeError] If no host is specified to connect in +config+ method.
    # @raise [Cassandra::ConnectError] If the connection fails for any reason.
    #
    def self.session
      Thread.current[:ilios_cassandra_session] ||= connect
    end
  end
end
