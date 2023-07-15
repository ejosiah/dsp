#pragma once

#include "type_defs.h"


namespace audio {

    inline MonoView wrapMono(real_t* samples, uint32_t numSamples){
        return choc::buffer::createMonoView(samples, numSamples);
    }

    inline InterleavedView wrapInterLeaved(real_t* samples, uint32_t numChannels, uint32_t numFrames){
        return choc::buffer::createInterleavedView(samples, numChannels, numFrames);
    }
}