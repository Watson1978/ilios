require 'mkmf'

libpath = `brew --prefix cassandra-cpp-driver`.chomp

$CPPFLAGS += " -I#{libpath}/include"
$LDFLAGS += " -L#{libpath}/lib -lcassandra"

create_makefile('ilios')
