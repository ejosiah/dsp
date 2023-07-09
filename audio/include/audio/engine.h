#pragma once

#include <cstdint>
#include <chrono>
#include "forwards.h"
#include "portaudio.h"
#include "format.h"
#include "circular_buffer.h"
#include <thread>
#include "patch.h"
#include <iostream>

#define ERR_GUARD_PA(err) \
if((err) != paNoError) {    \
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

        bool isActive() const;

        bool isStopped() const;

    private:
        explicit Info(Format format, PaStream* iStream);

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
        return stream && Pa_IsStreamActive(stream);
    }

    bool Info::isStopped() const {
        return stream && Pa_IsStreamStopped(stream);
    }

    class Engine {
    public:
        Engine(Format format);

        ~Engine();

        void start();

        void shutdown();

        PatchInput connectNewInput(uint32_t maxLatencyInSamples);

        PatchOutputStrongPtr connectNewOutput(uint32_t maxLatencyInSamples);

        void writeToDevice(float* out);

        void readFromDevice(const float* in);

        bool isActive() const;

        bool isStopped() const;

        void sleep(std::chrono::milliseconds duration);

        Info info() const;

        void update();

        CircularAudioBuffer<real_t> m_outputBuffer;
        CircularAudioBuffer<real_t> m_inputBuffer;

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
        std::thread m_updateThread;
        PatchMixer m_mixer{};
        PatchSplitter m_splitter{};
        std::mutex m_mutex;
        std::condition_variable requestAudioData;
    };

    Engine::Engine(Format format)
    : m_format{ format }
    , m_outputBuffer{ std::max( format.audioBufferSize, format.frameBufferSize * 1024)}
    , m_inputBuffer{ std::max( format.audioBufferSize, format.frameBufferSize * 1024)}
    {
    }

    Engine::~Engine() {
        shutdown();
    }

    void Engine::start() {
        ERR_GUARD_PA(Pa_Initialize());
        ERR_GUARD_PA(Pa_OpenDefaultStream(
                &m_audioStream
                , m_format.inputChannels
                , m_format.outputChannels
                , as<PaSampleFormat>(m_format.sampleType)
                , m_format.sampleRate
                , m_format.frameBufferSize
                , Engine::callback, this));
        ERR_GUARD_PA(Pa_StartStream(m_audioStream));
        m_updateThread = std::move(std::thread{ &Engine::update, this });
    }

    void Engine::shutdown() {
        if(m_audioStream) {
            ERR_GUARD_PA(Pa_StopStream(m_audioStream))
            ERR_GUARD_PA(Pa_CloseStream(m_audioStream))
            ERR_GUARD_PA(Pa_Terminate())
            m_audioStream = nullptr;
            requestAudioData.notify_one();
            m_updateThread.join();
        }
    }

    PatchInput Engine::connectNewInput(uint32_t maxLatencyInSamples) {
        return m_mixer.addNewInput(maxLatencyInSamples, 1);
    }

    PatchOutputStrongPtr Engine::connectNewOutput(uint32_t maxLatencyInSamples) {
        return {};
    }

    void Engine::writeToDevice(float *out) {
        requestAudioData.notify_one();

        const auto size = m_format.frameBufferSize;
        std::fill_n(out, size * m_format.outputChannels, 0);
        static std::vector<real_t> bucket(size);

        auto read = m_outputBuffer.pop(bucket.data(), size);

        for(int i = 0; i < read; ++i){
            for(int c = 0; c < m_format.outputChannels; ++c){
                *out = bucket[i];
                out++;
            }
        }
    }

    void Engine::readFromDevice(const float *in) {

    }

    bool Engine::isActive() const {
        return m_audioStream && Pa_IsStreamActive(m_audioStream) == 1;
    }

    bool Engine::isStopped() const {
        return m_audioStream && Pa_IsStreamStopped(m_audioStream) == 1;
    }

    Info Engine::info() const {
        assert(m_audioStream);
        return Info{ m_format, m_audioStream };
    }

    int Engine::callback(const void *inputBuffer, void *outputBuffer, unsigned long frameCount,
                         const PaStreamCallbackTimeInfo *timeInfo, PaStreamCallbackFlags statusFlags, void *userData) {
        auto& engine = *reinterpret_cast<Engine*>(userData);
        if(engine.m_format.inputChannels != 0){
            engine.readFromDevice(reinterpret_cast<const float*>(inputBuffer));
        }

        if(engine.m_format.outputChannels != 0){
            engine.writeToDevice(reinterpret_cast<float*>(outputBuffer));
        }

        return paContinue;
    }

    void Engine::sleep(std::chrono::milliseconds duration) {
        assert(m_audioStream);
        Pa_Sleep(as<long>(duration.count()));
    }

    void Engine::update() {
        static std::vector<float> buffer{};
        while(isActive()){
            std::unique_lock<std::mutex> lock{ m_mutex };

            std::cout << "waiting on data from input patches\n";
            requestAudioData.wait(lock);
            const auto available = m_mixer.maxNumberOfSamplesThatCanBePopped();
            std::cout << available <<  " available to read from patch\n";
            if(available != 0){
                buffer.resize(available);
                const auto read = m_mixer.popAudio(buffer.data(), available, false);
                m_outputBuffer.push(buffer.data(), read);
                std::cout << "read " << read << " from input patches\n";
            }
        }
    }
}