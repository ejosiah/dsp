
add_library(choc INTERFACE)
target_include_directories(choc INTERFACE ${CMAKE_CURRENT_LIST_DIR}/choc)

add_subdirectory(fmt)
add_subdirectory(portaudio)
add_subdirectory(vui)
add_subdirectory(kfr)