file(GLOB_RECURSE HPP_FILES ${CMAKE_CURRENT_LIST_DIR} *.h)
file(GLOB_RECURSE CPP_FILES ${CMAKE_CURRENT_LIST_DIR} *.cpp)

add_library(dsp ${HPP_FILES} ${CPP_FILES})
target_link_libraries(dsp choc PortAudio vui)
target_include_directories(vui PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
        $<INSTALL_INTERFACE:/include>
        )