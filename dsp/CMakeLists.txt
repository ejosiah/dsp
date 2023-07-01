file(GLOB_RECURSE HPP_FILES ${CMAKE_CURRENT_LIST_DIR} *.h *.hpp *.inl)
file(GLOB_RECURSE CPP_FILES ${CMAKE_CURRENT_LIST_DIR} *.cpp)

add_executable(dsp_main ${HPP_FILES} ${CPP_FILES})

add_library(dsp ${HPP_FILES} ${CPP_FILES})
target_link_libraries(dsp choc PortAudio vui)
target_link_libraries(dsp_main dsp choc PortAudio vui)
target_include_directories(vui PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
        $<INSTALL_INTERFACE:/include>
        )