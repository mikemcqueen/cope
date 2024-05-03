cmake_minimum_required (VERSION 3.21)

project (cope LANGUAGES CXX)

set (CMAKE_CXX_STANDARD 20)
set (CMAKE_CXX_EXTENSIONS OFF)
set (CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory (benchmarks)
add_subdirectory (examples)
