file(GLOB_RECURSE HPP_FILES ${CMAKE_CURRENT_LIST_DIR} *.h *.hpp *.inl)
file(GLOB_RECURSE CPP_FILES ${CMAKE_CURRENT_LIST_DIR} *.cpp)

add_library(audio ${HPP_FILES} ${CPP_FILES})
target_link_libraries(audio PUBLIC PortAudio)
target_include_directories(audio PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
        $<INSTALL_INTERFACE:/include>
        )