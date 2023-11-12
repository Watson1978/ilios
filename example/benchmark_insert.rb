# frozen_string_literal: true

require 'bundler/inline'
gemfile do
  source 'https://rubygems.org'
  gem 'benchmark-ips'
  gem 'sorted_set'
  gem 'cassandra-driver', require: 'cassandra'
  gem 'ilios'
end

Ilios::Cassandra.config = {
  keyspace: 'ilios',
  hosts: ['127.0.0.1']
}

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
    futures = []

    x.report('cassandra-driver:execute_async') do
      future = @session.execute_async(
        statement,
        {
          arguments: {
            id: Random.rand(2**40),
            message: 'hello',
            created_at: Time.now
          }
        }
      )
      future.on_success do |rows|
      end

      futures << future
    end

    Cassandra::Future.all(*futures).get
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
    futures = []

    x.report('ilios:execute_async') do
      statement.bind(
        {
          id: Random.rand(2**40),
          message: 'hello',
          created_at: Time.now
        }
      )
      future = Ilios::Cassandra.session.execute_async(statement)
      future.on_success do |results|
      end

      futures << future
    end

    futures.each(&:await)
  end

  def statement
    @statement ||= Ilios::Cassandra.session.prepare(<<-CQL)
      INSERT INTO ilios.benchmark_insert (
        id,
        message,
        created_at
      ) VALUES (?, ?, ?)
    CQL
  end
end

Benchmark.ips do |x|
  x.warmup = 1
  x.time = 2
  BenchmarkCassandra.new.run_execute(x)
  BenchmarkCassandra.new.run_execute_async(x)
end

GC.start
sleep 20

puts ''
Benchmark.ips do |x|
  x.warmup = 1
  x.time = 2
  BenchmarkIlios.new.run_execute(x)
  BenchmarkIlios.new.run_execute_async(x)
end

=begin
## Environment
- OS: Manjaro Linux x86_64
- CPU: AMD Ryzen 9 7940HS
- Compiler: gcc 13.2.1
- Ruby: ruby 3.2.2

## Results
Warming up --------------------------------------
cassandra-driver:execute
                       335.000  i/100ms
cassandra-driver:execute_async
                         2.558k i/100ms
Calculating -------------------------------------
cassandra-driver:execute
                          3.154k (±34.1%) i/s -      3.685k in   2.036576s
cassandra-driver:execute_async
                         34.718k (±29.2%) i/s -     61.392k in   2.055368s

Warming up --------------------------------------
       ilios:execute   439.000  i/100ms
 ilios:execute_async    11.018k i/100ms
Calculating -------------------------------------
       ilios:execute      4.393k (± 3.7%) i/s -      9.219k in   2.101254s
 ilios:execute_async    138.471k (±38.3%) i/s -    242.396k in   2.101860s
=end
