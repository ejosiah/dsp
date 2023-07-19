#pragma once

#include <chrono>
#include "format.h"
#include <portaudio.h>
#include <cassert>

namespace audio {
    class Info{
    public:
        friend class Engine;
        friend class PatchMixer;
        friend class PatchOutput;
        friend class PatchSplitter;

        [[nodiscard]]
        std::chrono::seconds time() const;

        [[nodiscard]]
        double cpuLoad() const;

        [[nodiscard]]
        Format format() const;

        bool isActive() const;

        bool isStopped() const;

        uint32_t sampleRate() const;

        int outputChannels();

    private:
        explicit Info(Format format, PaStream* iStream);

        Info() = default;

        PaStream* stream{};
        Format _format{};
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

    bool Info::isActive() const {
        return Pa_IsStreamActive(stream);
    }

    bool Info::isStopped() const {
        return Pa_IsStreamStopped(stream);
    }

    uint32_t Info::sampleRate() const {
        return _format.sampleRate;
    }

    int Info::outputChannels() {
        return _format.outputChannels;
    }
}