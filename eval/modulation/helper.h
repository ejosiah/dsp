#pragma once

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
#include <functional>

enum ModulatorType {AM, FM};
enum OscillatorType { Sine = 0, Square, Triangle, Saw, NumTypes };

struct Controls{
    int sampleRate{0};
    int type{FM};
    struct {
        float frequency{10};
        int index{30};
        int waveType{Sine};
    } modulator;
    struct {
        float frequency{100};
        int waveType{Sine};
    } carrier;
};


void computeFFT(const std::vector<float>& signal, std::vector<float>& fft, int iFrequency, int oFrequency){
    int size = fft.size();
    fft.resize(size);
    std::vector<std::complex<double>> cfft(size);

    std::vector<float> oSignal(size);
//    audio::resample(signal, oSignal, iFrequency, oFrequency);
    dsp::fft(signal.begin(), signal.end(), cfft.begin(), size);

    std::transform(cfft.begin(), cfft.end(), fft.begin(), [](auto c){
        return std::abs(c);
    });
}

class SignalGenerator {
public:

    void update(Controls controls){
        const auto sampleRate = static_cast<float>(controls.sampleRate);
        auto frequency = controls.modulator.frequency;
        modulatorType = controls.type;
        index = controls.modulator.index;

        switch (controls.modulator.waveType){
            case Sine: {
                choc::oscillator::Sine<float> osc{};
                osc.setFrequency(frequency, sampleRate);
                modulator = [osc]() mutable { return osc.getSample(); };
                break;
            }
            case Square: {
                choc::oscillator::Square<float> osc{};
                osc.setFrequency(frequency, sampleRate);
                modulator = [osc]() mutable { return osc.getSample(); };
                break;
            }
            case Saw: {
                choc::oscillator::Square<float> osc{};
                osc.setFrequency(frequency, sampleRate);
                modulator = [osc]() mutable { return osc.getSample(); };
                break;
            }
            case Triangle: {
                choc::oscillator::Square<float> osc{};
                osc.setFrequency(frequency, sampleRate);
                modulator = [osc]() mutable { return osc.getSample(); };
                break;
            }
        }
        frequency = controls.carrier.frequency;
        switch (controls.carrier.waveType){
            case Sine: {
                choc::oscillator::Sine<float> osc{};
                osc.setFrequency(frequency, sampleRate);
                carrier = [osc, sampleRate, frequency](float freq = 0) mutable {
                    if(freq != 0){
                        osc.setFrequency(freq + frequency, sampleRate);
                    }
                    return osc.getSample();
                };
                break;
            }
            case Square: {
                choc::oscillator::Square<float> osc{};
                osc.setFrequency(frequency, sampleRate);
                carrier = [osc, sampleRate, frequency](float freq = 0) mutable {
                    if(freq != 0){
                        osc.setFrequency(freq + frequency, sampleRate);
                    }
                    return osc.getSample();
                };
                break;
            }
            case Saw: {
                choc::oscillator::Square<float> osc{};
                osc.setFrequency(frequency, sampleRate);
                carrier = [osc, sampleRate, frequency](float freq = 0) mutable {
                    if(freq != 0){
                        osc.setFrequency(freq + frequency, sampleRate);
                    }
                    return osc.getSample();
                };
                break;
            }
            case Triangle: {
                choc::oscillator::Square<float> osc{};
                osc.setFrequency(frequency, sampleRate);
                carrier = [osc, sampleRate, frequency](float freq = 0) mutable {
                    if(freq != 0){
                        osc.setFrequency(freq + frequency, sampleRate);
                    }
                    return osc.getSample();
                };
                break;
            }
        }
    }

    float operator()() {
        if(modulatorType == AM){
            return modulator() * carrier(0);
        }else if(modulatorType == FM){
            return carrier(modulator() * index);
        }
        return 0;
    }


private:
    std::function<float(float)> carrier;
    std::function<float()> modulator;
    int modulatorType{AM};
    int index{};

};