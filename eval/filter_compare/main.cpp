#include <vui/vui.h>
#include <dsp/filter.h>
#include <dsp/sample_buffer.h>
#include <dsp/fourier/series.h>
#include <imgui.h>
#include <implot.h>
#include <random>
#include <vui/blocking_queue.h>
#include <iostream>

using Signal = dsp::SampleBuffer<double>;

int main(int, char**){
    vui::config config{1080, 720, "filter compare"};

    Signal cs{401, 0.0};
    cs[300] = 1;

    dsp::filter::SincFilter sincFilter{0.14, 200};
    auto sKernel = sincFilter.apply(cs);

    vui::show(config, [&]{
        ImGui::Begin("Filter Compare");
        ImGui::SetWindowSize({700, 700});

        if(ImPlot::BeginPlot("Sinc Filter", {500, 500})){
            ImPlot::PlotLine("SincFilter", sKernel.data(), sKernel.size());
            ImPlot::EndPlot();
        }

        ImGui::End();
    });

    vui::wait();
}