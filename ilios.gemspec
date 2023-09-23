# frozen_string_literal: true

require_relative "lib/ilios/version"

Gem::Specification.new do |spec|
  spec.name = "ilios"
  spec.version = Ilios::VERSION
  spec.authors = ["Watson"]
  spec.email = ["watson1978@gmail.com"]

  spec.summary = "Cassandra driver written by C language"
  spec.description = "Cassandra driver written by C language"
  spec.homepage = "https://github.com/Watson1978/ilios"
  spec.required_ruby_version = ">= 3.0.0"

  spec.metadata["homepage_uri"] = spec.homepage
  spec.metadata["source_code_uri"] = "https://github.com/Watson1978/ilios"
  # spec.metadata["changelog_uri"] = "TODO: Put your gem's CHANGELOG.md URL here."

  # Specify which files should be added to the gem when it is released.
  # The `git ls-files -z` loads the files in the RubyGem that have been added into git.
  spec.files = Dir.chdir(__dir__) do
    `git ls-files -z`.split("\x0").reject do |f|
      (f == __FILE__) || f.match(%r{\A(?:(?:bin|test|spec|features)/|\.(?:git|circleci)|appveyor)})
    end
  end
  spec.bindir = "exe"
  spec.executables = spec.files.grep(%r{\Aexe/}) { |f| File.basename(f) }
  spec.require_paths = ["lib"]

  spec.extensions << 'ext/ilios/extconf.rb'

  spec.add_runtime_dependency 'mini_portile2', '~> 2.8'
  spec.add_development_dependency 'rake-compiler', '~> 1.2'
end
