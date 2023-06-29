#pragma once

#include <complex>
#include <cmath>
#include "constants.h"
#include <numeric>
#include <functional>
#include <deque>

namespace dsp {

    enum class Domain : int { Time = 0, Spacial, Frequency};

    using Coefficients = std::deque<double>;

    enum class FilterType{ LowPass, HighPass };

    using Window = std::function<double(size_t, size_t)>;

    enum class InversionType : int { SpectralInversion, SpectralReversal, None};

    struct Windows {

        static auto blackMan(size_t i, size_t M) {
            const auto i_over_M = static_cast<double >(i)/static_cast<double >(M);
            return 0.42 - 0.5 * std::cos(2 * PI * i_over_M) + 0.08 * std::cos(4 * PI * i_over_M);
        }

        static auto identity(size_t, size_t){
            return 1.0;
        };

        static auto hamming(size_t i, size_t M){
            const auto i_over_M = static_cast<double >(i)/static_cast<double >(M);
            return 0.54 - 0.46 * std::cos(2.0 * PI * i_over_M);
        }
    };

    void normalize(std::vector<double> &kernel) {
        const auto sum = std::accumulate(kernel.begin(), kernel.end(), 0.0);
        for(auto& y : kernel){
            y /= sum;
        }
    }

    template<InversionType inversionType = InversionType::None>
    std::vector<double> sinc(double cf, int length, const Window& window = Windows::blackMan) {
        length = (length | 1);   // length must be odd
        const auto M = length - 1;
        std::vector<double> output;
        for(auto i = 0; i <= M; i++){
            const double IM2 = double(i)  - double(M)/2;
            const auto isMidPoint = IM2 == 0;

            double y = 2 * PI * cf;
            if(!isMidPoint){
                y = std::sin(y * IM2)/IM2;
            }
            y *= window(i, M);
            output.push_back(y);
        }

        normalize(output);

        if constexpr (inversionType == InversionType::SpectralInversion){
            for(auto& y : output){
                y = -y;
            }
            output[length/2] += 1;
        }
        if constexpr (inversionType == InversionType::SpectralReversal){
            for(int i = 1; i < length; i += 2){
                output[i] = -output[i];
            }
        }

        return output;
    }
}