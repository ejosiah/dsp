#include <audio/engine.h>
#include <random>
#include <algorithm>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <dsp/dsp.h>
#define SAMPLE_RATE (48000)
#define T (1.0/SAMPLE_RATE)
#define H (2.0 * dsp::PI)
#define kH (1000 * H)
#define FRAME_COUNT (512)

int main(int, char**){
    audio::Format format{0, 2, audio::SampleType::Float32, SAMPLE_RATE, FRAME_COUNT};
    audio::Engine engine{format};
    engine.start();
    auto patchIn = engine.connectNewInput(2048);

    auto rng = [dist=std::uniform_real_distribution<float>{-1, 1}
                , engine=std::default_random_engine{std::random_device{}()}]() mutable {
        return dist(engine);
    };

    const auto N = SAMPLE_RATE * 5;
    std::vector<float> samples(N);
    std::generate(begin(samples), end(samples), [&]{ return rng() * 0.01; });
    //engine.m_outputBuffer.push(noise.data(), N);
    patchIn.pushAudio(samples.data(), N);

   engine.sleep(std::chrono::milliseconds(3000));

    for(int i = 0; i < N; i++){
        float t = i * T;
        samples[i] = std::cosf(2 * dsp::PI * 440 * t);
    }
    patchIn.pushAudio(samples.data(), N);


   engine.sleep(std::chrono::milliseconds(10000));

   engine.shutdown();



//    audio::CircularAudioBuffer<float> buffer(4);
//    std::vector<float> bucket(10);
//    std::iota(begin(bucket), end(bucket), 0);
//    auto pushed = buffer.push(bucket.data(), 6);
//
//    std::cout << "pushed " << pushed << "\n";
//    std::cout << buffer.remainder() << " available to write\n";
//    std::cout << buffer.num() << " available to read\n";
//
//    auto read = buffer.pop(bucket.data(), 4);
//    for(int i = 0; i < read; i++){
//        std::cout << bucket[i] << "\n";
//    }

    return 0;
}