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
    futures = []

    x.report('cassandra-driver:execute_async') do
      future = @session.execute_async(statement)
      future.on_success do |rows|
      end

      futures << future
    end

    Cassandra::Future.all(*futures).get
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
    futures = []

    x.report('ilios:execute_async') do
      future = Ilios::Cassandra.session.execute_async(statement)
      future.on_success do |results|
        results.each do |row|
        end
      end

      futures << future
    end

    futures.each(&:await)
  end

  def statement
    @statement ||= Ilios::Cassandra.session.prepare(<<-CQL)
      SELECT * FROM ilios.benchmark_select
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
                        10.000  i/100ms
cassandra-driver:execute_async
                         3.684k i/100ms
Calculating -------------------------------------
cassandra-driver:execute
                         48.115  (±22.9%) i/s -    100.000  in   2.183978s
cassandra-driver:execute_async
                         78.647k (±30.0%) i/s -    117.888k in   2.014176s

Warming up --------------------------------------
       ilios:execute     9.000  i/100ms
 ilios:execute_async    26.000  i/100ms
Calculating -------------------------------------
       ilios:execute    168.780  (±28.4%) i/s -    288.000  in   2.029783s
 ilios:execute_async    453.181k (±32.9%) i/s -     10.660k in   3.549510s
=end
