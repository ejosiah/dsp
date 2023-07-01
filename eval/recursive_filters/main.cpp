#include <vui/vui.h>
#include <dsp/recursive_filters.h>
#include <imgui.h>
#include <implot.h>
#include <dsp/fft.h>
#include <vui/blocking_queue.h>
#include <dsp/chebyshev.h>
#include <array>
#include <iostream>
#include <dsp/ring_buffer.h>
#include <portaudio.h>
#include <random>
#include <dsp/pink_noise.h>
#include <kfr/base.hpp>
#include <kfr/dsp.hpp>
#include <kfr/io.hpp>

#define ERR_GUARD_PA(err) \
if(err != paNoError) {    \
    std::cerr <<  "PortAudio error: " <<   Pa_GetErrorText(err) << "\n"; \
    exit(err);               \
}

#define SAMPLE_RATE (44100)

enum FilterType { LOW_PASS = 0, HIGH_PASS, BAND_PASS, BAND_REJECT, NUM_FILTERS, NO_FILTER};

struct Data{
    dsp::SampleBuffer<double> impulseResponse;
    dsp::SampleBuffer<double> frequencyResponse;
};

struct Settings{
    float frequency{0.25};
    float bandwidth{0.006};
    int filterType{LOW_PASS};
    float volume{.01};
    bool on{false};
};

using Signal = dsp::SampleBuffer<double>;
using DataRingBuffer = dsp::RingBuffer<Data, 256>;
using SettingsRingBuffer = dsp::RingBuffer<Settings, 256>;
using SignalRingBuffer = dsp::RingBuffer<Signal, 256>;

Data createDate(Settings settings){
    constexpr int N = 511;
    dsp::SampleBuffer<double> impulse(N);
    impulse[N/2] = 1;

    dsp::SampleBuffer<double> impulseResponse;

    if(settings.filterType == BAND_PASS){
        impulseResponse = dsp::recursive::bandPassFilter(impulse, double(settings.frequency), double (settings.bandwidth));
    }else if(settings.filterType == BAND_REJECT){
        impulseResponse = dsp::recursive::bandRejectFilter(impulse, double(settings.frequency), double (settings.bandwidth));
    }else if(settings.filterType == LOW_PASS){
        impulseResponse = dsp::recursive::lowPassFilter(impulse, double(settings.frequency));
    }else if(settings.filterType == HIGH_PASS){
        impulseResponse = dsp::recursive::highPassFilter(impulse, double(settings.frequency));
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
    SignalRingBuffer* signal;
    SettingsRingBuffer* settings;
};

static int paNoiseCallback(const void *inputBuffer,
                           void *outputBuffer,
                           unsigned long frameCount,
                           const PaStreamCallbackTimeInfo* timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void *userData ){

    static std::default_random_engine engine{std::random_device{}()};
    static std::uniform_real_distribution<float> dist{-1.f, 1.f};
    static auto sample = std::bind(dist, engine);
    static Settings settings{};
//    static auto sample = [state=kfr::random_state{}]() mutable  {
//        kfr::gen_random_normal<float>(state);
//    };

    auto out = reinterpret_cast<Channels*>(outputBuffer);
    auto data = *reinterpret_cast<AudioData*>(userData);
    auto& signalBuffer = *data.signal;
    auto& rbSettings = *data.settings;


    static Signal signal(frameCount);

    std::generate(signal.begin(), signal.end(), [&]{ return sample() * settings.volume; });

    if(auto opt = rbSettings.poll()){
        settings = *opt;
    }

    if(settings.on){
        if(settings.filterType == BAND_PASS){
            signal = dsp::recursive::bandPassFilter(signal, double(settings.frequency), double (settings.bandwidth));
        }else if(settings.filterType == BAND_REJECT){
            signal = dsp::recursive::bandRejectFilter(signal, double(settings.frequency), double (settings.bandwidth));
        }else if(settings.filterType == LOW_PASS){
            signal = dsp::recursive::lowPassFilter(signal, double(settings.frequency));

            using namespace kfr;
            zpk<fbase> filt  = iir_lowpass(chebyshev1<fbase>(8, 2), settings.frequency);

        }else if(settings.filterType == HIGH_PASS){
            signal = dsp::recursive::highPassFilter(signal, double(settings.frequency));
        }
    }

    for(int i = 0; i < frameCount; i++){
        auto v = signal[i];
        out->left = v;
        out->right = v;
        out++;
    }
    signalBuffer.push(signal);

    return paContinue;
}


PaStream* initAudio(AudioData& audioData){
    ERR_GUARD_PA(Pa_Initialize())

    PaStream* stream;
    ERR_GUARD_PA(Pa_OpenDefaultStream(&stream, 0, 2, paFloat32, SAMPLE_RATE, 256, paNoiseCallback, &audioData))
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
        ImGui::SetWindowSize({1100, 1100});

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
        }

        if(ImPlot::BeginPlot("Signal", {1010, 500})){
            ImPlot::PlotLine("Signal", signal.data(), signal.size());
            ImPlot::EndPlot();
        }

        dirty |= ImGui::SliderFloat("frequency", &settings.frequency, 0.0001, 0.5);
        if(settings.filterType == BAND_PASS || settings.filterType == BAND_REJECT) {
            dirty |= ImGui::SliderFloat("bandwidth", &settings.bandwidth, 0.0001, 0.5);
        }

        dirty |= ImGui::SliderFloat("volume", &settings.volume, 0.01, 1.0);

        ImGui::End();

        if(dirty){
            dirty = false;
            rbSettings.push(settings);
        }

    });

    AudioData audioData{&rbSignal, &rbAudioSettings};

    auto audioStream = initAudio(audioData);

    while(true){
        if(auto optSettings = rbSettings.poll()){
            auto settings = *optSettings;
            rbAudioSettings.push(settings);
            rbData.push(createDate(settings));
        }
        if(!vui::isRunning()){
            break;
        }
    }

    shutdownAudio(audioStream);

    vui::wait();
}