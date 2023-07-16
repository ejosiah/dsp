#include <vui/vui.h>
#include <audio/engine.h>
#include <imgui.h>
#include <audio/choc_Oscillators.h>
#include <audio/choc_AudioFileFormat_WAV.h>
#include <dsp/recursive_filters.h>
#include <algorithm>
#include <numeric>
#include <implot.h>
#include <dsp/fft.h>
#include <complex>
#include <random>
#include "wind_generator.h"

#define SAMPLE_RATE (48000)
#define T (1.0/SAMPLE_RATE)
#define H (2.0 * dsp::PI)
#define kH (1000 * H)
#define FRAME_COUNT (512)

enum  SignalType : int { Sine = 0, Cosine, Square, Triangle };

struct Command{
    float frequency{440};
    float amplitude{1};
    int signalType{Sine};
};


std::vector<float> wind(std::chrono::seconds duration, int sampleRate) {

    WindGenerator windGenerator{ static_cast<uint32_t>(sampleRate) };
    Whistling wh0{ 400, 600, 1.2, 100, static_cast<uint32_t>(sampleRate), 0.12f };
    Whistling wh1{ 1000, 1000, 2.0, 1000, static_cast<uint32_t>(sampleRate) };
    TreeLeaves treeLeaves{static_cast<uint32_t>(sampleRate)};
    Howls howls0{ static_cast<uint32_t>(sampleRate), 100, std::make_tuple(0.35f, 0.6f), 0.5, 400, 40, 0.35f, 30.f, 200};
    Howls howls1{ static_cast<uint32_t>(sampleRate), 300, std::make_tuple(0.25f, 0.5f), 0.1, 200, 40, 0.25f, 20.f, 100};

//    std::vector<float> wind( duration.count() * sampleRate );
//    std::generate(wind.begin(), wind.end(), [&]{
//       return windGenerator.getSample() +  wh0.getSample() + wh1.getSample()
//             + howls0.getSample() + howls1.getSample() + treeLeaves.getSample();
//    });

    auto size = duration.count() * sampleRate;

    float left{}, right{};
    std::vector<float> wind{};
    for(int i = 0; i < size; i++){
        auto channels  = audio::fcpan(windGenerator.getSample(), 0.51);
        left = std::get<0>(channels);
        right = std::get<1>(channels);

        channels = audio::fcpan(wh0.getSample(), 0.28);
        left += std::get<0>(channels);
        right += std::get<1>(channels);

        channels = audio::fcpan(wh1.getSample(), 0.64);
        left += std::get<0>(channels);
        right += std::get<1>(channels);

        channels = audio::fcpan(treeLeaves.getSample(), 0.51);
        left += std::get<0>(channels);
        right += std::get<1>(channels);

        channels = audio::fcpan(howls0.getSample(), 0.91);
        left += std::get<0>(channels);
        right += std::get<1>(channels);

        channels = audio::fcpan(howls1.getSample(), 0.03);
        left += std::get<0>(channels);
        right += std::get<1>(channels);

        wind.push_back(left);
        wind.push_back(right);

    }

    return wind;
}

int main(int, char**){

    using namespace std::chrono_literals;

    std::string audioPath = "../../../../resources/voice.wav";
    std::shared_ptr<std::istream> audioStream = std::make_shared<std::ifstream>(audioPath, std::ios::binary);

    choc::audio::WAVAudioFileFormat<false> audioFileFormat{};
    auto reader = audioFileFormat.createReader(audioStream);
    auto props = reader->getProperties();

    auto fileBuffer = reader->readEntireStream<float>();
    auto iterator = fileBuffer.getIterator(0);
    std::vector<float> data{iterator.sample, iterator.sample + fileBuffer.getSize().getFrameRange().size()};

    std::cout << "numFrames: " << fileBuffer.getNumFrames() << "\n";
    std::cout << "numChannels: " << fileBuffer.getNumChannels() << "\n";

    float width = 1024;
    float height = 720;

    audio::Engine engine{{0, 2, audio::SampleType::Float32, SAMPLE_RATE, FRAME_COUNT}};
    engine.start();
    auto output = engine.connectNewInput((1 << 20));

    int sampleRate = 1 << 16;
    choc::oscillator::Sine<float> fc{};
    fc.setFrequency(400, sampleRate);

    choc::oscillator::Sine<float> fm{};
    fm.setFrequency(300, sampleRate);

    data.clear();
    data.resize(sampleRate * 0.25);
    std::generate(begin(data), end(data), [&] {
        auto sampleA = fc.getSample();
        auto sampleB = fm.getSample();

        auto am = sampleA * sampleB;

        auto sampleC = (sampleA + sampleB) * 0.5;

        return (am + sampleC) * 0.5;
    });

    data = wind(60s, SAMPLE_RATE);

//    const auto N = sampleRate;
    const auto N = data.size() * 2;


    auto log2n = static_cast<int>(std::ceil(std::log2(N)));
    auto sizePowerOf2 = static_cast<int>(std::pow(2, log2n));
    std::vector<std::complex<double>> fft(sizePowerOf2);
    std::vector<float> fMag(sizePowerOf2);

    dsp::fft(data.begin(), data.end(), fft.begin(), sizePowerOf2);

    std::transform(fft.begin(), fft.end(), fMag.begin(), [](const auto c){
        return std::abs(c);
    });

    vui::config config{1024, 720, "signal processing"};
    vui::show(config, [&]{
        float w = config.width;
        float h = config.height;


        ImGui::Begin("Signal processing");
        ImGui::SetWindowPos({0, 0});
        ImGui::SetWindowSize({w, h});

        if(ImPlot::BeginPlot("signal", {1024, 300})){
            ImPlot::PlotLine("signal", data.data(), data.size());
            ImPlot::EndPlot();
        }

        if(ImPlot::BeginPlot("fft", {1024, 300})){
            ImPlot::PlotLine("magnitude", fMag.data(), fMag.size()/2);
            ImPlot::EndPlot();
        }

        static int written = 0;
        static int leftOver = 0;
        constexpr auto numChannels = 2;
        if(leftOver != 0){
            written += output.pushAudio(audio::wrapInterLeaved(data.data() + written * numChannels, numChannels,  leftOver));
            leftOver = data.size()/numChannels - written;
            std::cout << "written: " << written << ", left over: " << leftOver << "\n";
        }

        if(ImGui::Button("Play")){
            written = output.pushAudio(audio::wrapInterLeaved(data.data(), numChannels, data.size()/numChannels));
            leftOver = data.size()/numChannels - written;
            std::cout << "written: " << written << ", left over: " << leftOver << "\n";
        }


        ImGui::End();
    });


    vui::wait();
    engine.shutdown();
    return 0;
}