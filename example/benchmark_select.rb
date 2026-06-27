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
  DROP TABLE IF EXISTS ilios.benchmark_select
CQL
Ilios::Cassandra.session.execute(statement)

statement = Ilios::Cassandra.session.prepare(<<~CQL)
  CREATE TABLE IF NOT EXISTS ilios.benchmark_select (
    id bigint,
    message text,
    created_at timestamp,
    PRIMARY KEY (id)
  ) WITH compaction = { 'class' : 'LeveledCompactionStrategy' }
  AND gc_grace_seconds = 691200;
CQL
Ilios::Cassandra.session.execute(statement)

# Prepare data
statement = Ilios::Cassandra.session.prepare(<<-CQL)
  INSERT INTO ilios.benchmark_select (
    id,
    message,
    created_at
  ) VALUES (?, ?, ?)
CQL

1000.times do
  statement.bind(
    {
      id: Random.rand(2**40),
      message: 'hello',
      created_at: Time.now
    }
  )
  Ilios::Cassandra.session.execute(statement)
end

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
      @session.execute(statement)
    end
  end

  def run_execute_async(x)
    x.report("cassandra-driver:execute_async (batch=#{ASYNC_BATCH_SIZE})") do
      futures = Array.new(ASYNC_BATCH_SIZE) { @session.execute_async(statement) }
      futures.each(&:join)
    end
  end

  def statement
    @statement ||= @session.prepare(<<~CQL)
      SELECT * FROM ilios.benchmark_select
    CQL
  end
end

class BenchmarkIlios
  def run_execute(x)
    x.report('ilios:execute') do
      Ilios::Cassandra.session.execute(statement)
    end
  end

  def run_execute_async(x)
    x.report("ilios:execute_async (batch=#{ASYNC_BATCH_SIZE})") do
      futures = Array.new(ASYNC_BATCH_SIZE) { Ilios::Cassandra.session.execute_async(statement) }
      futures.each(&:await)
    end
  end

  def statement
    @statement ||= Ilios::Cassandra.session.prepare(<<-CQL)
      SELECT * FROM ilios.benchmark_select
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
$ RUN=cassandra ruby benchmark_select.rb
Calculating -------------------------------------
                  cassandra-driver:execute    172.313 (± 7.5%) i/s    (5.80 ms/i) -      3.445k in  19.992696s
cassandra-driver:execute_async (batch=100)      4.913 (± 0.0%) i/s  (203.54 ms/i) -     99.000 in  20.150937s

### ilios
$ RUN=ilios ruby benchmark_select.rb
Calculating -------------------------------------
                  ilios:execute    310.300 (±11.0%) i/s    (3.22 ms/i) -      6.203k in  19.990320s
ilios:execute_async (batch=100)     18.593 (± 5.4%) i/s   (53.78 ms/i) -    372.000 in  20.007994s
=end
