#include <audio/engine.h>
#include <vui/vui.h>
#include <audio/choc_Oscillators.h>
#include <chrono>
#include <thread>
#include <iostream>
#include <audio/source.h>
#include <audio/choc_Oscillators.h>
#include <audio/choc_AudioFileFormat_WAV.h>

int main(int, char**){

    using namespace std::chrono_literals;

    audio::Engine engine{{0, 2, audio::SampleType::Float32, 48000, 512, 4096}};
    engine.start();

    const auto info = engine.info();
    const uint32_t maxLatency = 2048;
    auto output = engine.connectNewInput(maxLatency);
    choc::oscillator::Sine<float> osc{};
    osc.setFrequency(440, static_cast<float>(info.format().sampleRate));
    auto source = audio::continuous( output, [osc]() mutable { return osc.getSample() * 0.01f; });


    std::string audioPath = "../../../../resources/voice.wav";
    std::shared_ptr<std::istream> audioStream = std::make_shared<std::ifstream>(audioPath, std::ios::binary);

    choc::audio::WAVAudioFileFormat<false> audioFileFormat{};
    auto reader = audioFileFormat.createReader(audioStream);
    auto fileBuffer = reader->readEntireStream<float>();

    auto cOutput = engine.connectNewInput(maxLatency);
    auto iter = fileBuffer.getIterator(0);
    auto clip = audio::clip<float>(cOutput, std::span{ iter.sample, fileBuffer.getSize().numFrames });
    clip.loop(3);

//    source.play();
    clip.play();
//    source.stop();
    engine.sleep(10s);
    clip.stop();

    engine.shutdown();

    return 0;
}