# CMakeList.txt : CMake project for cope, include source and define
# project specific logic here.
#

# Add source to this project's executable.
add_executable (cope "cope.cpp" "cope.h" "txsellitem.h" "txsetprice.h" "dp.h" "txsetprice.cpp"
                     "log.h" "log.cpp" "txsellitem.cpp" "dp.cpp" "Eq2UiIds.h")

target_link_libraries(cope PUBLIC ui)

target_include_directories(cope PUBLIC
  "${PROJECT_SOURCE_DIR}/ui"
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET cope PROPERTY CXX_STANDARD 20)
endif()

target_compile_options(cope PUBLIC
  $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Werror>
)
