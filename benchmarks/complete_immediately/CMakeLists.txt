cmake_minimum_required (VERSION 3.21)

add_executable (complete_immediately "complete_immediately.cpp")

target_include_directories(complete_immediately PUBLIC
  "${PROJECT_SOURCE_DIR}/include"
)

set_property(TARGET complete_immediately PROPERTY CXX_STANDARD 20)

target_compile_options(complete_immediately PUBLIC
  $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX /utf-8 /Gd /permissive->
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Werror>
)
