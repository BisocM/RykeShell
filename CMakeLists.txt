cmake_minimum_required(VERSION 3.25.1)
project(RykeShell)

set(CMAKE_CXX_STANDARD 20)

include_directories(${PROJECT_SOURCE_DIR}/include)

add_executable(RykeShell src/ryke_shell.cpp
        include/ryke_shell.h
        src/parser.cpp
        src/executor.cpp
        src/utils.cpp
        src/input.cpp
        include/commands.h
        src/commands.cpp
        include/input.h
        include/utils.h
        include/autocomplete.h
        src/autocomplete.cpp)