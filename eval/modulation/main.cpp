#include <audio/engine.h>
#include <vui/vui.h>
#include <audio/choc_Oscillators.h>
#include <chrono>
#include <thread>
#include <iostream>
#include <audio/source.h>
#include <audio/choc_Oscillators.h>
#include <audio/choc_AudioFileFormat_WAV.h>
#include <imgui.h>
#include <implot.h>
#include <numeric>
#include <dsp/fft.h>
#include "helper.h"

int main(int, char**){

    using namespace std::chrono_literals;

    audio::Engine engine{{0, 1, audio::SampleType::Float32, 65536, 512, 4096}};
    engine.start();

    const auto info = engine.info();
    const uint32_t maxLatency = 2048;
    auto output = engine.connectNewInput(maxLatency);
    choc::oscillator::Sine<float> osc{};
    osc.setFrequency(400, static_cast<float>(info.format().sampleRate));

    SignalGenerator signalGenerator;
    Controls defaultControls{ static_cast<int>(info.format().sampleRate)};
    signalGenerator.update(defaultControls);
    auto source = audio::continuous( output, [&]() mutable { return signalGenerator() * 0.01; });



    vui::config config{1920, 1080, "modulation"};
    auto tap = engine.outputTap(engine.sampleRate());

//    vui::frameRate(24);
    vui::show(config, [&]{
        static int frame = 0;
        static int frameSize = 1024;
        static int sampleRate = engine.sampleRate();
        static int fftSampleRate = 1 << static_cast<int>(std::ceilf(std::log2f(sampleRate)));
        static float period = static_cast<float>(frameSize)/static_cast<float>(sampleRate);
        static int framePeriod = 20;
        static float frameTime = 0;
        static int totalTime = 0;
        static float speed = 20;
        static int sid = 0;

        static std::vector<float> buffer(frameSize);
        static std::vector<float> signal(sampleRate/2);
        static std::vector<float> fft(fftSampleRate);
        static std::vector<float> x(frameSize);


        std::iota(x.begin(), x.end(), 1.f);
        std::transform(x.begin(), x.end(), x.begin(), [](auto v){ return v * period; });

        static int32_t read = 0;
        if(frameTime == 0) {
            read = tap->popAudio(buffer.data(), buffer.size(), false);
            auto sItr = signal.begin();
            std::advance(sItr, sid);
            std::copy(buffer.begin(), buffer.end(), sItr);
            sid += read;
            if(sid >= signal.size()){
                computeFFT(signal, fft, sampleRate, fftSampleRate);
            }
            sid %= signal.capacity();
        }

        // graphs

        ImGui::Begin("modulation");
        ImGui::SetWindowSize({1400, 1080});
        ImGui::SetWindowPos({0, 0});

        if(ImPlot::BeginPlot("wave", {1400, 500})){
            ImPlot::PlotLine("sinewave", x.data(), buffer.data(), read);
            ImPlot::EndPlot();
        }
        if(ImPlot::BeginPlot("fft", {1400, 500})){
            ImPlot::PlotLine("magnitude", fft.data(), 1000);
            ImPlot::EndPlot();
        }

        ImGui::End();

        // controls
        static Controls controls = defaultControls;
        static Controls prevCtrl = defaultControls;

        ImGui::Begin("controls");
        ImGui::SetWindowSize({480, 1080});

        ImGui::RadioButton("AM", &controls.type, AM);
        ImGui::SameLine();
        ImGui::RadioButton("FM", &controls.type, FM);

        // modulator
        ImGui::Text("modulator:");
        ImGui::Indent(16);
        ImGui::RadioButton("Sine", &controls.modulator.waveType, Sine); ImGui::SameLine();
        ImGui::RadioButton("Square", &controls.modulator.waveType, Square);  ImGui::SameLine();
        ImGui::RadioButton("Triangle", &controls.modulator.waveType, Triangle);  ImGui::SameLine();
        ImGui::RadioButton("Saw", &controls.modulator.waveType, Saw);

        ImGui::SliderFloat("frequency", &controls.modulator.frequency, 1, 1000);
        if(controls.type == FM){
            ImGui::SliderInt("index", &controls.modulator.index, 1, 1000);
        }

        ImGui::Indent(-16);

        ImGui::SetWindowPos({1410, 0});

        ImGui::Text("carrier:");
        ImGui::Indent(16);
        ImGui::PushID("carrier");
        ImGui::RadioButton("Sine", &controls.carrier.waveType, Sine); ImGui::SameLine();
        ImGui::RadioButton("Square", &controls.carrier.waveType, Square);  ImGui::SameLine();
        ImGui::RadioButton("Triangle", &controls.carrier.waveType, Triangle);  ImGui::SameLine();
        ImGui::RadioButton("Saw", &controls.carrier.waveType, Saw);

        ImGui::SliderFloat("frequency", &controls.carrier.frequency, 20, 1000);
        ImGui::PopID();
        ImGui::Indent(-16);

        ImGui::SetWindowPos({1410, 0});
        ImGui::End();

        frameTime += 0.2f * speed;
        if(frameTime >= framePeriod){
            frameTime = 0;
        }

        if(!ImGui::IsAnyItemActive() && std::memcmp(&prevCtrl, &controls, sizeof(Controls)) != 0){
            prevCtrl = controls;
            signalGenerator.update(controls);
        }

    });
    source.play();
    vui::wait();
    source.stop();
    engine.shutdown();

    return 0;
}