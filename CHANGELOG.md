# Change Log

## 1.0.4

-  Fix macOS build failure with Apple Clang on macOS 26+ (#23)

## 1.0.3

- Return 0 from ilios_malloc_size when no size API is available
- Add proper unblock function for nogvl_sem_wait
- Register thread-pool slots with GC only once
- Guard future callback dispatch against double invocation
- Use write barriers when mutating WB_PROTECTED structs
- Retain Statement object in async execute future
- Update libuv to v1.52.1 (#22)
- Remove unnecessary parentheses
- Remove extconf_compile_commands_json from runtime dependency
- Revert "Fix build error with -c++11-extensions"
- Revert "Disable -Werror on macOS"
- Disable -Werror on macOS
- Fix build error with -c++11-extensions
- Add workaround to avoid build error on macOS
- Use extconf_compile_commands_json gem for clangd LSP

## 1.0.2

- Fix install error with CMake 4.x

## 1.0.1

- Update libuv version to 1.50.0 (#18)
- Add Ruby 3.4 support (#17)

## 1.0.0

- Support for
  - Ruby 3.1 or later
  - Cassandra 3.0 or later
- Used libraries
  - DataStax C/C++ Driver 2.17.1
  - libuv 1.48.0
