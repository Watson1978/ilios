require 'mkmf'

libpath = "../cpp-driver"

$CPPFLAGS += " -I#{libpath}/include"
$LDFLAGS += " -L#{libpath}/lib -lcassandra"

create_makefile('ilios')
