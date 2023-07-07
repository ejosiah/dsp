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

enum FilterType { LOW_PASS = 0, HIGH_PASS, BAND_PASS, BAND_REJECT, LOW_SHELF, HIGH_SHELF, PEAKING_FILTER, NUM_FILTERS, NO_FILTER};
size_t ceil2(const int n){  return 1 << static_cast<size_t>(std::ceil(std::log2(n))); }

using Signal = dsp::SampleBuffer<double>;


struct Data{
    Signal impulseResponse;
    Signal frequencyResponse;
    Signal sFrequencyResponse;
};

struct Settings{
    int filterType{NO_FILTER};
    float frequency{0.25};
    float bandwidth{0.006};
    float gain{1};
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



Signal applyFilter(const Signal& input, float frequency, float bandwidth, float gain, int filterType){
    Signal output;

    if(filterType == BAND_PASS){
        auto filter = dsp::recursive::bandPassFilter(frequency, bandwidth);
        output = filter(input);
    }else if(filterType == BAND_REJECT){
        auto filter = dsp::recursive::bandRejectFilter(frequency, bandwidth);
        output = filter(input);
    }else if(filterType == LOW_PASS){
        auto filter = dsp::recursive::lowPassFilter(frequency);
        output = filter(input);
    }else if(filterType == HIGH_PASS){
        auto filter = dsp::recursive::highPassFilter(frequency);
        output = filter(input);
    }else if(filterType == LOW_SHELF){
        auto filter = dsp::recursive::lowShelf(frequency, gain);
        output = filter(input);
    }else if(filterType == HIGH_SHELF){
        auto filter = dsp::recursive::highShelf(frequency, gain);
        output = filter(input);
    }else if(filterType == PEAKING_FILTER){
        auto filter = dsp::recursive::peakingFilter(frequency, gain, bandwidth);
        output = filter(input);
    }
    return output;
}


Data createDate(Settings settings){
    constexpr int N = 1024;
    Signal impulse(N);
    impulse[N/2] = 1;

    Signal impulseResponse = applyFilter(impulse, settings.frequency, settings.bandwidth
                                         , settings.gain, settings.filterType);

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
    double cpuLoad{0};
};
constexpr int Duration = 10; // seconds
constexpr int MaxSamples = SAMPLE_RATE * Duration;


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
    static double dt = 1.0/SAMPLE_RATE;
    static auto sample = std::bind(dist, engine);
    static Settings settings{};
    static int nextSample = 0;
//    static auto sample = [&]() {
//        auto f = 440.0;
//        auto s = std::cos(2.0 * dsp::PI * f * t);
//        s += std::cos(2.0 * dsp::PI * 2 * f * t);
//        s += std::cos(2.0 * dsp::PI * 3 * f * t);
//        s += std::cos(2.0 * dsp::PI * 4 * f * t);
//        s += std::cos(2.0 * dsp::PI * 5 * f * t);
//        s += std::cos(2.0 * dsp::PI * 6 * f * t);
//        t += dt;
//        return s;
//    };

    auto out = reinterpret_cast<Channels*>(outputBuffer);
    auto& data = *reinterpret_cast<AudioData*>(userData);
    auto& signalBuffer = *data.signal;
    auto& rbSettings = *data.settings;


    static Signal signal(frameCount);
    static dsp::CircularBuffer<double, FRAME_COUNT> oSignal{};

    std::generate(signal.begin(), signal.end(), [&]{ return sample() * settings.volume; });

    if(auto opt = rbSettings.poll()){
        settings = *opt;
    }

    if(settings.on){
        if(settings.filterType == BAND_PASS){
            auto filter = dsp::recursive::bandPassFilter(settings.frequency, settings.bandwidth);
            filter(signal, oSignal);
        }else if(settings.filterType == BAND_REJECT){
            auto filter = dsp::recursive::bandRejectFilter(settings.frequency, settings.bandwidth);
            filter(signal, oSignal);
        }else if(settings.filterType == LOW_PASS){
            auto filter = dsp::recursive::lowPassFilter(settings.frequency);
            filter(signal, oSignal);
        }else if(settings.filterType == HIGH_PASS){
            auto filter = dsp::recursive::highPassFilter(settings.frequency);
            filter(signal, oSignal);
        }else if(settings.filterType == LOW_SHELF){
            auto filter = dsp::recursive::lowShelf(settings.frequency, settings.gain);
            filter(signal, oSignal);
        }else if(settings.filterType == HIGH_SHELF){
            auto filter = dsp::recursive::highShelf(settings.frequency, settings.gain);
            filter(signal, oSignal);
        }else if(settings.filterType == PEAKING_FILTER){
            auto filter = dsp::recursive::peakingFilter(settings.frequency, settings.gain, settings.bandwidth);
            filter(signal, oSignal);
        }
        std::copy(oSignal.begin(), oSignal.end(), signal.begin());
    }

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
    rbData.push(createDate({LOW_PASS}));

    static double t = 0;
    static auto sample = [dist=std::normal_distribution<float>{0, 2}
                            , engine=std::default_random_engine{std::random_device{}()}]() mutable { return dist(engine); };

    AudioData audioData{&rbSignal, &rbAudioSettings};

    vui::show(config, [&]{

        static Settings settings{LOW_PASS};
        static Data data;
        static bool dirty = false;
        static Signal signal;

        static std::array<const char*, NUM_FILTERS> filters{
            "low pass", "high pass", "band pass", "band reject"
            , "low shelf", "high shelf", "peaking filter"};

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
        if(settings.filterType == BAND_PASS || settings.filterType == BAND_REJECT || settings.filterType == PEAKING_FILTER) {
            dirty |= ImGui::SliderFloat("bandwidth", &settings.bandwidth, 0.0001, 0.5);
        }

        if(settings.filterType == LOW_SHELF || settings.filterType == HIGH_SHELF || settings.filterType == PEAKING_FILTER){
            dirty |= ImGui::SliderFloat("gain", &settings.gain, 0.001, 10);
        }

        dirty |= ImGui::SliderFloat("volume", &settings.volume, 0.01, 1.0);
        ImGui::Text("audio callback runtime %s microseconds", std::to_string(audioData.avgRuntime).c_str());
        ImGui::Text("Cpu load: %s", std::to_string(audioData.cpuLoad).c_str());

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
        audioData.cpuLoad = Pa_GetStreamCpuLoad(audioStream);
    }

    shutdownAudio(audioStream);

    vui::wait();

/*    static std::default_random_engine engine{std::random_device{}()};
    static std::normal_distribution<float> dist{0, 2};
    static auto sample = std::bind(dist, engine);

    Signal signal(100);
    std::generate(signal.begin(), signal.end(), [&]{ return sample(); });

    auto filter = dsp::recursive::lowPassFilter(0.25);

    auto out = filter(signal);*/


}