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

# Number of queries issued per benchmark iteration.
#
# Both the sync and async benchmarks run this many queries per iteration so
# their results are directly comparable. The async benchmarks issue this many
# queries concurrently and then wait for *all* of them to resolve (not a
# fire-and-forget issue); the sync benchmarks run them one after another. Both
# drivers are driven the same way, which makes the results comparable
# regardless of how each driver handles backpressure.
#
# NOTE: benchmark-ips therefore reports *batches* per second. Multiply the
# reported i/s by BATCH_SIZE to get completed queries per second.
BATCH_SIZE = Integer(ENV.fetch('BATCH_SIZE', '100'))

class BenchmarkCassandra
  def initialize
    @cluster = Cassandra.cluster
    @keyspace = 'ilios'
    @session = @cluster.connect(@keyspace)
  end

  def run_execute(x)
    x.report("cassandra-driver:execute (batch=#{BATCH_SIZE})") do
      BATCH_SIZE.times do
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
  end

  def run_execute_async(x)
    x.report("cassandra-driver:execute_async (batch=#{BATCH_SIZE})") do
      futures = Array.new(BATCH_SIZE) do
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
    x.report("ilios:execute (batch=#{BATCH_SIZE})") do
      BATCH_SIZE.times do
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
  end

  def run_execute_async(x)
    # ilios binds values into a single mutable statement per Cassandra::Statement
    # object, so each concurrently in-flight query needs its own statement;
    # otherwise rebinding would race on values the driver is still reading.
    statements = Array.new(BATCH_SIZE) { build_statement }
    x.report("ilios:execute_async (batch=#{BATCH_SIZE})") do
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
      cassandra-driver:execute (batch=100)     25.234 (±19.8%) i/s   (39.63 ms/i) -    505.000 in  20.012849s
cassandra-driver:execute_async (batch=100)    141.953 (±19.0%) i/s    (7.04 ms/i) -      2.839k in  19.999596s

### ilios
$ RUN=ilios ruby benchmark_insert.rb
Calculating -------------------------------------
      ilios:execute (batch=100)     37.861 (±18.5%) i/s   (26.41 ms/i) -    758.000 in  20.020361s
ilios:execute_async (batch=100)    778.005 (±55.9%) i/s    (1.29 ms/i) -     15.551k in  19.988308s
=end
