cmake_minimum_required(VERSION 3.21)

include_directories(${PROJECT_SOURCE_DIR}/include)

#add_executable(simple simple.cpp)
#add_executable(nested nested.cpp)

#target_compile_options(simple PUBLIC
#  $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX /utf-8 /Gd /permissive->
#  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -pedantic -Werror -Wno-changes-meaning>
#)

#target_compile_options(nested PUBLIC
#  $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX /utf-8 /Gd /permissive->
#  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-O3 -Wall -Wextra -pedantic -Werror -Wno-changes-meaning>
#)
