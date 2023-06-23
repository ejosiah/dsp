#include <vui/vui.h>
#include <imgui.h>
#include <implot.h>
#include <array>
#include <random>

int main(int, char**){
    vui::config config{1000, 700, "Central Limit Theorem"};

    std::array<int, 20> events{};
    std::array<int, 100> groupEvents{};

    std::default_random_engine engine{std::random_device{}()};
    std::uniform_int_distribution<int> dist{0, events.size()};

    for(int i = 0; i < 1000000; i++){
        auto X = dist(engine);
        events[X]++;
        X += dist(engine);
        X += dist(engine);
        X += dist(engine);
        X += dist(engine);

        groupEvents[X]++;
    }

    vui::show(config, [&]{
        ImGui::Begin("Central Limit Theorem");
        ImGui::SetWindowSize({1100, 600});

        if(ImPlot::BeginPlot("random events", {500, 500})){
            ImPlot::PlotBars("random Events", events.data(), events.size());
            ImPlot::EndPlot();
            ImGui::SameLine();
        }
        if(ImPlot::BeginPlot("group events", {500, 500})){
            ImPlot::PlotBars("group Events", groupEvents.data(), groupEvents.size());
            ImPlot::EndPlot();
        }

        ImGui::End();
    });

    vui::wait();
}