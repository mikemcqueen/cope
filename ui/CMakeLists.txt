add_library(ui UiInput.cpp ui_msg.cpp)

target_include_directories(ui PRIVATE
  "${PROJECT_SOURCE_DIR}/cope"  # TODO fix, for log.h, dp.h
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET ui PROPERTY CXX_STANDARD 20)
endif()

target_compile_options(ui PUBLIC
  $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Werror>
)

