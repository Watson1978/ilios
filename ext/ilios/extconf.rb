# frozen_string_literal: true

require File.expand_path('../../../lib/ilios/version', __FILE__)
require "mini_portile2"
require "fileutils"
require 'mkmf'

have_func('malloc_usable_size')
have_func('malloc_size')

CASSANDRA_CPP_DRIVER_INSTALL_PATH = File.expand_path("cpp-driver")
LIBUV_INSTALL_PATH = File.expand_path("libuv")

unless find_executable("cmake")
  puts "--------------------------------------------------"
  puts "Error: cmake is required to build this gem"
  puts "--------------------------------------------------"
  raise
end

unless File.exist?(LIBUV_INSTALL_PATH)
  class LibuvRecipe < MiniPortileCMake
    def configure_prefix
      "-DCMAKE_INSTALL_PREFIX=#{LIBUV_INSTALL_PATH}"
    end

    def configure_defaults
      flags = super
      flags << "-DCMAKE_BUILD_TYPE=Release"
    end
  end

  libuv_recipe = LibuvRecipe.new("libuv", Ilios::LIBUV_VERSION, make_command: "make -j")
  libuv_recipe.files << {
    url: "https://github.com/libuv/libuv/archive/v#{Ilios::LIBUV_VERSION}.tar.gz",
  }
  libuv_recipe.cook
  if RUBY_PLATFORM =~ /darwin/
    unless find_executable("install_name_tool")
      puts "------------------------------------------------------"
      puts "Error: install_name_tool is required to build this gem"
      puts "------------------------------------------------------"
      raise
    end
    xsystem("install_name_tool -id #{LIBUV_INSTALL_PATH}/lib/libuv.1.dylib #{LIBUV_INSTALL_PATH}/lib/libuv.1.dylib")
  end
end

unless File.exist?(CASSANDRA_CPP_DRIVER_INSTALL_PATH)
  class CassandraRecipe < MiniPortileCMake
    def initialize(name, version, **kwargs)
      ENV["LIBUV_ROOT_DIR"] = LIBUV_INSTALL_PATH
      super(name, version, **kwargs)
    end
    def configure_prefix
      "-DCMAKE_INSTALL_PREFIX=#{CASSANDRA_CPP_DRIVER_INSTALL_PATH}"
    end

    def configure_defaults
      flags = super
      flags << "-DCMAKE_BUILD_TYPE=Release"
    end
  end

  cassandra_recipe = CassandraRecipe.new("cpp-driver", Ilios::CASSANDRA_CPP_DRIVER_VERSION, make_command: "make -j")
  cassandra_recipe.files << {
    url: "https://github.com/datastax/cpp-driver/archive/#{Ilios::CASSANDRA_CPP_DRIVER_VERSION}.tar.gz",
  }
  cassandra_recipe.cook
  if RUBY_PLATFORM =~ /darwin/
    xsystem("install_name_tool -id #{CASSANDRA_CPP_DRIVER_INSTALL_PATH}/lib/libcassandra.2.dylib #{CASSANDRA_CPP_DRIVER_INSTALL_PATH}/lib/libcassandra.2.dylib")
  end
end

FileUtils.rm_rf("ports")
FileUtils.rm_rf("tmp")

$CPPFLAGS += " -I#{CASSANDRA_CPP_DRIVER_INSTALL_PATH}/include -I#{LIBUV_INSTALL_PATH}/include"
$LDFLAGS += " -L#{CASSANDRA_CPP_DRIVER_INSTALL_PATH}/lib -Wl,-rpath,#{CASSANDRA_CPP_DRIVER_INSTALL_PATH}/lib -lcassandra"
$LDFLAGS += " -L#{LIBUV_INSTALL_PATH}/lib -Wl,-rpath,#{LIBUV_INSTALL_PATH}/lib -luv"

create_makefile('ilios')
