#include <audio/engine.h>
#include <audio/choc_Oscillators.h>
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
    audio::Format format{0, 2, audio::SampleType::Float32, SAMPLE_RATE, FRAME_COUNT, SAMPLE_RATE};
    audio::Engine engine{format};
    engine.start();
//    auto patchIn0 = engine.connectNewInput(2048);
    auto patchIn1 = engine.connectNewInput(SAMPLE_RATE/10);

    auto rng = [dist=std::uniform_real_distribution<float>{-1, 1}
                , engine=std::default_random_engine{std::random_device{}()}]() mutable {
        return dist(engine);
    };



//    std::thread t0{[&]{
//        const auto N = 1024;
//        std::vector<float> samples(N);
//        while(engine.isActive()) {
//            std::generate(begin(samples), end(samples), [&] { return rng() * 0.01; });
//            patchIn0.pushAudio(samples.data(), N);
//            std::this_thread::sleep_for(std::chrono::milliseconds(2));
//        }
//    }};

    choc::oscillator::Sine<float> oscillator;
    oscillator.setFrequency(2500, SAMPLE_RATE);
    choc::oscillator::Square<float> pause;
    pause.setFrequency(5, SAMPLE_RATE);

    std::thread t1{[&]{
        const auto info = engine.info();
//        const int N = SAMPLE_RATE;
        const int N = 1024;
        std::vector<float> samples(N);

        while(info.isActive()){
            for(int i = 0; i < N; i++){
                auto sample = std::cosf(2.f * dsp::PI * (i * T) * 2500) * 0.01;
                float amp = (i/(SAMPLE_RATE/10))%2;
//                std::cout << "i: " << i << " amp: " << amp << "\n";
                samples[i] = oscillator.getSample() * 0.01 * (.5 * pause.getSample() + .5);
            }
            patchIn1.pushAudio(samples.data(), N);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));

        }
    }};



   engine.sleep(std::chrono::milliseconds(5000));
   engine.shutdown();

//    t0.join();
    t1.join();




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