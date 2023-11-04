# Ilios

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

## Example
### Basic usage

Create the keyspace in advance using the `cqlsh` command.

```sh
CREATE KEYSPACE IF NOT EXISTS ilios
WITH REPLICATION = {
  'class' : 'SimpleStrategy',
  'replication_factor' : 1
};
```

```ruby
require 'ilios'

Ilios::Cassandra.config = {
  keyspace: 'ilios',
  hosts: ['127.0.0.1'],
}

# Create the table
statement = Ilios::Cassandra.session.prepare(<<~CQL)
  CREATE TABLE IF NOT EXISTS ilios.example (
    id bigint,
    message text,
    created_at timestamp,
    PRIMARY KEY (id)
  ) WITH compaction = { 'class' : 'LeveledCompactionStrategy' }
  AND gc_grace_seconds = 691200;
CQL
Ilios::Cassandra.session.execute(statement)

# Insert the records
statement = Ilios::Cassandra.session.prepare(<<~CQL)
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
  Ilios::Cassandra.session.execute(statement)
end

# Select the records
statement = Ilios::Cassandra.session.prepare(<<~CQL)
  SELECT * FROM ilios.example
CQL
statement.page_size = 25
result = Ilios::Cassandra.session.execute(statement)
result.each do |row|
  p row
end

while(result.next_page)
  result.each do |row|
    p row
  end
end
```

### Synchronous API
`Ilios::Cassandra::Session#prepare` and `Ilios::Cassandra::Session#execute` are provided as synchronous API.

```ruby
statement = Ilios::Cassandra.session.prepare(<<~CQL)
  SELECT * FROM ilios.example
CQL
result = Ilios::Cassandra.session.execute(statement)
```

### Asynchronous API
`Ilios::Cassandra::Session#prepare_async` and `Ilios::Cassandra::Session#execute_async` are provided as asynchronous API.

```ruby
prepare_future = Ilios::Cassandra.session.prepare_async(<<~CQL)
  INSERT INTO ilios.example (
    id,
    message,
    created_at
  ) VALUES (?, ?, ?)
CQL

prepare_future.on_success { |statement|
  statement.bind({
    id: 1,
    message: 'Hello World',
    created_at: Time.now,
  })
  result_future = Ilios::Cassandra.session.execute_async(statement)
  result_future.on_success { |result|
    p result
    p "success"
  }
  result_future.on_failure {
    p "fail"
  }
}
```

## Contributing

Bug reports and pull requests are welcome on GitHub at https://github.com/Watson1978/ilios.
