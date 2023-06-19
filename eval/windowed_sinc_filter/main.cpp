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
#include <memory>
#include <iostream>
#include <dsp/dsp.h>
#include <complex>
#include <condition_variable>

static constexpr float PI = 3.1415926535897932384626433832795;
static constexpr float TWO_PI = 6.283185307179586476925286766559;

enum class WindowType : int { Hamming = 0, Blackman, None };

struct Settings {
    float cutoffFrequency{0.5};
    int length{128};
    WindowType windowType{WindowType::None};
};

struct Data {
    std::vector<float> x;
    std::vector<float> y;
    std::vector<float> mag;
    std::vector<float> phase;
};

std::shared_ptr<Data> create(Settings settings){
    int M = settings.length;
    float fc = settings.cutoffFrequency;

    std::cout << "creating data points for fc: " << fc << " and M: " << M << "\n";

    auto data = std::make_shared<Data>();
    for(int i = 0; i < M; i++){
        auto x = (i - M/2);
        auto value = x == 0 ? TWO_PI * fc :  std::sin(TWO_PI * fc * x)/x;
        if(settings.windowType == WindowType::Hamming){
            value *= 0.54 - 0.46 * std::cos(TWO_PI * i / M);
        }else if(settings.windowType == WindowType::Blackman){
            value *= 0.24 - 0.5 * std::cos(TWO_PI * i / M) + 0.08 * cos(4 * PI * i / M);
        }

        data->x.push_back(i);
        data->y.push_back(value);
    }

    auto n = 1 << static_cast<int>(std::ceil(std::log2(M)));
    std::vector<std::complex<double>> fd(n);

    dsp::fft(data->y.begin(), data->y.end(), fd.begin() );

    for(const auto c : fd){
        data->mag.push_back(std::abs(c));
    }

    return data;
}

std::shared_ptr<Data> create2(Settings settings){
    int M = settings.length;
    float fc = settings.cutoffFrequency;

    std::cout << "creating data points for fc: " << fc << " and M: " << M << "\n";
    auto data = std::make_shared<Data>();

    float dt = 1.0f/static_cast<float>(M);
    float t = 0;
    for(int i = 0; i < M; i++){
        float x = 2 * t - 1;
        float iv = PI * x * fc;
        float y = iv == 0 ? 1 : std::sin(iv)/iv;
        data->x.push_back(x);
        data->y.push_back(y);
        t += dt;
    }

    auto N = 1 << static_cast<int>(std::ceil(std::log2(M)));
//    std::vector<std::complex<double>> fd(N);
//    dsp::fft(data->x.begin(), data->x.end(), fd.begin() );

    std::vector<std::complex<double>> time_domain_input;
    for(const auto& real : data->y){
        time_domain_input.emplace_back(real, 0);
    }
    auto padding = N - M;
    for(int i = 0; i < padding; i++){
        time_domain_input.emplace_back(0, 0);
    }

    auto fd = dsp::fft2(time_domain_input);

    for(const auto& c : fd){
        data->mag.push_back(std::abs(c));
    }

    return data;
}

std::mutex mutex;
std::condition_variable data_ready;
std::condition_variable ui_updated;

int main(int, char**){
    vui::config config{1280, 1200, "Windowed Sinc filter"};

    float fc = 0.01;

    Settings settings{0.25, 200};
    std::shared_ptr<Data> data = create(settings);

    vui::show(config, [&]{

        static float fc = settings.cutoffFrequency;
        static int M = settings.length;
        static int window = static_cast<int>(settings.windowType);
        static bool dirty = false;
        static std::array<const char*, 3> windows{"Hamming", "Blackman", "None"};


        std::unique_lock lock{mutex};
        auto res = data_ready.wait_for(lock, std::chrono::seconds(0));
        if(res == std::cv_status::no_timeout){
            M = data->x.size();
            std::cout << "M :" << M << "\n";
        }
        lock.unlock();


        ImGui::Begin("Graph");
        ImGui::SetWindowSize({600, 720});
        if(ImPlot::BeginPlot("Sinc filter")){
            ImPlot::PlotLine("Sinc", data->x.data(), data->y.data(), data->x.size());
            ImPlot::EndPlot();
        }

        if(ImPlot::BeginPlot("frequency")){
            ImPlot::PlotLine("Frequency", data->mag.data(), M/2);
            ImPlot::EndPlot();
        }

        dirty |= ImGui::SliderFloat("cutoff frequency", &fc, 0, 0.5);
        dirty |= ImGui::SliderInt("length", &M, 0, 1000);
        dirty |= ImGui::Combo("window", &window, windows.data(), 3);

        if(!ImGui::IsAnyItemActive() && dirty){
            lock = std::unique_lock{mutex};
            dirty = false;
            settings.cutoffFrequency = fc;
            settings.length = M;
            settings.windowType = static_cast<WindowType>(window);
            lock.unlock();
            ui_updated.notify_one();
        }

        ImGui::End();
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
        data = create(settings);
        data_ready.notify_one();
        lk.unlock();

    }

    vui::wait();

}