#include <portmidi.h>
#include "porttime.h"
#include "synthesizer.h"

#include <array>
#include <iostream>
#include <cassert>
#include <cstdlib>
#include <cstdio>
#include "synthesizer.h"
#include <audio/engine.h>
#include <vui/vui.h>
#include <imgui.h>
#include <implot.h>

#define SAMPLE_RATE (48000)
#define T (1.0/SAMPLE_RATE)
#define H (2.0 * dsp::PI)
#define kH (1000 * H)
#define FRAME_COUNT (64)

#define INPUT_BUFFER_SIZE 100
#define TIME_PROC ((int32_t (*)(void *)) Pt_Time)
#define TIME_INFO NULL


PmStream* initMidi(){
    PmStream* midi;
    PmSysDepInfo *sysdepinfo = NULL;
    auto inputDevice = Pm_GetDefaultInputDeviceID();

    Pm_OpenInput(&midi, inputDevice, sysdepinfo, INPUT_BUFFER_SIZE, TIME_PROC, TIME_INFO);
    printf("Midi Input opened\n");
    Pm_SetFilter(midi, PM_FILT_ACTIVE | PM_FILT_CLOCK | PM_FILT_SYSEX);

    /* empty the buffer after setting filter, just in case anything
    got through */
    PmEvent event;
    while (Pm_Poll(midi)) {
        Pm_Read(midi, &event, 1);
    }

    return midi;
}

void close(PmStream* midi){
    printf("ready to close...");

    Pm_Close(midi);
    printf("done closing...");
}


int main(int, char**){
    auto midi = initMidi();

    PmEvent event;

    audio::Engine engine{{0, 1, audio::SampleType::Float32, SAMPLE_RATE, FRAME_COUNT, 4096}};
    engine.start();
    printf("audio engine online\n");

    Synthesizer synthesizer{ engine.connectNewInput(2048), SAMPLE_RATE };
    synthesizer.run();
    printf("synthesizer online\n");

    vui::config config{1024, 720, "midi"};

    auto signal = engine.outputTap(512);

    vui::show(config, [&]{
        static std::vector<float> data(512);
        auto read = std::min(data.size(), signal->getNumSamplesAvailable());

        if(read != 0) {
            signal->popAudio(data.data(), read, false);
        }

        ImGui::Begin("visualize");
        ImGui::SetWindowPos({0, 0});
        ImGui::SetWindowSize({1024, 400});

        if(ImPlot::BeginPlot("signal", {1024, 320})){
            ImPlot::PlotLine("signal", data.data(), data.size());
            ImPlot::EndPlot();
        }
        static int oscillator{0};
        ImGui::Text("Oscillator:");
        ImGui::Indent(16);

        static bool dirty = false;
        dirty |= ImGui::RadioButton("Sine Wave", &oscillator, asInt(OscillatorType::Sine)); ImGui::SameLine();
        dirty |= ImGui::RadioButton("Square Wave", &oscillator, asInt(OscillatorType::Square)); ImGui::SameLine();
        dirty |= ImGui::RadioButton("Saw Wave", &oscillator, asInt(OscillatorType::Saw)); ImGui::SameLine();
        dirty |= ImGui::RadioButton("Triangle Wave", &oscillator, asInt(OscillatorType::Triangle));

        if(dirty){
            dirty = false;
            synthesizer.set(toOsc(oscillator));
        }

        ImGui::Indent(-16);

        ImGui::End();

    }, [&]{ engine.shutdown(); });

    while (engine.isActive()) {
        if (Pm_Poll(midi) == TRUE) {
            auto length = static_cast<PmError>(Pm_Read(midi, &event, 1));
            if (length > 0) {
                synthesizer.addEvent(event);
            }
        }
    }

    vui::wait();

    close(midi);
    return 0;
}