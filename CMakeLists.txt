cmake_minimum_required(VERSION 3.22)
project(dsp)

set(CMAKE_CXX_STANDARD 20)

add_subdirectory(dependencies)
add_subdirectory(dsp)
add_subdirectory(audio)
add_subdirectory(eval)

message(bin=${CMAKE_CURRENT_BINARY_DIR})