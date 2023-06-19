#include <iostream>

#define PINK_MEASURE

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <ctime>
#include <istream>
#include <audio/choc_AudioFileFormat.h>
#include <audio/choc_Oscillators.h>
#include <audio/choc_AudioFileFormat_WAV.h>
#include <portaudio.h>
#include <pa_ringbuffer.h>
#include <pa_util.h>
#include "pink_noise.h"
#include <vui/vui.h>

#define ERR_GUARD_PA(err) \
if(err != paNoError) {    \
    std::cerr <<  "PortAudio error: " <<   Pa_GetErrorText(err) << "\n"; \
    exit(err);               \
}

#define SAMPLE_RATE (44100)
#define NUM_SECONDS (10)

using Oscillator = choc::oscillator::Triangle<float>;

struct Channels{
    float left;
    float right;
};

static int paStreamCallback( const void *inputBuffer,
                             void *outputBuffer,
                             unsigned long frameCount,
                             const PaStreamCallbackTimeInfo* timeInfo,
                             PaStreamCallbackFlags statusFlags,
                             void *userData ){

    auto oscillator = reinterpret_cast<Oscillator*>(userData);
    auto channel = reinterpret_cast<Channels*>(outputBuffer);

    for(int i = 0; i < frameCount; i++){
        auto amp = oscillator->getSample();
        channel->left = amp;
        channel->right = amp;
        channel++;
    }

    return 0;

}

struct AudioInfo{
    choc::buffer::FrameCount currentFrame;
    choc::buffer::ChannelArrayBuffer<float> buffer;
};

static int paPlayAudioCallback( const void *inputBuffer,
                                void *outputBuffer,
                                unsigned long frameCount,
                                const PaStreamCallbackTimeInfo* timeInfo,
                                PaStreamCallbackFlags statusFlags,
                                void *userData ){

    auto info = reinterpret_cast<AudioInfo*>(userData);
    auto out = reinterpret_cast<Channels*>(outputBuffer);

    for(int i = 0; i < frameCount; i++){
        if(info->currentFrame >= info->buffer.getNumFrames()){
            return paComplete;
        }
        auto sample = info->buffer.getSample(0, info->currentFrame);
        out->left  = sample;
        out->right = sample;
        out++;
        info->currentFrame++;
    }

    return paContinue;
}

struct PinkNoiseChannels {
    PinkNoise left;
    PinkNoise right;
};

static int paPinkNoiseCallback(const void *inputBuffer,
                               void *outputBuffer,
                               unsigned long frameCount,
                               const PaStreamCallbackTimeInfo* timeInfo,
                               PaStreamCallbackFlags statusFlags,
                               void *userData ){

    auto noise = reinterpret_cast<PinkNoiseChannels*>(userData);
    auto out = reinterpret_cast<Channels*>(outputBuffer);

    for(int i = 0; i < frameCount; i++){
        out->left = noise->left.next();
        out->right = noise->right.next();
        out++;
    }

    return paContinue;

}


static int paNoiseCallback(const void *inputBuffer,
                           void *outputBuffer,
                           unsigned long frameCount,
                           const PaStreamCallbackTimeInfo* timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void *userData ){

    static std::default_random_engine engine{std::random_device{}()};
//    static std::uniform_int_distribution<int> dist{ -1, 1 };
    static std::normal_distribution<float> dist{0.f, 1.f};
    static auto sample = std::bind(dist, engine);

    auto out = reinterpret_cast<Channels*>(outputBuffer);

    for(int i = 0; i < frameCount; i++){
        auto v = sample() * 0.01;
        out->left = v;
        out->right = v;
        out++;
    }

    return paContinue;
}

int audio_main() {

    std::string audioPath = "../../resources/voice.wav";
    std::shared_ptr<std::istream> audioStream = std::make_shared<std::ifstream>(audioPath, std::ios::binary);

    choc::audio::WAVAudioFileFormat<false> audioFileFormat{};
    auto reader = audioFileFormat.createReader(audioStream);
    auto props = reader->getProperties();


    AudioInfo info{0};
    info.buffer = reader->readEntireStream<float>();

    std::cout << "numFrames: " << info.buffer.getNumFrames() << "\n";
    std::cout << "numChannels: " << info.buffer.getNumChannels() << "\n";


    ERR_GUARD_PA(Pa_Initialize())

    PaStream* stream;
//    Oscillator osc;
//    osc.setFrequency(440, SAMPLE_RATE);
//    ERR_GUARD_PA(Pa_OpenDefaultStream(&stream, 0, 2, paFloat32, SAMPLE_RATE, 256, paStreamCallback, &osc))

    //ERR_GUARD_PA(Pa_OpenDefaultStream(&stream, 0, 2, paFloat32, props.sampleRate, 256, paPlayAudioCallback, &info))

    PinkNoiseChannels  noise{ PinkNoise{12}, PinkNoise{16}};
    ERR_GUARD_PA(Pa_OpenDefaultStream(&stream, 0, 2, paFloat32, SAMPLE_RATE, 256, paNoiseCallback, &noise))
    ERR_GUARD_PA(Pa_StartStream(stream))

    Pa_Sleep(NUM_SECONDS * 1000);

//    while(Pa_IsStreamActive(stream)){
//        Pa_Sleep(1000);
//    }

    ERR_GUARD_PA(Pa_StopStream(stream))
    ERR_GUARD_PA(Pa_CloseStream(stream))
    ERR_GUARD_PA(Pa_Terminate())


    return 0;
}
