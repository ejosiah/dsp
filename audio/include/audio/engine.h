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
#include "audio.h"

#define ERR_GUARD_PA(err) \
if((err) != paNoError) {    \
    std::cerr <<  "PortAudio error: " <<   Pa_GetErrorText(err) << "\n"; \
    exit(err);               \
}

namespace audio {

    class Engine {
    public:
        Engine(Format format);

        ~Engine();

        void start();

        void shutdown();

        PatchInput connectNewInput(uint32_t maxLatencyInSamples);

        PatchOutputStrongPtr connectNewOutput(uint32_t maxLatencyInSamples);

        PatchOutputStrongPtr outputTap(uint32_t maxLatencyInSamples);

        void writeToDevice(float* out);

        void readFromDevice(const float* in);

        bool isActive() const;

        bool isStopped() const;

        uint32_t sampleRate() const;

        uint32_t outputChannels() const;

        uint32_t inputChannels() const;

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


        static uint32_t computeSize(Format format, uint32_t channels);

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
        PatchSplitter m_tap{};
        std::mutex m_mutex;
        std::condition_variable requestAudioData;
    };

    Engine::Engine(Format format)
    : m_format{ format }
    , m_outputBuffer{ computeSize(format, format.outputChannels)}
    , m_inputBuffer{ computeSize(format, format.inputChannels)}
    {}

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

        m_mixer.set(info());
        m_splitter.set(info());
        m_tap.set(info());

        m_updateThread = std::move(std::thread{ &Engine::update, this });
    }

    void Engine::shutdown() {
        if(m_audioStream) {
            ERR_GUARD_PA(Pa_StopStream(m_audioStream))
            ERR_GUARD_PA(Pa_CloseStream(m_audioStream))
            ERR_GUARD_PA(Pa_Terminate())
            m_audioStream = nullptr;
            requestAudioData.notify_one();

            if(m_updateThread.joinable()) {
                m_updateThread.join();
            }
        }
    }

    PatchInput Engine::connectNewInput(uint32_t maxLatencyInSamples) {
        return m_mixer.addNewInput(maxLatencyInSamples, 1);
    }

    PatchOutputStrongPtr Engine::connectNewOutput(uint32_t maxLatencyInSamples) {
        return {};
    }

    PatchOutputStrongPtr Engine::outputTap(uint32_t maxLatencyInSamples) {
        return m_tap.addNewPatch(maxLatencyInSamples, 1);
    }

    void Engine::writeToDevice(float *out) {
        requestAudioData.notify_one();

        const auto size = m_format.frameBufferSize * m_format.outputChannels;
        std::fill_n(out, size, 0.f);
        static std::vector<real_t> bucket(size);

        auto read = m_outputBuffer.pop(bucket.data(), size);

        auto in = bucket.data();

        for(int i = 0; i < read; i += m_format.outputChannels){
            for(int c = 0; c < m_format.outputChannels; ++c){
                *out = *in;
                ++in, ++out;
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

    uint32_t Engine::computeSize(Format format, uint32_t channels){
        auto defaultSize = format.sampleRate * channels;
        auto size = std::max(defaultSize, format.audioBufferSize * channels);
        size = alignedSize(size, format.frameBufferSize * channels);
        return size;
    }

    void Engine::sleep(std::chrono::milliseconds duration) {
        assert(m_audioStream);
        Pa_Sleep(as<long>(duration.count()));
    }

    void Engine::update() {
        static std::vector<float> buffer{};
        static int32_t read = 0;

        while(true){
            std::unique_lock<std::mutex> lock{ m_mutex };
            requestAudioData.wait(lock);

            if(!isActive()) break;

            const auto available = m_mixer.maxNumberOfSamplesThatCanBePopped();

            if (available > 0) {
                auto readSize = std::min(as<int32_t>(m_outputBuffer.remainder()), (available));
                buffer.resize(readSize);

                if(readSize%m_format.outputChannels != 0) {
                    std::cout << "invalid readSize: " << readSize << "\n";
                    assert(readSize%m_format.outputChannels == 0);
                }

                read = m_mixer.popAudio(buffer.data(), readSize, false);

                m_outputBuffer.push(buffer.data(), read);
                m_tap.pushAudio(buffer.data(), read);
            }

        }
    }

    uint32_t Engine::sampleRate() const {
        return m_format.sampleRate;
    }

    uint32_t Engine::outputChannels() const {
        return m_format.outputChannels;
    }

    uint32_t Engine::inputChannels() const {
        return m_format.inputChannels;
    }
}