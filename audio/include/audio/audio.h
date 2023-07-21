#pragma once

#include "type_defs.h"
#include <tuple>
#include <cmath>
#include <vector>
#include <numeric>

namespace audio {

    using Channel2 = std::tuple<real_t, real_t>;


    template<typename Type>
    Type dbToVolume(Type dB){
        return as<Type>(std::pow(10, 0.05 * dB));
    }

    template<typename Type>
    Type volumeToDb(Type volume){
        return as<Type>(20 * std::log10(volume));
    }

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

    constexpr uint32_t alignedSize(uint32_t value, uint32_t alignment){
        return (value + alignment - 1) & ~(alignment - 1);
    }

    template<typename Type>
    void resample(std::vector<Type> vIn, std::vector<Type> vOut, int iFrequency, int oFrequency){
        const auto LCM = std::lcm(iFrequency, oFrequency);
        const auto iRatio = LCM / iFrequency;
        const auto oRatio = LCM / oFrequency;
        const auto iSize = vIn.size();
        const auto oSize = vOut.size();
        for(auto i = 0u; i < oSize; i++){
            auto from = i * oRatio / iRatio;
            auto to = from + 1;
            if(to >= iSize) break;
            auto offset = (i * oRatio) % iRatio;
            auto t = float(offset)/float(iRatio);
            vOut[i] = std::lerp(vIn[from], vIn[to], t);
        }


    }
}