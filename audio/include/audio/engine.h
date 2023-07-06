#pragma once

#include <cstdint>
#include <chrono>
#include "forwards.h"
#include "portaudio.h"
#include "format.h"
#include <thread>
#include "patch.h"

#define ERR_GUARD_PA(err) \
if(err != paNoError) {    \
    std::cerr <<  "PortAudio error: " <<   Pa_GetErrorText(err) << "\n"; \
    exit(err);               \
}

namespace audio {

    class Info{
    public:
        friend class Engine;

        [[nodiscard]]
        std::chrono::seconds time() const;

        [[nodiscard]]
        double cpuLoad() const;

        [[nodiscard]]
        Format format() const;

    private:
        explicit Info(Format format, PaStream* iStream);

        PaStream* stream{};
        Format _format{};
    };

    class Engine {
    public:
        Engine(Format format);

        void start();

        void shutdown();

        PatchInput connectNewInput(uint32_t maxLatencyInSamples);

        PatchOutput connectNewOutput(uint32_t maxLatencyInSamples);

        bool isActive() const;

        bool isStopped() const;

        Info info() const;

    private:
        static int callback(const void *inputBuffer,
                        void *outputBuffer,
                        unsigned long frameCount,
                        const PaStreamCallbackTimeInfo* timeInfo,
                        PaStreamCallbackFlags statusFlags,
                        void *userData);
    private:
        struct {
            int input{0};
            int output{0};
        } m_channels{};

        Format m_format{};
        PaStream* m_audioStream{};
        std::thread m_thisThread;
    };

    std::chrono::seconds Info::time() const {
        assert(stream);
        return std::chrono::seconds{ static_cast<long long>(Pa_GetStreamTime(stream)) };
    }

    double Info::cpuLoad() const {
        assert(stream);
        return Pa_GetStreamCpuLoad(stream);
    }

    Info::Info( Format format, PaStream *iStream)
    : stream{iStream}
    , _format{format}
    {}

    Format Info::format() const {
        return _format;
    }
}