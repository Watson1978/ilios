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
      BATCH_SIZE.times { @session.execute(statement) }
    end
  end

  def run_execute_async(x)
    x.report("cassandra-driver:execute_async (batch=#{BATCH_SIZE})") do
      futures = Array.new(BATCH_SIZE) { @session.execute_async(statement) }
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
    x.report("ilios:execute (batch=#{BATCH_SIZE})") do
      BATCH_SIZE.times { Ilios::Cassandra.session.execute(statement) }
    end
  end

  def run_execute_async(x)
    x.report("ilios:execute_async (batch=#{BATCH_SIZE})") do
      futures = Array.new(BATCH_SIZE) { Ilios::Cassandra.session.execute_async(statement) }
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
      cassandra-driver:execute (batch=100)      1.763 (± 0.0%) i/s  (567.20 ms/i) -     36.000 in  20.419075s
cassandra-driver:execute_async (batch=100)      4.888 (± 0.0%) i/s  (204.58 ms/i) -     98.000 in  20.049122s

### ilios
$ RUN=ilios ruby benchmark_select.rb
Calculating -------------------------------------
      ilios:execute (batch=100)      3.212 (± 0.0%) i/s  (311.36 ms/i) -     65.000 in  20.238291s
ilios:execute_async (batch=100)     18.662 (± 5.4%) i/s   (53.58 ms/i) -    374.000 in  20.040410s
=end
