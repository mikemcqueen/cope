add_executable (cope "cope.cpp" "txsetprice.cpp" "log.cpp" "txsellitem.cpp" "dp.cpp" "Price_t.cpp")

target_link_libraries(cope PUBLIC ui)

target_include_directories(cope PUBLIC
  "${PROJECT_SOURCE_DIR}/ui"
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET cope PROPERTY CXX_STANDARD 20)
endif()

target_compile_options(cope PUBLIC
  $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX /utf-8>
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Werror>
)
