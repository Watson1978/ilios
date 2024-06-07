# frozen_string_literal: true

require 'bundler/gem_tasks'
require 'rake/extensiontask'
require 'rake/testtask'

desc 'Run tests'
task test: :compile

task default: :test

Rake::ExtensionTask.new('ilios') do |ext|
  ext.ext_dir = 'ext/ilios'
end

Rake::TestTask.new do |task|
  task.pattern = 'test/test_*.rb'
end

namespace :rbs do
  desc 'Validate RBS definitions'
  task :validate do
    all_sigs = Dir.glob('sig').map { |dir| "-I #{dir}" }.join(' ')
    sh("bundle exec rbs #{all_sigs} validate") do |ok, _|
      abort('one or more rbs validate failed') unless ok
    end
  end
end
