#pragma once

#include <cstdint>
#include <chrono>
#include "forwards.h"
#include "portaudio.h"

#define ERR_GUARD_PA(err) \
if(err != paNoError) {    \
    std::cerr <<  "PortAudio error: " <<   Pa_GetErrorText(err) << "\n"; \
    exit(err);               \
}

namespace audio {

    using ulong = unsigned long;

    enum class SampleFormat : ulong {
        Float32 = 0x00000001,
        Int32 = 0x00000002,
        Int24 = 0x00000004,
        Int16 = 0x00000008,
        Int8 = 0x00000010,
        Uint8 = 0x00000020,
        Custom = 0x00010000
    };

    class Engine {
    public:
        Engine(int inputChannels, int outputChannels, SampleFormat format, double sampleRate, ulong framesPerBuffer);

        static int callback(const void *inputBuffer,
                        void *outputBuffer,
                        unsigned long frameCount,
                        const PaStreamCallbackTimeInfo* timeInfo,
                        PaStreamCallbackFlags statusFlags,
                        void *userData);

        PatchInput connectNewInput(uint32_t maxLatencyInSamples);
        PatchInput connectNewOutput(uint32_t maxLatencyInSamples);

        bool isActive() const;

        bool isStopped() const;

        std::chrono::seconds time() const;

        double cpuLoad() const;
    private:
        struct {
            int input{0};
            int output{0};
        } m_channels{};

        SampleFormat m_format;
        double m_sampleRate;
        ulong  m_framesPerBuffer;
        PaStream* m_audioStream{};
    };
}