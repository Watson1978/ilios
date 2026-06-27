# frozen_string_literal: true

require 'bundler/inline'
gemfile do
  source 'https://rubygems.org'
  gem 'benchmark-ips'
  gem 'sorted_set'
  gem 'bigdecimal'
  gem 'logger'
  gem 'cassandra-driver', require: 'cassandra'
  gem 'ilios'
end

module Ilios
  module Cassandra
    def self.session
      @session ||= begin
        cluster = Ilios::Cassandra::Cluster.new
        cluster.keyspace('ilios')
        cluster.hosts(['127.0.0.1'])
        cluster.connect
      end
    end
  end
end

# Create new table
statement = Ilios::Cassandra.session.prepare(<<~CQL)
  DROP TABLE IF EXISTS ilios.benchmark_insert
CQL
Ilios::Cassandra.session.execute(statement)

statement = Ilios::Cassandra.session.prepare(<<~CQL)
  CREATE TABLE IF NOT EXISTS ilios.benchmark_insert (
    id bigint,
    message text,
    created_at timestamp,
    PRIMARY KEY (id)
  ) WITH compaction = { 'class' : 'LeveledCompactionStrategy' }
  AND gc_grace_seconds = 691200;
CQL
Ilios::Cassandra.session.execute(statement)

# Number of async queries issued per benchmark iteration.
#
# The async benchmarks issue this many queries concurrently and then wait for
# *all* of them to resolve, so a single iteration corresponds to N completed
# queries (not a fire-and-forget issue). Both drivers are driven the same way
# (issue N, then wait), which makes the results comparable regardless of how
# each driver handles backpressure.
#
# NOTE: benchmark-ips therefore reports *batches* per second. Multiply the
# reported i/s by ASYNC_BATCH_SIZE to get completed queries per second.
ASYNC_BATCH_SIZE = Integer(ENV.fetch('ASYNC_BATCH_SIZE', '100'))

class BenchmarkCassandra
  def initialize
    @cluster = Cassandra.cluster
    @keyspace = 'ilios'
    @session = @cluster.connect(@keyspace)
  end

  def run_execute(x)
    x.report('cassandra-driver:execute') do
      @session.execute(
        statement,
        {
          arguments: {
            id: Random.rand(2**40),
            message: 'hello',
            created_at: Time.now
          }
        }
      )
    end
  end

  def run_execute_async(x)
    x.report("cassandra-driver:execute_async (batch=#{ASYNC_BATCH_SIZE})") do
      futures = Array.new(ASYNC_BATCH_SIZE) do
        @session.execute_async(
          statement,
          {
            arguments: {
              id: Random.rand(2**40),
              message: 'hello',
              created_at: Time.now
            }
          }
        )
      end
      futures.each(&:join)
    end
  end

  def statement
    @statement ||= @session.prepare(<<~CQL)
      INSERT INTO ilios.benchmark_insert (
        id,
        message,
        created_at
      ) VALUES (?, ?, ?)
    CQL
  end
end

class BenchmarkIlios
  def run_execute(x)
    x.report('ilios:execute') do
      statement.bind(
        {
          id: Random.rand(2**40),
          message: 'hello',
          created_at: Time.now
        }
      )
      Ilios::Cassandra.session.execute(statement)
    end
  end

  def run_execute_async(x)
    # ilios binds values into a single mutable statement per Cassandra::Statement
    # object, so each concurrently in-flight query needs its own statement;
    # otherwise rebinding would race on values the driver is still reading.
    statements = Array.new(ASYNC_BATCH_SIZE) { build_statement }
    x.report("ilios:execute_async (batch=#{ASYNC_BATCH_SIZE})") do
      futures = statements.map do |st|
        st.bind(
          {
            id: Random.rand(2**40),
            message: 'hello',
            created_at: Time.now
          }
        )
        Ilios::Cassandra.session.execute_async(st)
      end
      futures.each(&:await)
    end
  end

  def statement
    @statement ||= build_statement
  end

  def build_statement
    Ilios::Cassandra.session.prepare(<<-CQL)
      INSERT INTO ilios.benchmark_insert (
        id,
        message,
        created_at
      ) VALUES (?, ?, ?)
    CQL
  end
end

case ENV['RUN']
when 'cassandra'
  Benchmark.ips do |x|
    x.warmup = 0
    x.time = 20
    BenchmarkCassandra.new.run_execute(x)
    BenchmarkCassandra.new.run_execute_async(x)
  end

when 'ilios', nil
  Benchmark.ips do |x|
    x.warmup = 0
    x.time = 20
    BenchmarkIlios.new.run_execute(x)
    BenchmarkIlios.new.run_execute_async(x)
  end
end

=begin
## Environment
- OS: CachyOS x86_64
- Kernel: Linux 7.1.1-2-cachyos
- CPU: Intel(R) Core(TM) Ultra 9 275HX
- Compiler: 16.1.1
- Ruby: ruby 4.0.5

## Results
### cassandra-driver
$ RUN=cassandra ruby benchmark_insert.rb
Calculating -------------------------------------
                  cassandra-driver:execute      1.814k (±78.7%) i/s  (551.42 μs/i) -     36.211k in  19.967317s
cassandra-driver:execute_async (batch=100)    146.388 (±19.8%) i/s    (6.83 ms/i) -      2.928k in  20.001580s

### ilios
$ RUN=ilios ruby benchmark_insert.rb
Calculating -------------------------------------
                  ilios:execute      2.339k (±80.7%) i/s  (427.61 μs/i) -     46.714k in  19.975484s
ilios:execute_async (batch=100)    762.482 (±57.6%) i/s    (1.31 ms/i) -     15.241k in  19.988665s
=end
