#include <audio/engine.h>
#include <audio/choc_Oscillators.h>
#include <audio/choc_AudioFileFormat_WAV.h>
#include <random>
#include <algorithm>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <dsp/dsp.h>
#include <audio/source.h>
#include <memory>
#include <chrono>
#include "wind_generator.h"

#define SAMPLE_RATE (48000)
#define T (1.0/SAMPLE_RATE)
#define H (2.0 * dsp::PI)
#define kH (1000 * H)
#define FRAME_COUNT (512)

enum class Choice : int { Noise = 1, SineWave, Wind, AudioPlayback};

int main(int argc, char** argv){
    using namespace std::chrono_literals;

    if(argc == 1){
        std::cout << "usage: [1|2|3] 1 for noise, 2 for sine wave, 3 for Wind, 4 for audio playback";
        exit(0);
    }

    Choice selection = static_cast<Choice>(std::stoi(argv[1]));

    audio::Format format{0, 2, audio::SampleType::Float32, SAMPLE_RATE, FRAME_COUNT, SAMPLE_RATE};
    audio::Engine engine{format};
    engine.start();

    auto output = engine.connectNewInput(2048);

    switch(selection){
        case Choice::Noise: {
            auto dist = std::uniform_real_distribution<float>(-1, 1);
            auto eng = std::default_random_engine{ std::random_device{}() };
            auto rng = [dist, eng ]() mutable {
                return dist(eng);
            };

            auto source = audio::continuous(output, [rng]() mutable { return rng(); });
            source.play();
            engine.sleep(10s);
            source.stop();
            break;
        }
        case Choice::SineWave: {
            choc::oscillator::Sine<float> osc{};
            osc.setFrequency(440, SAMPLE_RATE);

            auto source = audio::continuous(output, [osc]() mutable { return osc.getSample(); });
            source.play();
            engine.sleep(10s);
            source.stop();
            break;
        }
        case Choice::Wind: {
            WindGenerator windGenerator{ static_cast<uint32_t>(SAMPLE_RATE) };
            Whistling wh0{ 400, 600, 1.2, 100, static_cast<uint32_t>(SAMPLE_RATE), 0.12f };
            Whistling wh1{ 1000, 1000, 2.0, 1000, static_cast<uint32_t>(SAMPLE_RATE) };
            TreeLeaves treeLeaves{static_cast<uint32_t>(SAMPLE_RATE)};
            Howls howls0{ static_cast<uint32_t>(SAMPLE_RATE), 100, std::make_tuple(0.35f, 0.6f), 0.5, 400, 40, 0.35f, 30.f, 200};
            Howls howls1{ static_cast<uint32_t>(SAMPLE_RATE), 300, std::make_tuple(0.25f, 0.5f), 0.1, 200, 40, 0.25f, 20.f, 100};

            auto source = audio::continuous(output, [&]() mutable {
               return windGenerator.getSample() +  wh0.getSample() + wh1.getSample()
                        + howls0.getSample() + howls1.getSample() + treeLeaves.getSample();
            });
            source.play();
            engine.sleep(60s);
            source.stop();
            break;
        }
        case Choice::AudioPlayback: {
            std::string audioPath = "../../../../resources/voice.wav";
            std::shared_ptr<std::istream> audioStream = std::make_shared<std::ifstream>(audioPath, std::ios::binary);

            choc::audio::WAVAudioFileFormat<false> audioFileFormat{};
            auto reader = audioFileFormat.createReader(audioStream);
            auto buffer = reader->readEntireStream<float>();
            auto iter = buffer.getIterator(0);
            auto clip = audio::clip(output, std::span{ iter.sample, buffer.getSize().numFrames });
            clip.loop(3);
            clip.play();
            engine.sleep(10s);
            break;
        }
    }


    engine.shutdown();

    return 0;
}