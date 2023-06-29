#pragma once

#include "dsp.h"
#include "sample_buffer.h"
#include <deque>
#include <tuple>

namespace dsp::recursive {

    template<typename SampleType>
    SampleBuffer<SampleType> filter(SampleBuffer<SampleType>& input, Coefficients A, Coefficients B){
        B.push_front(0);
        const auto N = input.size();
        const auto aN = A.size();
        const auto bN = B.size();
        SampleBuffer<SampleType> output(N);
        const auto& X = input;
        auto& Y = output;

        for(auto i = bN; i < N; i++){
            for(auto j = 0; j < aN; j++){
                Y[i] += A[j] * X[i - j];
            }

            for(auto j = 1; j < bN; j++){
                Y[i] += B[j] * Y[i - j];
            }
        }

        return output;
    }

    template<typename SampleType>
    SampleBuffer<SampleType> lowPassFilter(SampleBuffer<SampleType>& input, SampleType cutoffFrequency){
        assert(cutoffFrequency >= 0 && cutoffFrequency <= 0.5);
        auto x = std::exp(-2 * PI * cutoffFrequency);
        Coefficients A{1.0 - x};
        Coefficients B{x};
        return filter(input, A, B);
    }

    template<typename SampleType>
    SampleBuffer<SampleType> highPassFilter(SampleBuffer<SampleType>& input, SampleType cutoffFrequency){
        assert(cutoffFrequency >= 0 && cutoffFrequency <= 0.5);
        auto x = std::exp(-2 * PI * cutoffFrequency);
        Coefficients A{(1.0 + x) * 0.5, -(1.0 + x) * 0.5};
        Coefficients B{x};
        return filter(input, A, B);
    }

    template<typename SampleType>
    SampleBuffer<SampleType> bandPassFilter(SampleBuffer<SampleType>& input, SampleType centerFrequency, SampleType bandwidth){
        const auto cf = centerFrequency;
        const auto BW = bandwidth;
        const auto R = 1 - 3 * BW;
        const auto cos2pif = std::cos(2 * PI * cf);
        const auto K = (1 - 2 * R * cos2pif + R * R) / (2 - 2 * cos2pif);

        Coefficients  A{
            1 - K,
            2 * (K - R) * cos2pif,
            R * R - K
        };

        Coefficients  B{
            2 * R * cos2pif,
            - (R * R)
        };

        return filter(input, A, B);

    }

    template<typename SampleType>
    SampleBuffer<SampleType> bandRejectFilter(SampleBuffer<SampleType>& input, SampleType centerFrequency, SampleType bandwidth){
        const auto cf = centerFrequency;
        const auto BW = bandwidth;
        const auto R = 1 - 3 * BW;
        const auto cos2pif = std::cos(2 * PI * cf);
        const auto K = (1 - 2 * R * cos2pif + R * R) / (2 - 2 * cos2pif);

        Coefficients  A{
            K,
            -2 * K * cos2pif,
            K
        };

        Coefficients  B{
            2 * R * cos2pif,
           - (R * R)
        };

        return filter(input, A, B);

    }
}