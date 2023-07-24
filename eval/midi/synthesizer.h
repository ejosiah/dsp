#pragma once

#include <thread>
#include <dsp/ring_buffer.h>
#include <cmath>
#include <audio/patch_input.h>
#include <portmidi.h>
#include <array>
#include <audio/choc_Oscillators.h>
#include <algorithm>
#include <dsp/dsp.h>
#include <chrono>
#include <cstdlib>
#include <dsp/recursive_filters.h>
#include <dsp/fourier/series.h>
#include <random>

enum class OscillatorType : int { Sine = 0, Square, Saw, Triangle };

int asInt(OscillatorType type){ return static_cast<int>(type); }
OscillatorType toOsc(int i){ return static_cast<OscillatorType>(i); }

union OscillatorParams{
    choc::oscillator::Sine<float> sine;
    choc::oscillator::Square<float> square;
    choc::oscillator::Saw<float> saw;
};

struct Oscillator {
    OscillatorType type{OscillatorType::Sine};
    OscillatorParams params{};
    choc::oscillator::Triangle<float> triangle;

    float getSample() {
        switch(type){
            case OscillatorType::Sine: return params.sine.getSample();
            case OscillatorType::Square: return params.square.getSample();
            case OscillatorType::Saw: return params.saw.getSample();
            case OscillatorType::Triangle: return triangle.getSample();
        }
    }

    void setFrequency (float frequency, float sampleRate) {
         params.sine.setFrequency(frequency, sampleRate);
        triangle.setFrequency(frequency, sampleRate);
    }
};


struct Note{
    enum class State : int { OFF = 0, ON};

    State state{State::OFF};
    int pitch{0};
    float loudness{0};
    Oscillator osc{};
    float t{0};

    [[nodiscard]]
    bool isOn() const {
        return state == State::ON;
    }
};

class Synthesizer{
public:
    explicit Synthesizer(const audio::PatchInput& patch, float sampleRate);

    ~Synthesizer();

    void addEvent(PmEvent event);

    void set(OscillatorType oscillator);

    void run();

private:
    int processEvents();

    float nextSample();

    bool notePressed() const;

private:
    std::array<Note, 88> m_notes;
    audio::PatchInput m_patch;
    std::thread m_thread;
    dsp::RingBuffer<PmEvent, 512> m_events;
    dsp::BiQuad m_lop;
    float m_period{0};
    static constexpr int mOffset = 21;

};




Synthesizer::Synthesizer(const audio::PatchInput& patch, float sampleRate)
: m_patch{ patch }
, m_period{ 1/ sampleRate }
, m_lop{ dsp::recursive::lowPassFilter<2>(1 / sampleRate ) }
{
    auto mtof = [](auto m){ return std::powf(2.f, (m - 69.f)/12.f) * 440.f; };

    for(auto i = 0; i < 88; i++){
        auto pitch = mtof(i + mOffset);
        m_notes[i].pitch = static_cast<int>(pitch);
        m_notes[i].osc.setFrequency(pitch, sampleRate);
    }

}

Synthesizer::~Synthesizer() {
    if(m_thread.joinable()){
        m_thread.join();
    }
}

void Synthesizer::addEvent(PmEvent event) {
    m_events.push(event);
}


void Synthesizer::run() {
    std::thread thread{[this]{

        audio::CircularAudioBuffer<float> buffer(64);
        std::vector<float> transferBuf;

        while(m_patch.isOpen()){
            processEvents();

            if(!notePressed()) continue;

            transferBuf.resize(buffer.remainder());
            std::generate(transferBuf.begin(), transferBuf.end(), [this]{ return nextSample(); });
            buffer.push(transferBuf.data(), transferBuf.size());

            transferBuf.resize(buffer.num());
            buffer.pop(transferBuf.data(), transferBuf.size());

            auto pushed = m_patch.pushAudio(audio::wrapMono(transferBuf.data(), transferBuf.size()));
            auto remainder = transferBuf.size() - pushed;
            buffer.setNum(remainder);
        }
        printf("Synthesizer shutting down...\n");
    }};

    m_thread = std::move(thread);
}

int Synthesizer::processEvents() {
    int numEvents = m_events.size();
    while(auto oEvent = m_events.poll()){
        auto msg = oEvent->message;
        int status = Pm_MessageStatus(msg);

        if(status != 0x80 && status != 0x90){
            // we only care about channel one for now
            continue;
        }

        int mPitch = Pm_MessageData1(msg);
        int velocity = Pm_MessageData2(msg);

        int noteId = mPitch - mOffset;
        m_notes[noteId].state = static_cast<Note::State>((status >> 4) & 1);

        if(m_notes[noteId].state == Note::State::ON){
            m_notes[noteId].loudness = static_cast<float>(velocity)/127.f;    // FIXME use decibels
        }

        if(m_notes[noteId].state == Note::State::OFF){
            m_notes[noteId].t = 0;
        }
        printf("note p: %d, v: %d, s:%d\n", mPitch, velocity, ((status >> 4) & 1));
    }
    return numEvents;
}

float Synthesizer::nextSample() {

    static auto dist = std::uniform_real_distribution<float>(-1, 1);
    static auto eng = std::default_random_engine{ std::random_device{}() };

    auto sample = 0.f;
    for(auto& note : m_notes){
        if(note.isOn()) {
            note.t += m_period;
            auto t = note.t;
            auto envelope = std::expf(-note.t * 3.0f);
//            auto envelope = 2.f * (1.f - std::expf(-10.f * t)) * std::exp(-3.f * t);
            sample += envelope * note.loudness * note.osc.getSample();
        }

    }
    return sample;
}

bool Synthesizer::notePressed() const {
    return std::any_of(m_notes.begin(), m_notes.end(), [](auto& n){ return n.isOn(); });
}

void Synthesizer::set(OscillatorType oscillator) {
    for(auto& note : m_notes){
        note.osc.type = oscillator;
    }
}
