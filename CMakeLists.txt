cmake_minimum_required (VERSION 3.21)

project (cope LANGUAGES CXX)

set (CMAKE_CXX_STANDARD 20)
set (CMAKE_CXX_EXTENSIONS OFF)
set (CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (MSVC)
  add_compile_options(/W4 /WX /utf-8 /Gd /permissive-)
else()
  add_compile_options(-Wall -Wextra -pedantic -Werror)
endif()

add_subdirectory (benchmarks)
add_subdirectory (examples)
