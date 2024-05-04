#cmake_minimum_required(VERSION 3.21)

include_directories(${PROJECT_SOURCE_DIR}/include)

add_executable(simple simple.cpp)
add_executable(nested nested.cpp)
