cmake_minimum_required(VERSION 3.16)
project(batppc LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(batppc
    src/main.cpp
    src/compiler_utils.cpp
    src/compiler_parse.cpp
    src/compiler_transpile.cpp
    src/compiler_emit.cpp
)
