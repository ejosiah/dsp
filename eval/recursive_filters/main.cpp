#include <vui/vui.h>
#include <dsp/recursive_filters.h>
#include <imgui.h>
#include <implot.h>
#include <dsp/fft.h>
#include <vui/blocking_queue.h>
#include <array>
#include <iostream>
#include <dsp/ring_buffer.h>
#include <portaudio.h>
#include <random>
#include <dsp/pink_noise.h>
#include <kfr/base.hpp>
#include <kfr/dsp.hpp>
#include <kfr/io.hpp>
#include <chrono>

using namespace kfr;


#define ERR_GUARD_PA(err) \
if(err != paNoError) {    \
    std::cerr <<  "PortAudio error: " <<   Pa_GetErrorText(err) << "\n"; \
    exit(err);               \
}

#define SAMPLE_RATE (44100)
#define T (1.0/SAMPLE_RATE)
#define H (2.0 * dsp::PI)
#define kH (1000 * H)
#define FRAME_COUNT (256)

enum FilterType { LOW_PASS = 0, HIGH_PASS, BAND_PASS, BAND_REJECT, NUM_FILTERS, NO_FILTER};
size_t ceil2(const int n){  return 1 << static_cast<size_t>(std::ceil(std::log2(n))); }

using Signal = dsp::SampleBuffer<double>;


struct Data{
    Signal impulseResponse;
    Signal frequencyResponse;
    Signal sFrequencyResponse;
};

struct Settings{
    float frequency{0.25};
    float bandwidth{0.006};
    int filterType{LOW_PASS};
    float volume{.01};
    bool on{false};
};

using DataRingBuffer = dsp::RingBuffer<Data, 256>;
using SettingsRingBuffer = dsp::RingBuffer<Settings, 256>;
using SignalRingBuffer = dsp::RingBuffer<Signal, 256>;

void computeFFT(Signal& signal, Signal& output, int nFrequency){
    auto fn = ceil2(nFrequency);

    Signal input(fn);
    std::memcpy(input.data(), signal.data(), signal.size());
    std::vector<std::complex<double>> frequency(fn);

    dsp::fft(input.begin(), input.end(), frequency.begin());

    output.clear();
    for(const auto& c : frequency){
        output.add(std::abs(c));
    }

}


Data createDate(Settings settings){
    constexpr int N = 1024;
    dsp::SampleBuffer<double> impulse(N);
    impulse[N/2] = 1;

    dsp::SampleBuffer<double> impulseResponse;

    if(settings.filterType == BAND_PASS){
        auto filter = dsp::recursive::bandPassFilter(settings.frequency, settings.bandwidth);
        impulseResponse = filter(impulse);
    }else if(settings.filterType == BAND_REJECT){
        auto filter = dsp::recursive::bandRejectFilter(settings.frequency, settings.bandwidth);
        impulseResponse = filter(impulse);
    }else if(settings.filterType == LOW_PASS){
        auto filter = dsp::recursive::lowPassFilter(settings.frequency);
        impulseResponse = filter(impulse);
    }else if(settings.filterType == HIGH_PASS){
        auto filter = dsp::recursive::highPassFilter(settings.frequency);
        impulseResponse = filter(impulse);
    }

    std::vector<std::complex<double>> freq(impulseResponse.size() + 1);

    dsp::fft(impulseResponse.begin(), impulseResponse.end(), freq.begin());
    dsp::SampleBuffer<double> freqMag;
    for(const auto& c : freq){
        freqMag.add(abs(c));
    }

    return { impulseResponse, freqMag };
}

struct Channels{
    float left;
    float right;
};

dsp::RingBuffer<dsp::SampleBuffer<double>, 256> noiseBuffer;

struct AudioData {
    SignalRingBuffer* signal{};
    SettingsRingBuffer* settings{};
    std::array<long long , 100> runtime{};
    int instance{0};
    float avgRuntime{0};
};
constexpr int Duration = 10; // seconds
constexpr int MaxSamples = SAMPLE_RATE * Duration;

Signal gSignal(MaxSamples);
Signal gSignalCopy;

static int paNoiseCallback(const void *inputBuffer,
                           void *outputBuffer,
                           unsigned long frameCount,
                           const PaStreamCallbackTimeInfo* timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void *userData ){
    auto start = std::chrono::steady_clock::now();
    static std::default_random_engine engine{std::random_device{}()};
    static std::normal_distribution<float> dist{0, 2};
    static double t = 0;
//    static auto sample = std::bind(dist, engine);
    static Settings settings{};
    static int nextSample = 0;
    static auto sample = [&]() {
        auto s = gSignal[nextSample++];
        nextSample %= MaxSamples;
        return s;
    };

    auto out = reinterpret_cast<Channels*>(outputBuffer);
    auto& data = *reinterpret_cast<AudioData*>(userData);
    auto& signalBuffer = *data.signal;
    auto& rbSettings = *data.settings;


    static Signal signal(frameCount);

    std::generate(signal.begin(), signal.end(), [&]{ return sample() * settings.volume; });

    if(auto opt = rbSettings.poll()){
        settings = *opt;
    }

//    if(settings.on){
//        if(settings.filterType == BAND_PASS){
//            signal = dsp::recursive::bandPassFilter(signal, double(settings.frequency), double (settings.bandwidth));
//        }else if(settings.filterType == BAND_REJECT){
//            signal = dsp::recursive::bandRejectFilter(signal, double(settings.frequency), double (settings.bandwidth));
//        }else if(settings.filterType == LOW_PASS){
//            signal = dsp::recursive::lowPassFilter(signal, double(settings.frequency));
//        }else if(settings.filterType == HIGH_PASS){
//            signal = dsp::recursive::highPassFilter(signal, double(settings.frequency));
//        }
//    }

    for(int i = 0; i < frameCount; i++){
        auto v = signal[i];
        out->left = v;
        out->right = v;
        out++;
    }
    signalBuffer.push(signal);
    auto duration = std::chrono::steady_clock::now() - start;
    data.runtime[data.instance++] = std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
    data.instance = data.instance%data.runtime.size();

    return paContinue;
}


PaStream* initAudio(AudioData& audioData){
    ERR_GUARD_PA(Pa_Initialize())

    PaStream* stream;
    ERR_GUARD_PA(Pa_OpenDefaultStream(&stream, 0, 2, paFloat32, SAMPLE_RATE, FRAME_COUNT, paNoiseCallback, &audioData))
    ERR_GUARD_PA(Pa_StartStream(stream))

    return stream;
}

void shutdownAudio(PaStream* stream){
    ERR_GUARD_PA(Pa_StopStream(stream))
    ERR_GUARD_PA(Pa_CloseStream(stream))
    ERR_GUARD_PA(Pa_Terminate())
}

int main(int, char**){
    vui::config config{1120, 1200, "Recursive filters"};

    SettingsRingBuffer rbSettings;
    DataRingBuffer rbData;
    SignalRingBuffer rbSignal;
    SettingsRingBuffer  rbAudioSettings;

    rbAudioSettings.push({});
    rbData.push(createDate({}));

    static std::default_random_engine engine{std::random_device{}()};
    static std::normal_distribution<float> dist{0, 2};
    static double t = 0;
    static auto sample = std::bind(dist, engine);

    std::generate(gSignal.begin(), gSignal.end(), [&]{ return sample(); });
    gSignalCopy = gSignal;
    AudioData audioData{&rbSignal, &rbAudioSettings};

    vui::show(config, [&]{

        static Settings settings{};
        static Data data;
        static bool dirty = false;
        static Signal signal;

        static std::array<const char*, NUM_FILTERS> filters{"low pass", "high pass", "band pass", "band reject"};

        if(auto opt = rbData.poll()){
            data = *opt;
        }

        ImGui::Begin("Recursive filters");
        ImGui::SetWindowSize({1100, 1200});

        dirty |= ImGui::Combo("filter", &settings.filterType, filters.data(), filters.size());
        ImGui::SameLine();
        dirty |= ImGui::Checkbox("enable", &settings.on);

        if(ImPlot::BeginPlot("Impulse Response", {500, 500})){
            ImPlot::PlotLine("Band pass filter", data.impulseResponse.data(), data.impulseResponse.size());
            ImPlot::EndPlot();
            ImGui::SameLine();
        }

        if(ImPlot::BeginPlot("Frequency Response", {500, 500})){
            ImPlot::PlotLine("FFT", data.frequencyResponse.data(), data.frequencyResponse.size()/2);
            ImPlot::EndPlot();
        }

        if(auto optSignal = rbSignal.poll()){
            signal = *optSignal;
            computeFFT(signal, data.sFrequencyResponse, SAMPLE_RATE);
        }

        if(ImPlot::BeginPlot("Signal", {500, 500})){
            ImPlot::PlotLine("Signal", signal.data(), signal.size());
            ImPlot::EndPlot();
            ImGui::SameLine();
        }

        if(ImPlot::BeginPlot("Signal FFT", {500, 500})){
            ImPlot::PlotLine("FFT", data.sFrequencyResponse.data(), data.sFrequencyResponse.size()/2);
            ImPlot::EndPlot();
        }

        dirty |= ImGui::SliderFloat("frequency", &settings.frequency, 0.0001, 0.5);
        if(settings.filterType == BAND_PASS || settings.filterType == BAND_REJECT) {
            dirty |= ImGui::SliderFloat("bandwidth", &settings.bandwidth, 0.0001, 0.5);
        }

        dirty |= ImGui::SliderFloat("volume", &settings.volume, 0.01, 1.0);
        ImGui::Text("audio callback runtime %s microseconds", std::to_string(audioData.avgRuntime).c_str());

        ImGui::End();

        if(dirty){
            dirty = false;
            rbSettings.push(settings);
        }

    });


    auto audioStream = initAudio(audioData);
    auto time = std::chrono::steady_clock::now();
    while(true){
        if (auto optSettings = rbSettings.poll()) {
            auto settings = *optSettings;

            if (settings.on) {
                if (settings.filterType == BAND_PASS) {
                    auto filter = dsp::recursive::bandPassFilter(settings.frequency, settings.bandwidth);
                    gSignal = filter(gSignalCopy);
                } else if (settings.filterType == BAND_REJECT) {
                    auto filter = dsp::recursive::bandRejectFilter(settings.frequency, settings.bandwidth);
                    gSignal = filter(gSignalCopy);
                } else if (settings.filterType == LOW_PASS) {
                    auto filter = dsp::recursive::lowPassFilter(settings.frequency);
                    gSignal = filter(gSignalCopy);

                } else if (settings.filterType == HIGH_PASS) {
                    auto filter = dsp::recursive::highPassFilter(settings.frequency);
                    gSignal = filter(gSignalCopy);
                }
            }

            rbAudioSettings.push(settings);
            rbData.push(createDate(settings));
        }
        if(!vui::isRunning()){
            break;
        }

        auto now = std::chrono::steady_clock::now();
        auto elapsedTime = now - time;
        if(std::chrono::duration_cast<std::chrono::seconds>(elapsedTime) >= std::chrono::seconds(1)){
            time = now;
            auto sum = static_cast<float>(std::accumulate(audioData.runtime.begin(), audioData.runtime.end(), 0LL));
            audioData.avgRuntime = sum/audioData.runtime.size();
        }
    }

    shutdownAudio(audioStream);

    vui::wait();
}