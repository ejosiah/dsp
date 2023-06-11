cmake_minimum_required(VERSION 3.24)
project(dsp)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(dependencies)
add_subdirectory(src)

message(bin=${CMAKE_CURRENT_BINARY_DIR})