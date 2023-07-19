#pragma once

#include "audio.h"
#include "circular_buffer.h"
#include "patch_input.h"
#include <thread>
#include <span>

namespace audio {

    template<typename SampleGenerator>
    class StreamSource{
    public:
        StreamSource(PatchInput& patch, SampleGenerator sampleGenerator)
        : m_patchInput{patch}
        , m_sampleGenerator(std::move(sampleGenerator))
        {}

        ~StreamSource(){
            m_thread.join();
        }

        void play() {
            std::thread thread{ [&]{
                isRunning = true;

                audio::CircularAudioBuffer<float> buffer(1024);
                std::vector<float> transferBuf;

                while(isRunning && m_patchInput.isOpen()){
                    transferBuf.resize(buffer.remainder());
                    std::generate(transferBuf.begin(), transferBuf.end(), [&]{ return m_sampleGenerator(); });
                    buffer.push(transferBuf.data(), transferBuf.size());

                    transferBuf.resize(buffer.num());
                    buffer.pop(transferBuf.data(), transferBuf.size());

                    auto pushed = m_patchInput.pushAudio(audio::wrapMono(transferBuf.data(), transferBuf.size()));
                    auto remainder = transferBuf.size() - pushed;
                    buffer.setNum(remainder);
                }
            }};

            m_thread = std::move(thread);

        }

        void stop() {
            isRunning = false;
        }

    private:
        PatchInput& m_patchInput;
        SampleGenerator m_sampleGenerator;
        std::thread m_thread;
        std::atomic_bool isRunning{};
    };

    template<typename SampleType>
    class Clip{
    public:
        Clip(PatchInput& input, std::span<SampleType> span)
        : m_patchInput{ input }
        , m_source{ span }
        {}

        ~Clip(){
            m_thread.join();
        }

        void play(){
            std::thread thread{ [&]{
                isRunning = true;

                uint32_t capacity =  m_source.size();

                audio::CircularAudioBuffer<float> buffer(capacity);
                buffer.push(m_source.data(), capacity);
                std::vector<float> transferBuf;

                const auto limit = m_loop.load();
                auto loops = 0;

                for(auto i = 0; i < limit; i++){
                    buffer.setNum(capacity);
                    while (isRunning && m_patchInput.isOpen() && buffer.num() > 0) {
                        transferBuf.resize(buffer.num());
                        buffer.pop(transferBuf.data(), transferBuf.size());

                        auto pushed = m_patchInput.pushAudio(audio::wrapMono(transferBuf.data(), transferBuf.size()));
                        auto remainder = transferBuf.size() - pushed;
                        buffer.setNum(remainder);
                    }
                }
            }};

            m_thread = std::move(thread);
        }

        void loop(int value){
            m_loop = std::max(1, value);
        }

        void stop(){
            isRunning = false;
        }

    private:
        std::span<SampleType> m_source;
        std::atomic_int m_loop{1};
        std::thread m_thread;
        PatchInput& m_patchInput;
        std::atomic_bool isRunning{};
    };

    template<typename SampleGenerator>
    inline auto continuous(PatchInput& input, SampleGenerator&& sampleGenerator){
        return StreamSource{ input, sampleGenerator };
    }

    template<typename SampleType>
    inline auto clip(PatchInput& input, std::span<SampleType> source){
        return Clip{ input, source};
    }
}