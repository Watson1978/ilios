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
    x.report('cassandra-driver:execute_async') do
      future = @session.execute_async(statement)
      future.on_success do |rows|
      end
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
    x.report('ilios:execute_async') do
      future = Ilios::Cassandra.session.execute_async(statement)
      future.on_success do |results|
        results.each do |row|
        end
      end
    end
  end

  def statement
    @statement ||= Ilios::Cassandra.session.prepare(<<-CQL)
      SELECT * FROM ilios.benchmark_select
    CQL
  end
end

Benchmark.ips do |x|
  BenchmarkCassandra.new.run_execute(x)
  BenchmarkCassandra.new.run_execute_async(x)
end

sleep 10

puts ''
Benchmark.ips do |x|
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
                        13.000  i/100ms
cassandra-driver:execute_async
                         7.621k i/100ms
Calculating -------------------------------------
cassandra-driver:execute
                         48.627  (±20.6%) i/s -    247.000  in   5.301501s
cassandra-driver:execute_async
                         77.896k (±31.4%) i/s -    304.840k in   5.063101s

Warming up --------------------------------------
       ilios:execute    15.000  i/100ms
 ilios:execute_async    10.000  i/100ms
Calculating -------------------------------------
       ilios:execute    166.764  (±28.2%) i/s -    735.000  in   5.135326s
 ilios:execute_async    539.420k (±24.4%) i/s -     90.870k in   5.656643s
=end
