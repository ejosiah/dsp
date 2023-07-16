#pragma once

#include "type_defs.h"
#include <tuple>
#include <cmath>
#include <vector>

namespace audio {

    using Channel2 = std::tuple<real_t, real_t>;

    inline MonoView wrapMono(real_t* samples, uint32_t numSamples){
        return choc::buffer::createMonoView(samples, numSamples);
    }

    inline InterleavedView wrapInterLeaved(real_t* samples, uint32_t numChannels, uint32_t numFrames){
        return choc::buffer::createInterleavedView(samples, numChannels, numFrames);
    }

    inline Channel2 fcpan(real_t input, real_t t){
        real_t a = t * real_t{0.25} - real_t{0.25};
        real_t b = a - real_t{0.25};

        auto left = std::cos(TWO_PI * b) * input;
        auto right = std::cos(TWO_PI * a) * input;

        return std::make_tuple(left, right);
    }

    inline std::vector<real_t> fcpan(const std::vector<real_t>& data, real_t t){
        std::vector<real_t> output;
        for(auto& sample : data){
            auto [left, right] = fcpan(sample, t);
            output.push_back(left);
            output.push_back(right);
        }
        return output;
    }
}