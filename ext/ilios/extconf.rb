require File.expand_path('../../../lib/ilios/version', __FILE__)
require "mini_portile2"
require "fileutils"
require 'mkmf'

CASSANDRA_CPP_DRIVER_INSTALL_PATH = File.expand_path(File.join(File.dirname(__FILE__), "cpp-driver"))
LIBUV_INSTALL_PATH = File.expand_path(File.join(File.dirname(__FILE__), "libuv"))

unless find_executable("cmake")
  puts "--------------------------------------------------"
  puts "Error: cmake is required to build this gem"
  puts "--------------------------------------------------"
  raise
end

unless File.exist?(LIBUV_INSTALL_PATH)
  libuv_recipe = MiniPortileCMake.new("libuv", Ilios::LIBUV_VERSION, make_command: "make -j")
  libuv_recipe.files << {
    :url => "https://github.com/libuv/libuv/archive/v#{Ilios::LIBUV_VERSION}.tar.gz",
  }
  libuv_recipe.cook
  lib_path = File.join(File.dirname(__FILE__), "ports/#{libuv_recipe.host}/libuv/#{Ilios::LIBUV_VERSION}")
  FileUtils.mv(lib_path, LIBUV_INSTALL_PATH)
end

unless File.exist?(CASSANDRA_CPP_DRIVER_INSTALL_PATH)
  ENV["LIBUV_ROOT_DIR"] = LIBUV_INSTALL_PATH
  cassandra_recipe = MiniPortileCMake.new("cpp-driver", Ilios::CASSANDRA_CPP_DRIVER_VERSION, make_command: "make -j")
  cassandra_recipe.files << {
    :url => "https://github.com/datastax/cpp-driver/archive/#{Ilios::CASSANDRA_CPP_DRIVER_VERSION}.tar.gz",
  }
  cassandra_recipe.cook
  lib_path = File.join(File.dirname(__FILE__), "ports/#{cassandra_recipe.host}/cpp-driver/#{Ilios::CASSANDRA_CPP_DRIVER_VERSION}")
  FileUtils.mv(lib_path, CASSANDRA_CPP_DRIVER_INSTALL_PATH)
end

FileUtils.rm_rf("ports")
FileUtils.rm_rf("tmp")

$CPPFLAGS += " -I#{CASSANDRA_CPP_DRIVER_INSTALL_PATH}/include"
$LDFLAGS += " -L#{CASSANDRA_CPP_DRIVER_INSTALL_PATH}/lib -lcassandra"

create_makefile('ilios')
