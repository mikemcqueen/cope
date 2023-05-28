cmake_minimum_required (VERSION 3.21)

add_executable (cope "cope.cpp" "txsetprice.cpp" "log.cpp" "txsellitem.cpp" "Price_t.cpp")

target_link_libraries(cope PUBLIC ui)

target_include_directories(cope PUBLIC
  "${PROJECT_SOURCE_DIR}/ui"
)

set_property(TARGET cope PROPERTY CXX_STANDARD 20)

target_compile_options(cope PUBLIC
  $<$<CXX_COMPILER_ID:MSVC>:/Wall /WX /utf-8 /Gd /permissive->
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Werror>
)
