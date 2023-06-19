#include <iostream>

#include "audio_test.h"
#include "dsp/ring_buffer.h"

int main() {
    RingBuffer<int, 256> rb{};

    std::cout << "RingBuffer before fill\n";
    std::cout << "\treadIndex: " << rb.readIndex() << "\n";
    std::cout << "\twriteIndex: " << rb.writeIndex() << "\n";

    for(int i = 0; i < 256; ++i){
        rb.write(i);
    }

    std::cout << "\nRingBuffer after fill\n";
    std::cout << "\treadIndex: " << rb.readIndex() << "\n";
    std::cout << "\twriteIndex: " << rb.writeIndex() << "\n\n";

    for(int i = 0; i < 256; i++){
        std::cout << "next: " << rb.next() << ", readIndex: " << rb.readIndex() << "\n";
    }

    for(int i = 0; i < 128; ++i){
        rb.write(i);
    }

    std::cout << "\nRingBuffer after fill again\n";
    std::cout << "\treadIndex: " << rb.readIndex() << "\n";
    std::cout << "\twriteIndex: " << rb.writeIndex() << "\n\n";

    for(int i = 0; i < 128; i++){
        std::cout << "next: " << rb.next() << ", readIndex: " << rb.readIndex() << "\n";
    }
    return 0;
}
