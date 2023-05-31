cmake_minimum_required (VERSION 3.21)

add_executable (sellitems "sellitems.cpp" "txsellitem.cpp" "txsetprice.cpp" )

target_include_directories(sellitems PUBLIC
  "${PROJECT_SOURCE_DIR}/include"
)

set_property(TARGET sellitems PROPERTY CXX_STANDARD 20)

target_compile_options(sellitems PUBLIC
  $<$<CXX_COMPILER_ID:MSVC>:/Wall /WX /utf-8 /Gd /permissive->
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Werror>
)