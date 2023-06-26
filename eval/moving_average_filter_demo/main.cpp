#include <vui/vui.h>
#include <dsp/filter/filter.h>
#include <dsp/signal_buffer.h>
#include <dsp/fourier/series.h>
#include <imgui.h>
#include <implot.h>
#include <random>

int main(int, char**){

    vui::config config{720, 600, "moving average"};

    dsp::SampleBuffer<double> buffer =
            dsp::fourier::series::pulse(500, 1000, 500, 0.27, 1.0, 1.0, 1.0, 0.5);

    std::default_random_engine engine{ std::random_device{}()};
    std::uniform_real_distribution<double> dist{-1.0, 1.0 };

    for(auto itr = buffer.begin(); itr != buffer.end(); itr++){
        *itr += dist(engine) * 0.1;
    }

    vui::show(config, [&buffer]{
        ImGui::Begin("Pulse");
        ImGui::SetWindowSize({500, 500});

        static double filter_size;

        if(ImPlot::BeginPlot("Pulse", {450, 450})){
            ImPlot::PlotLine("pluse", buffer.data(), (int)buffer.size());
            ImPlot::EndPlot();
        }

        ImGui::End();
    });

    vui::wait();
}