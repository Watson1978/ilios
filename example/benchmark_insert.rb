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
        results.each do |row|
        end
      end
    end
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
                       304.000  i/100ms
cassandra-driver:execute_async
                         3.679k i/100ms
Calculating -------------------------------------
cassandra-driver:execute
                          3.332k (±34.1%) i/s -     10.032k in   5.040480s
cassandra-driver:execute_async
                         34.484k (±26.6%) i/s -    150.839k in   5.003236s

Warming up --------------------------------------
       ilios:execute   457.000  i/100ms
 ilios:execute_async    17.478k i/100ms
Calculating -------------------------------------
       ilios:execute      4.622k (± 1.6%) i/s -     23.307k in   5.044216s
 ilios:execute_async    167.010k (± 3.4%) i/s -    838.944k in   5.029271s
=end
