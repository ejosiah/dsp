#include <vui/vui.h>
#include <imgui.h>
#include <implot.h>
#include <thread>
#include <array>
#include <chrono>
#include <numeric>
#include <algorithm>
#include <cmath>
#include <dsp/ring_buffer.h>
#include <dsp/fft.h>
#include <memory>
#include <iostream>
#include <dsp/dsp.h>
#include <dsp/filter.h>
#include <complex>
#include <condition_variable>

static constexpr float PI = 3.1415926535897932384626433832795;
static constexpr float TWO_PI = 6.283185307179586476925286766559;

enum class SignalType : int { Impulse = 0, RectangularPulse, Sinc };

struct Settings {
    int shift{0};
    float f{0.25};
    SignalType signalType{SignalType::Sinc};
};

struct Data {
    std::vector<float> x;
    std::vector<float> y;
    std::vector<float> fx;
    std::vector<float> mag;
    std::vector<float> phase;
    std::vector<float> real;
    std::vector<float> img;
};

void update(Data& data, Settings settings){
    data.x.clear();
    data.y.clear();
    data.fx.clear();
    data.mag.clear();
    data.phase.clear();
    data.real.clear();
    data.img.clear();

    auto init = [](Data& data, int n){
        data.x.resize(n);
        data.y.resize(n);

        std::iota(data.x.begin(), data.x.end(), 0);
    };

    int N;

    switch(settings.signalType){
        case SignalType::Impulse:
            N = 64;
            init(data, N);
            data.y[settings.shift] = 1;
            break;
        case SignalType::RectangularPulse:
            N = 128;
            init(data, N);

            for(int offset = -5; offset <= 5; offset++){
                auto index = (N + settings.shift + offset)%N;
                data.y[index] = 1;
            }
            break;
        case SignalType::Sinc:
            N = 128;
            init(data, N);
            auto buffer = dsp::sinc(0.14, 200);
            for(int i = 0; i < N; i++){
                auto x = (static_cast<float>(i)/static_cast<float>(N) - 0.5f) * 10;
                auto f = settings.f ;
                auto xx = PI * f * x;
                data.y[i] = xx == 0 ? 1 : std::sin(xx)/xx;
                data.x[i] = x;
            }
            break;

    }

    std::vector<std::complex<double>> fd(N);
    dsp::fft(data.y.begin(), data.y.end(), fd.begin());

    data.real.resize(N);
    data.img.resize(N);
    data.mag.resize(N);
    data.phase.resize(N);

    for(int i = 0; i < N; i++){
        const auto& c = fd[i];
        int index = (i + N/2)%N;
        data.real[index] = c.real();
        data.img[index] = c.imag();
        data.mag[index] = std::abs(c);
        data.phase[index] = std::arg(c);
        data.fx.push_back(static_cast<float>(i)/static_cast<float>(N) - 0.5f);
    }

}

int main(int, char**){
    std::mutex mutex;
    std::condition_variable data_ready;
    std::condition_variable ui_updated;

    Settings settings{};
    Data data;
    update(data, settings);

    vui::config config{1280, 1200, "Fourier Transform Pairs"};

    vui::show(config, [&]{

        static int shift = settings.shift;
        static int signalType = static_cast<int>(settings.signalType);
        static std::array<const char*, 3> signals{ "Impulse", "Rectangular pulse", "Sinc" };
        static bool dirty = false;
        static Data lData = data;
        static bool polar = true;
        static float f = 0;

        ImGui::Begin("FT Pairs");
        ImGui::SetWindowSize({1250, 720});

        if(ImPlot::BeginPlot("Time Domain", {400, 400})){
            ImPlot::PlotLine("Impulse", data.x.data(), data.y.data(), data.x.size());
            ImPlot::EndPlot();
        }

        if(polar) {
            ImGui::SameLine();
            if (ImPlot::BeginPlot("Fd Magnitude", {400, 400})) {
                ImPlot::PlotLine("Magnitude", data.fx.data(), data.mag.data(), data.fx.size());
                ImPlot::EndPlot();
            }
            ImGui::SameLine();
            if (ImPlot::BeginPlot("Fd Phase", {400, 400})) {
                ImPlot::PlotLine("Phase", data.fx.data(), data.phase.data(), data.fx.size());
                ImPlot::EndPlot();
            }
        }else{
            ImGui::SameLine();
            if (ImPlot::BeginPlot("Real", {400, 400})) {
                ImPlot::PlotLine("Real", data.fx.data(), data.real.data(), data.fx.size());
                ImPlot::EndPlot();
            }
            ImGui::SameLine();
            if (ImPlot::BeginPlot("Imaginary", {400, 400})) {
                ImPlot::PlotLine("Imaginary", data.fx.data(), data.img.data(), data.fx.size());
                ImPlot::EndPlot();
            }
        }
        ImGui::Checkbox("polar", &polar);

        if(settings.signalType == SignalType::RectangularPulse || settings.signalType == SignalType::Impulse) {
            dirty |= ImGui::SliderInt("shift", &shift, 0, data.x.size() - 1);
        }

        if(settings.signalType == SignalType::Sinc){
            dirty |= ImGui::SliderFloat("f", &f, 0.f, 10.f);
        }

        dirty |= ImGui::Combo("signal", &signalType, signals.data(), signals.size());

        ImGui::End();

        if(!ImGui::IsAnyItemActive() && dirty){
            std::unique_lock lock{mutex};
            dirty = false;
            settings.shift = shift;
            settings.signalType = static_cast<SignalType>(signalType);
            settings.f = f;
            lock.unlock();
            ui_updated.notify_one();
        }

        std::unique_lock lock{mutex};
        auto res = data_ready.wait_for(lock, std::chrono::seconds(0));
        if(res == std::cv_status::no_timeout){
            lData = data;
        }
        lock.unlock();

    }, [&]{
        std::cout << "UI shutting down releasing ui wait lock\n";
        std::unique_lock lk{mutex};
        lk.unlock();
        ui_updated.notify_one();
    });

    while(true){
        std::unique_lock lk{mutex};
        std::cout << "waiting on ui update\n";
        ui_updated.wait(lk);

        if(!vui::isRunning()){
            break;
        }
        std::cout << "ui updated\n";
        update(data, settings);
        data_ready.notify_one();
        lk.unlock();

    }

    vui::wait();
}