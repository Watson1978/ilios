# frozen_string_literal: true

require File.expand_path('../../lib/ilios/version', __dir__)
require 'etc'
require 'fileutils'
require 'mini_portile2'
require 'mkmf'
require 'rbconfig'

have_func('malloc_usable_size')
have_func('malloc_size')

module LibuvInstaller
  LIBUV_INSTALL_PATH = File.expand_path('libuv')
  private_constant :LIBUV_INSTALL_PATH

  @@special_install_path = nil

  class LibuvRecipe < MiniPortileCMake
    def configure_prefix
      "-DCMAKE_INSTALL_PREFIX=#{LIBUV_INSTALL_PATH}"
    end

    def cmake_compile_flags
      flags = super
      flags << "-DCMAKE_POLICY_VERSION_MINIMUM='3.5'"
    end
  end

  def self.install
    install_from_source
  end

  def self.install_from_source
    unless File.exist?(LIBUV_INSTALL_PATH)
      libuv_recipe = LibuvRecipe.new('libuv', Ilios::LIBUV_VERSION, make_command: "make -j #{Etc.nprocessors}")
      libuv_recipe.files << {
        url: "https://github.com/libuv/libuv/archive/v#{Ilios::LIBUV_VERSION}.tar.gz"
      }
      libuv_recipe.cook
      if RUBY_PLATFORM.include?('darwin')
        xsystem(
          "install_name_tool -id #{LIBUV_INSTALL_PATH}/lib/libuv.1.dylib #{LIBUV_INSTALL_PATH}/lib/libuv.1.dylib"
        )
      end
    end
    @@special_install_path = LIBUV_INSTALL_PATH

    FileUtils.rm_rf('ports')
    FileUtils.rm_rf('tmp')

    $CPPFLAGS += " -I#{LIBUV_INSTALL_PATH}/include"
    $LDFLAGS += " -L#{LIBUV_INSTALL_PATH}/lib -Wl,-rpath,#{LIBUV_INSTALL_PATH}/lib -luv"
  end

  def self.special_install_path
    @@special_install_path
  end
end

module CassandraDriverInstaller
  CASSANDRA_CPP_DRIVER_INSTALL_PATH = File.expand_path('cpp-driver')
  private_constant :CASSANDRA_CPP_DRIVER_INSTALL_PATH

  class CassandraRecipe < MiniPortileCMake
    def initialize(name, version, **kwargs)
      ENV['LIBUV_ROOT_DIR'] ||= LibuvInstaller.special_install_path

      super
    end

    def configure_prefix
      "-DCMAKE_INSTALL_PREFIX=#{CASSANDRA_CPP_DRIVER_INSTALL_PATH}"
    end

    def cmake_compile_flags
      flags = super
      flags << "-DCMAKE_POLICY_VERSION_MINIMUM='3.5'"
      # Avoid "Unsupported compiler: AppleClang" on macOS
      if RUBY_PLATFORM.include?('darwin') && RbConfig::CONFIG['CC'].include?('clang')
        flags << "-DCMAKE_CXX_FLAGS='-Wno-error -Wno-c++11-extensions'"
        flags << '-DCMAKE_CXX_COMPILER_ID=Clang'
      end

      flags
    end
  end

  def self.install
    install_from_source
  end

  def self.install_from_source
    unless File.exist?(CASSANDRA_CPP_DRIVER_INSTALL_PATH)
      cassandra_recipe = CassandraRecipe.new('cpp-driver', Ilios::CASSANDRA_CPP_DRIVER_VERSION, make_command: "make -j #{Etc.nprocessors}")
      cassandra_recipe.files << {
        url: "https://github.com/datastax/cpp-driver/archive/#{Ilios::CASSANDRA_CPP_DRIVER_VERSION}.tar.gz"
      }
      cassandra_recipe.cook
      if RUBY_PLATFORM.include?('darwin')
        xsystem(
          "install_name_tool -id #{CASSANDRA_CPP_DRIVER_INSTALL_PATH}/lib/libcassandra.2.dylib #{CASSANDRA_CPP_DRIVER_INSTALL_PATH}/lib/libcassandra.2.dylib"
        )
      end
    end
    @@installed_path_from_source = CASSANDRA_CPP_DRIVER_INSTALL_PATH

    FileUtils.rm_rf('ports')
    FileUtils.rm_rf('tmp')

    $CPPFLAGS += " -I#{CASSANDRA_CPP_DRIVER_INSTALL_PATH}/include"
    $LDFLAGS += " -L#{CASSANDRA_CPP_DRIVER_INSTALL_PATH}/lib -Wl,-rpath,#{CASSANDRA_CPP_DRIVER_INSTALL_PATH}/lib -lcassandra"
  end
end

if (dir = with_config('--with-cassandra-driver-dir'))
  $CPPFLAGS += " -I#{dir}/include"
  $LDFLAGS += " -L#{dir}/lib -Wl,-rpath,#{dir}/lib -lcassandra"
else

  unless find_executable('cmake')
    puts '--------------------------------------------------'
    puts 'Error: cmake is required to build this gem'
    puts '--------------------------------------------------'
    raise
  end

  if RUBY_PLATFORM.include?('darwin') && !find_executable('install_name_tool')
    puts('------------------------------------------------------')
    puts('Error: install_name_tool is required to build this gem')
    puts('------------------------------------------------------')
    raise
  end

  if (dir = with_config('--with-libuv-dir'))
    $CPPFLAGS += " -I#{dir}/include"
    $LDFLAGS += " -L#{dir}/lib -Wl,-rpath,#{dir}/lib -luv"
    ENV['LIBUV_ROOT_DIR'] = dir
  else
    LibuvInstaller.install
  end
  CassandraDriverInstaller.install
end

$CPPFLAGS += " #{ENV['CPPFLAGS']}"
$LDFLAGS += " #{ENV['LDFLAGS']}"

create_makefile('ilios')

require 'extconf_compile_commands_json'
ExtconfCompileCommandsJson.generate!
