file(GLOB_RECURSE HPP_FILES *.h)
file(GLOB_RECURSE CPP_FILES *.cpp)

add_executable(dsp ${HPP_FILES} ${CPP_FILES})
target_link_libraries(dsp choc PortAudio)