#pragma once

#include "dsp.h"
#include "sample_buffer.h"
#include <deque>
#include <tuple>
#include "chebyshev.h"

namespace dsp::recursive {

    template<typename SampleType, size_t Poles>
    SampleBuffer<SampleType> filter(SampleBuffer<SampleType>& input, Coefficients<Poles> coefficients){
        auto& A = coefficients.a;
        auto& B = coefficients.b;
        assert(B[0] == 0);
        assert(!A.empty());
        assert(!B.empty());

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
        const auto coeff = chebyshev::computeCoefficients<4>(FilterType::LowPass, 0.5, cutoffFrequency);
        return filter(input, coeff);
    }

    template<typename SampleType>
    SampleBuffer<SampleType> highPassFilter(SampleBuffer<SampleType>& input, SampleType cutoffFrequency){
        assert(cutoffFrequency >= 0 && cutoffFrequency <= 0.5);
        const auto coeff = chebyshev::computeCoefficients<4>(FilterType::HighPass, 0.5, cutoffFrequency);
        return filter(input, coeff);
    }

    template<typename SampleType>
    SampleBuffer<SampleType> bandPassFilter(SampleBuffer<SampleType>& input, SampleType centerFrequency, SampleType bandwidth){
        const auto cf = centerFrequency;
        const auto BW = bandwidth;
        const auto R = 1 - 3 * BW;
        const auto cos2pif = std::cos(2 * PI * cf);
        const auto K = (1 - 2 * R * cos2pif + R * R) / (2 - 2 * cos2pif);

        Coefficients<2> coefficients{
            {
                1 - K,
                2 * (K - R) * cos2pif,
                R * R - K
            }
            ,{
                0,
                2 * R * cos2pif,
                - (R * R)
            }
        };

        return filter(input, coefficients);

    }

    template<typename SampleType>
    SampleBuffer<SampleType> bandRejectFilter(SampleBuffer<SampleType>& input, SampleType centerFrequency, SampleType bandwidth){
        const auto cf = centerFrequency;
        const auto BW = bandwidth;
        const auto R = 1 - 3 * BW;
        const auto cos2pif = std::cos(2 * PI * cf);
        const auto K = (1 - 2 * R * cos2pif + R * R) / (2 - 2 * cos2pif);

        Coefficients<2> coefficients{
            {
                K,
                -2 * K * cos2pif,
                K
            },
            {
                0,
                2 * R * cos2pif,
               - (R * R)
            }
        };

        return filter(input, coefficients);

    }
}