# frozen_string_literal: true

require 'bundler/gem_tasks'
require 'rake/extensiontask'
require 'rake/testtask'

task default: :test

Rake::ExtensionTask.new('ilios') do |ext|
  ext.ext_dir = 'ext/ilios'
end

test_config = lambda do |t|
  t.deps = [:compile]
  t.pattern = 'test/test_*.rb'
end
Rake::TestTask.new(:test, &test_config)

namespace :rbs do
  desc 'Validate RBS definitions'
  task :validate do
    all_sigs = Dir.glob('sig').map { |dir| "-I #{dir}" }.join(' ')
    sh("bundle exec rbs #{all_sigs} validate") do |ok, _|
      abort('one or more rbs validate failed') unless ok
    end
  end
end

if RUBY_PLATFORM.include?('linux')
  require 'ruby_memcheck'

  RubyMemcheck.config(binary_name: 'ilios')

  namespace :test do
    RubyMemcheck::TestTask.new(:valgrind, &test_config)
  end
end
