# frozen_string_literal: true

require 'bundler/gem_tasks'
require 'rake/extensiontask'
require 'rake/testtask'

task test: :compile
task default: :test

Rake::ExtensionTask.new('ilios') do |ext|
  ext.ext_dir = 'ext/ilios'
end

Rake::TestTask.new do |task|
  task.pattern = 'test/test_*.rb'
end
