#include <vui/vui.h>
#include <imgui.h>
#include <implot.h>
#include <thread>
#include <array>
#include <chrono>
#include <numeric>
#include <cmath>
#include <dsp/ring_buffer.h>
#include <iostream>
#include <dsp/dsp.h>
#include <complex>
#include <vui/blocking_queue.h>
#include <dsp/filter.h>
#include <dsp/fft.h>

static constexpr float PI = 3.1415926535897932384626433832795;
static constexpr float TWO_PI = 6.283185307179586476925286766559;

enum  WindowType  { Hamming = 0, Blackman, None };

enum FilterType{ LOW_PASS = 0, HIGH_PASS, BAND_PASS, BAND_REJECT };

constexpr double SampleRate = 1000.f;


struct Settings {
    float cutoffFrequency{0.5};
    int length{128};
    int windowType{WindowType::None};
    int filterType{LOW_PASS};
    int inversionType{static_cast<int>(dsp::InversionType::None)};
    bool applyFilter{false};
    bool frDecibels{false};
};

struct Data {
    std::vector<double> x;
    std::vector<double> y;
    std::vector<double> mag;
    std::vector<double> fx;
    std::vector<double> phase;
    std::vector<double> signal;
    std::vector<double> signalFFt;
    std::vector<double> sx;
};

void createSignal(Data& data, int padding){
    constexpr auto period = 1/SampleRate;
    auto pd = period * padding;
    const auto N = SampleRate + padding;
    for(int i = 0; i < N; i++){
        auto t = period * i - pd;
        auto signal = 0.0;
        signal += std::cos(TWO_PI * 2 * t);
        signal += std::cos(TWO_PI * 10 * t) * 0.5;
        signal += std::cos(TWO_PI * 50 * t) * 0.25;
        signal += std::cos(TWO_PI * 250 * t) * 0.125;
        signal += std::cos(TWO_PI * 450 * t) * 0.0625;

        data.signal.push_back(signal);
        data.sx.push_back(t);
    }

}

template<dsp::InversionType InversionType = dsp::InversionType::None>
Data create(Settings settings){
    int M = settings.length;
    float fc = settings.cutoffFrequency;

    std::cout << "creating data points for fc: " << fc << " and M: " << M << "\n";

    Data data{};

    dsp::filter::SincFilter<InversionType> filter{fc, size_t(M)};

    data.y = filter.kernel();
    data.x = std::vector<double>(data.y.size());
    std::iota(data.x.begin(), data.x.end(), 0);



    auto n = 1 << static_cast<int>(std::ceil(std::log2(M)));
    std::vector<std::complex<double>> fd(n);

    dsp::fft(data.y.begin(), data.y.end(), fd.begin() );

    for(int i = 0; i < fd.size(); i++){
        double x = static_cast<double>(i)/static_cast<double>(fd.size());
        data.fx.push_back(x);
    }

    for(const auto c : fd){
        auto mag = std::abs(c);
        if(settings.frDecibels){
            mag = std::log10(mag);
        }
        data.mag.push_back(mag);
    }

    createSignal(data, filter.kernel().size());

    if(settings.applyFilter){
        dsp::SampleBuffer<double> buffer{data.signal.begin(), data.signal.end()};
        buffer = filter.apply(buffer);
        data.signal = std::vector<double>{buffer.begin(), buffer.end()};
    }

    auto offset = filter.kernel().size();
    n = data.signal.size() - offset;
    n = 1 << static_cast<int>(std::ceil(std::log2(n)));
    fd = std::vector<std::complex<double>>(n);
    auto first = data.signal.begin();
    auto last = data.signal.end();
    std::advance(first, offset);
    dsp::fft(first, last, fd.begin());

    for(const auto& c : fd){
        data.signalFFt.push_back(abs(c));
    }

    return data;
}


using SettingsQueue = vui::SingleEntryBlockingQueue<Settings>;
using DataQueue = vui::SingleEntryBlockingQueue<Data>;

int main(int, char**){
    vui::config config{1280, 1700, "Windowed Sinc filter"};

    SettingsQueue sQueue{};
    DataQueue dQueue;
    dQueue.offer(create(Settings{0.25, 200}));

    vui::show(config, [&]{

        static bool dirty = false;

        static Settings settings{0.25, 200};

        static std::array<const char*, 3> windows{"Hamming", "Blackman", "None"};
        static Data data;
        
        
        if(auto mData = dQueue.poll()){
            settings.length = mData->x.size();
            data = *mData;
            std::cout << "M :" << settings.length << "\n";
        }


        ImGui::Begin("Graph");
        ImGui::SetWindowSize({1220, 1650});
        if(ImPlot::BeginPlot("Sinc filter", {600, 500})){
            ImPlot::PlotLine("Sinc", data.x.data(), data.y.data(), data.x.size());
            ImPlot::EndPlot();
            ImGui::SameLine();
        }

        if(ImPlot::BeginPlot("frequency", {600, 500})){
            ImPlot::PlotLine("Frequency", data.fx.data(), data.mag.data(), data.mag.size()/2);
            ImPlot::EndPlot();
        }

        if(ImPlot::BeginPlot("Signal", {600, 500})){
            auto offset = data.x.size();
            ImPlot::PlotLine("Signal", data.sx.data() + offset, data.signal.data() + offset, data.signal.size() - offset);
            ImPlot::EndPlot();
            ImGui::SameLine();
        }

        if(ImPlot::BeginPlot("Signal FFT", {600, 500})){
            ImPlot::PlotLine("FFT", data.signalFFt.data(), data.signalFFt.size()/2);
            ImPlot::EndPlot();
        }

        ImGui::Text("Filter:");
        ImGui::Indent(16);
        dirty |= ImGui::RadioButton("Low pass", &settings.filterType, LOW_PASS);
        ImGui::SameLine();
        dirty |= ImGui::RadioButton("High pass", &settings.filterType, HIGH_PASS);
        ImGui::SameLine();
        dirty |= ImGui::RadioButton("Band pass", &settings.filterType, BAND_PASS);
        ImGui::SameLine();
        dirty |= ImGui::RadioButton("Band reject", &settings.filterType, BAND_REJECT);
        ImGui::Indent(-16);
        ImGui::Separator();

        ImGui::Text("Settings:");
        ImGui::Indent(16);
        if(settings.filterType == HIGH_PASS){
            if(settings.inversionType == static_cast<int>(dsp::InversionType::None)){
                settings.inversionType = static_cast<int>(dsp::InversionType::SpectralInversion);
            }
            ImGui::Text("Inversion Type:");
            ImGui::SameLine();
            dirty |= ImGui::RadioButton("Spectral Inversion", &settings.inversionType, static_cast<int>(dsp::InversionType::SpectralInversion));
            ImGui::SameLine();
            dirty |= ImGui::RadioButton("Spectral reversal", &settings.inversionType, static_cast<int>(dsp::InversionType::SpectralReversal));
        }else {
            settings.inversionType = static_cast<int>(dsp::InversionType::None);
        }


        dirty |= ImGui::SliderFloat("cutoff frequency", &settings.cutoffFrequency, 0, 0.5);
        dirty |= ImGui::SliderInt("length", &settings.length, 0, 400);
        dirty |= ImGui::Combo("window", &settings.windowType, windows.data(), 3);

        dirty |= ImGui::Checkbox("filter signal", &settings.applyFilter);
        ImGui::SameLine();
        dirty |= ImGui::Checkbox("Decibels", &settings.frDecibels);
        ImGui::Indent(-16);

        if(!ImGui::IsAnyItemActive() && dirty){
            dirty = false;
            sQueue.offer(settings);
        }

        ImGui::End();
    }, [&]{
        std::cout << "UI shutting down releasing ui wait lock\n";
        sQueue.offer(Settings{});
    });


    while(true){
        std::cout << "waiting on ui update\n";
        auto settings = sQueue.take();

        if(!vui::isRunning()){
            break;
        }
        std::cout << "ui updated\n";
        Data data;
        if(settings.inversionType == static_cast<int>(dsp::InversionType::SpectralInversion)){
            data = create<dsp::InversionType::SpectralInversion>(settings);
        }else if(settings.inversionType == static_cast<int>(dsp::InversionType::SpectralReversal)){
            data = create<dsp::InversionType::SpectralReversal>(settings);
        }else{
            data = create(settings);
        }
        dQueue.put(data);

    }

    vui::wait();

}