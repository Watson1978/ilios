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

```ruby
require 'ilios'

Ilios::Cassandra.config = {
  keyspace: 'ilios',
  hosts: ['127.0.0.1'],
}

# Create the keyspace
statement = Ilios::Cassandra.session.prepare(<<~CQL)
  CREATE KEYSPACE IF NOT EXISTS ilios
  WITH REPLICATION = {
  'class' : 'SimpleStrategy',
  'replication_factor' : 1
  };
CQL
Ilios::Cassandra.session.execute(statement)

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
  statement
    .bind_bigint(0, i)
    .bind_text(1, 'Hello World')
    .bind_timestamp(2, Time.now)
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

## Contributing

Bug reports and pull requests are welcome on GitHub at https://github.com/Watson1978/ilios.
