# Ilios

[![Gem Version](https://badge.fury.io/rb/ilios.svg)](https://badge.fury.io/rb/ilios)
[![CI](https://github.com/Watson1978/ilios/actions/workflows/CI.yml/badge.svg)](https://github.com/Watson1978/ilios/actions/workflows/CI.yml)
[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/Watson1978/ilios)

Ilios that Cassandra driver written by C language for Ruby using [DataStax C/C++ Driver](https://github.com/datastax/cpp-driver).

## Installation

Install the gem and add to the application's Gemfile by executing:

```sh
$ bundle add ilios
```

If bundler is not being used to manage dependencies, install the gem by executing:

```sh
$ gem install ilios
```

This gem's installer will install the DataStax C/C++ Driver to the appropriate location automatically.
However, if you prefer to install the DataStax C/C++ Driver manually, you can do so by executing:

```sh
$ bundle config set --local build.ilios --with-libuv-dir=/path/to/libuv-installed-dir
$ bundle config set --local build.ilios --with-cassandra-driver-dir=/path/to/cassandra-cpp-driver-installed-dir
$ bundle add ilios
```

or

```sh
$ gem install ilios -- --with-libuv-dir=/path/to/libuv-installed-dir --with-cassandra-driver-dir=/path/to/cassandra-cpp-driver-installed-dir
```

## Requirements

- cmake (in order to build the DataStax C/C++ Driver and libuv)
- C/C++ compiler
- install_name_tool (macOS only)

## Supported

- Ruby 3.4 or later
- Cassandra 3.0 or later
- Linux and macOS platform

## Example
### Basic usage

Create the keyspace in advance using the `cqlsh` command.

```cql
CREATE KEYSPACE IF NOT EXISTS ilios
WITH REPLICATION = {
  'class' : 'SimpleStrategy',
  'replication_factor' : 1
};
```

Then, you can run the following code.

```ruby
require 'ilios'

cluster = Ilios::Cassandra::Cluster.new
cluster.keyspace('ilios')
cluster.hosts(['127.0.0.1'])
session = cluster.connect

# Create the table
statement = session.prepare(<<~CQL)
  CREATE TABLE IF NOT EXISTS ilios.example (
    id bigint,
    message text,
    created_at timestamp,
    PRIMARY KEY (id)
  ) WITH compaction = { 'class' : 'LeveledCompactionStrategy' }
  AND gc_grace_seconds = 691200;
CQL
session.execute(statement)

# Insert the records
statement = session.prepare(<<~CQL)
  INSERT INTO ilios.example (
    id,
    message,
    created_at
  ) VALUES (?, ?, ?)
CQL

100.times do |i|
  statement.bind({
    id: i,
    message: 'Hello World',
    created_at: Time.now,
  })
  session.execute(statement)
end

# Select the records
statement = session.prepare(<<~CQL)
  SELECT * FROM ilios.example
CQL
statement.idempotent = true
statement.page_size = 25
result = session.execute(statement)
result.each do |row|
  p row
end

while(result.next_page)
  result.each do |row|
    p row
  end
end
```

### Collection types

`list`, `set` and `map` columns (including nested collections such as
`list<frozen<list<int>>>`) are supported.

```ruby
statement = session.prepare(<<~CQL)
  CREATE TABLE IF NOT EXISTS ilios.collection_example (
    id bigint,
    tags set<text>,
    scores list<int>,
    attributes map<text, bigint>,
    PRIMARY KEY (id)
  );
CQL
session.execute(statement)

statement = session.prepare(<<~CQL)
  INSERT INTO ilios.collection_example (
    id,
    tags,
    scores,
    attributes
  ) VALUES (?, ?, ?, ?)
CQL
statement.bind({
  id: 1,
  tags: Set['ruby', 'cassandra'],  # or an Array
  scores: [85, 92],
  attributes: { 'height' => 180 },
})
session.execute(statement)

statement = session.prepare(<<~CQL)
  SELECT * FROM ilios.collection_example
CQL
session.execute(statement).each do |row|
  row['tags']       # => Set["cassandra", "ruby"]
  row['scores']     # => [85, 92]
  row['attributes'] # => {"height" => 180}
end
```

Notes:

- A `set` column accepts both `Set` and `Array` on bind, and is always returned as a `Set`.
- A `list` column also accepts both `Array` and `Set` on bind, but is always returned as an `Array`. Binding a `Set` to a `list` column keeps the order `Set#to_a` returns.
- Cassandra stores an empty non-frozen collection as `null`, so inserting `[]`, `Set.new` or `{}` returns `nil` on select. This is Cassandra's data model, not an Ilios limitation.
- `nil` is not allowed as a collection element (Cassandra collections cannot contain `null`).
- `Symbol` is accepted for `text` (as well as `ascii` and `varchar`) columns and collection elements, and is stored (and returned) as a `String`.
- Because a `Symbol` map key is stored as its `String` equivalent, binding a map that contains both (for example `{ k1: 1, 'k1' => 2 }`) ends up as a single key on the server; the entry bound last wins.
- Binding a `String` containing a NUL character (`\0`) to a `text` (or `ascii` / `varchar`) column raises `ArgumentError` (known limitation).

### Synchronous API
`Ilios::Cassandra::Session#prepare` and `Ilios::Cassandra::Session#execute` are provided as synchronous API.

```ruby
statement = session.prepare(<<~CQL)
  SELECT * FROM ilios.example
CQL
result = session.execute(statement)
```

### Asynchronous API
`Ilios::Cassandra::Session#prepare_async` and `Ilios::Cassandra::Session#execute_async` are provided as asynchronous API.

```ruby
prepare_future = session.prepare_async(<<~CQL)
  INSERT INTO ilios.example (
    id,
    message,
    created_at
  ) VALUES (?, ?, ?)
CQL

prepare_future.on_success { |statement|
  futures = []

  10.times do |i|
    statement.bind({
      id: i,
      message: 'Hello World',
      created_at: Time.now,
    })
    result_future = session.execute_async(statement)
    result_future.on_success { |result|
      p result
      p "success"
    }
    result_future.on_failure {
      p "fail"
    }

    futures << result_future
  end
  futures.each(&:await)
}

prepare_future.await
```

## Contributing

Bug reports and pull requests are welcome on GitHub at https://github.com/Watson1978/ilios.
