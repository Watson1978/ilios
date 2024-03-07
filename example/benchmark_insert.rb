# frozen_string_literal: true

require 'bundler/inline'
gemfile do
  source 'https://rubygems.org'
  gem 'benchmark-ips'
  gem 'sorted_set'
  gem 'bigdecimal'
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
- OS: Manjaro Linux x86_64
- Kernel: 6.7.7-1-MANJARO
- CPU: AMD Ryzen 9 7940HS
- Compiler: gcc 13.2.1
- Ruby: ruby 3.3.0

## Results
### cassandra-driver
$ RUN=cassandra ruby benchmark_insert.rb
Calculating -------------------------------------
cassandra-driver:execute
                          4.022k (±19.1%) i/s -     76.144k in  19.956985s
cassandra-driver:execute_async
                         45.162k (±20.6%) i/s -    514.720k in  19.872873s

### ilios
$ RUN=ilios ruby benchmark_insert.rb
Calculating -------------------------------------
       ilios:execute      4.857k (±24.0%) i/s -     91.235k in  19.960159s
 ilios:execute_async    351.393k (±55.5%) i/s -      2.029M in  19.462660s
=end
