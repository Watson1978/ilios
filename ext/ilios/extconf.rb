# frozen_string_literal: true

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
  class LibuvRecipe < MiniPortileCMake
    def configure_prefix
      "-DCMAKE_INSTALL_PREFIX=#{LIBUV_INSTALL_PATH}"
    end
  end

  libuv_recipe = LibuvRecipe.new("libuv", Ilios::LIBUV_VERSION, make_command: "make -j")
  libuv_recipe.files << {
    :url => "https://github.com/libuv/libuv/archive/v#{Ilios::LIBUV_VERSION}.tar.gz",
  }
  libuv_recipe.cook
end

unless File.exist?(CASSANDRA_CPP_DRIVER_INSTALL_PATH)
  class CassandraRecipe < MiniPortileCMake
    def configure_prefix
      "-DCMAKE_INSTALL_PREFIX=#{CASSANDRA_CPP_DRIVER_INSTALL_PATH}"
    end

    def cmake_compile_flags
      flags = super
      flags.unshift "-DCASS_BUILD_STATIC=ON,"
    end
  end

  ENV["LIBUV_ROOT_DIR"] = LIBUV_INSTALL_PATH
  cassandra_recipe = CassandraRecipe.new("cpp-driver", Ilios::CASSANDRA_CPP_DRIVER_VERSION, make_command: "make -j")
  cassandra_recipe.files << {
    :url => "https://github.com/datastax/cpp-driver/archive/#{Ilios::CASSANDRA_CPP_DRIVER_VERSION}.tar.gz",
  }
  cassandra_recipe.cook
end

FileUtils.rm_rf("ports")
FileUtils.rm_rf("tmp")

$CPPFLAGS += " -I#{CASSANDRA_CPP_DRIVER_INSTALL_PATH}/include #{CASSANDRA_CPP_DRIVER_INSTALL_PATH}/lib/libcassandra_static.a"

create_makefile('ilios')
