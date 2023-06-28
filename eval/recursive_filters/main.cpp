#include <vui/vui.h>
#include <dsp/recursive_filters.h>
#include <imgui.h>
#include <implot.h>
#include <dsp/fft.h>

int main(int, char**){
    vui::config config{1020, 1000, "Recursive filters"};


    dsp::SampleBuffer<double> impulse(255);
    impulse[impulse.size()/2] = 1;

    auto impulseResponse = dsp::recursive::bandPassFilter(impulse, 0.25, 0.006);

    std::vector<std::complex<double>> freq(impulseResponse.size() + 1);

    dsp::fft(impulseResponse.begin(), impulseResponse.end(), freq.begin());

    dsp::SampleBuffer<double> freqMag;
    for(const auto& c : freq){
        freqMag.add(abs(c));
    }

    vui::show(config, [&]{
        ImGui::Begin("Recursive filters");
        ImGui::SetWindowSize({1000, 700});

        if(ImPlot::BeginPlot("Band pass filter", {500, 500})){
            ImPlot::PlotLine("Band pass filter", impulseResponse.data(), impulseResponse.size());
            ImPlot::EndPlot();
            ImGui::SameLine();
        }

        if(ImPlot::BeginPlot("Frequency Response", {500, 500})){
            ImPlot::PlotLine("FFT", freqMag.data(), freqMag.size()/2);
            ImPlot::EndPlot();
        }

        ImGui::End();
    });

    vui::wait();
}