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

      if futures.size > 100
        tmp = futures.slice!(0..10)
        Cassandra::Future.all(*tmp).get
      end
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

      futures.slice!(0..10).each(&:await) if futures.size > 100
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
  x.warmup = 0
  x.time = 10
  BenchmarkCassandra.new.run_execute(x)
  BenchmarkCassandra.new.run_execute_async(x)
end

GC.start
sleep 20

puts ''
Benchmark.ips do |x|
  x.warmup = 0
  x.time = 10
  BenchmarkIlios.new.run_execute(x)
  BenchmarkIlios.new.run_execute_async(x)
end

=begin
## Environment
- OS: Manjaro Linux x86_64
- Kernel: 6.7.7-1-MANJARO
- CPU: AMD Ryzen 9 7940HS
- Compiler: gcc 13.2.1
- Ruby: ruby 3.3.0

## Results
Calculating -------------------------------------
cassandra-driver:execute
                        139.571 (± 7.2%) i/s -      1.387k in   9.998314s
cassandra-driver:execute_async
                         11.554k (±86.6%) i/s -      2.467k in  10.001188s

Calculating -------------------------------------
       ilios:execute    327.715 (±16.5%) i/s -      3.187k in   9.994403s
 ilios:execute_async    454.625k (±78.4%) i/s -     10.161k in   9.996146s
=end
