#include <vui/vui.h>
#include <dsp/recursive_filters.h>
#include <imgui.h>
#include <implot.h>
#include <dsp/fft.h>
#include <vui/blocking_queue.h>
#include <array>

enum FilterType { LOW_PASS = 0, HIGH_PASS, BAND_PASS, BAND_REJECT, NUM_FILTERS};

struct Data{
    dsp::SampleBuffer<double> impulseResponse;
    dsp::SampleBuffer<double> frequencyResponse;
};

struct Settings{
    float frequency{0.25};
    float bandwidth{0.006};
    int filterType{BAND_PASS};
};

using DataQueue = vui::SingleEntryBlockingQueue<Data>;
using SettingsQueue = vui::SingleEntryBlockingQueue<Settings>;

Data bandPass(Settings settings){
    dsp::SampleBuffer<double> impulse(511);
    impulse[impulse.size()/2] = 1;

    auto impulseResponse =
            dsp::recursive::bandPassFilter(impulse, double(settings.frequency), double (settings.bandwidth));

    std::vector<std::complex<double>> freq(impulseResponse.size() + 1);

    dsp::fft(impulseResponse.begin(), impulseResponse.end(), freq.begin());
    dsp::SampleBuffer<double> freqMag;
    for(const auto& c : freq){
        freqMag.add(abs(c));
    }

    return { impulseResponse, freqMag };
}

int main(int, char**){
    vui::config config{1020, 1000, "Recursive filters"};

    DataQueue dQueue;
    SettingsQueue sQueue;

    dQueue.offer(bandPass({}));

    vui::show(config, [&]{

        static Settings settings{};
        static Data data;
        static bool dirty = false;

        static std::array<const char*, NUM_FILTERS> filters{"low pass", "high pass", "band pass", "band reject"};

        if(auto opt = dQueue.poll()){
            data = *opt;
        }

        ImGui::Begin("Recursive filters");
        ImGui::SetWindowSize({1000, 700});

        dirty |= ImGui::Combo("filter", &settings.filterType, filters.data(), filters.size());

        if(ImPlot::BeginPlot("Impulse Response", {500, 500})){
            ImPlot::PlotLine("Band pass filter", data.impulseResponse.data(), data.impulseResponse.size());
            ImPlot::EndPlot();
            ImGui::SameLine();
        }

        if(ImPlot::BeginPlot("Frequency Response", {500, 500})){
            ImPlot::PlotLine("FFT", data.frequencyResponse.data(), data.frequencyResponse.size()/2);
            ImPlot::EndPlot();
        }
        dirty |= ImGui::SliderFloat("frequency", &settings.frequency, 0.0001, 0.5);
        dirty |= ImGui::SliderFloat("bandwidth", &settings.bandwidth, 0.0001, 0.5);

        ImGui::End();

        if(dirty){
            dirty = false;
            sQueue.put(settings);
        }

    }, [&]{
        sQueue.put({});
    });

    while(true){
        auto settings = sQueue.take();
        if(!vui::isRunning()){
            break;
        }
        dQueue.put(bandPass(settings));
    }

    vui::wait();
}