cmake_minimum_required(VERSION 3.25.1)
project(RykeShell LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(rykeshell_lib
        src/ryke_shell.cpp
        src/parser.cpp
        src/executor.cpp
        src/utils.cpp
        src/input.cpp
        src/commands.cpp
        src/autocomplete.cpp)
target_include_directories(rykeshell_lib PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_compile_options(rykeshell_lib PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(rykeshell_lib PUBLIC dl)

add_executable(RykeShell src/main.cpp)
target_link_libraries(RykeShell PRIVATE rykeshell_lib)

enable_testing()
add_executable(RykeShellTests
        tests/test_runner.cpp
        tests/parser_tests.cpp
        tests/executor_tests.cpp
        tests/expansion_tests.cpp)
target_link_libraries(RykeShellTests PRIVATE rykeshell_lib)
add_test(NAME rykeshell_tests COMMAND RykeShellTests)
