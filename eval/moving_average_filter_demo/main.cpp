#include <vui/vui.h>
#include <dsp/filter.h>
#include <dsp/sample_buffer.h>
#include <dsp/fourier/series.h>
#include <imgui.h>
#include <implot.h>
#include <random>
#include <vui/blocking_queue.h>
#include <iostream>

int main(int, char**){
    vui::config config{720, 700, "moving average"};

    dsp::SampleBuffer<double> buffer =
            dsp::fourier::series::pulse(500, 1000, 500, 0.27, 1.0, 1.0, 1.0, 0.5);

    std::default_random_engine engine{ std::random_device{}()};
    std::uniform_real_distribution<double> dist{-1.0, 1.0 };

    for(double& sample : buffer){
        sample += dist(engine) * 0.1;
    }

    vui::SingleEntryBlockingQueue<dsp::SampleBuffer<double>> bQeueu{};
    bQeueu.put(dsp::SampleBuffer<double>(buffer));

    vui::SingleEntryBlockingQueue<int> sQueue;

    vui::show(config, [&]{
        ImGui::Begin("Pulse");
        ImGui::SetWindowSize({500, 550});

        static int filter_size = 0;
        static dsp::SampleBuffer<double> buffer = bQeueu.take();
        static bool dirty = false;

        if(ImPlot::BeginPlot("Pulse", {450, 450})){
            ImPlot::PlotLine("pulse", buffer.data(), (int)buffer.size());
            ImPlot::EndPlot();
        }
        dirty |= ImGui::SliderInt("moving average points", &filter_size, 1, 51);

        if(!ImGui::IsAnyItemActive() && dirty){
            dirty = false;
            sQueue.offer(filter_size);
        }

        if(auto newBuffer = bQeueu.poll()){
            buffer = *newBuffer;
        }

        ImGui::End();
    }, [&]{
        sQueue.offer(0);
    });

    dsp::filter::MovingAverageFilter filter{};

    while(true){
        auto size = sQueue.take();
        if(!vui::isRunning()){
            break;
        }
        filter.numPoints(size);
        auto filteredBuffer = filter(buffer);
        bQeueu.put(filteredBuffer);
    }

    vui::wait();
}