#pragma once

#include <cstdint>

namespace audio {
    using ulong = unsigned long;

    enum class SampleType : ulong {
        Float32 = 0x00000001,
        Int32 = 0x00000002,
        Int24 = 0x00000004,
        Int16 = 0x00000008,
        Int8 = 0x00000010,
        Uint8 = 0x00000020,
        Custom = 0x00010000
    };

  //  Engine(int inputChannels, int outputChannels, SampleType format, double sampleRate, ulong framesPerBuffer);

    struct Format {
        uint32_t inputChannels{};
        uint32_t outputChannels{};
        SampleType sampleType{SampleType::Float32};
        uint32_t sampleRate{};
        uint32_t  frameBufferSize{};
        uint32_t audioBufferSize{};
    };
}