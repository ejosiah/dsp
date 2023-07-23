#add_executable(midi testio.cpp)
add_executable(midi synthesizer.h main.cpp)
target_link_libraries(midi vui audio choc dsp portmidi)