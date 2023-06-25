#pragma once

#include <cmath>
#include <dsp/signal_buffer.h>

namespace dsp::fourier::series {

    constexpr double PI = 3.1415926535897932384626433832795;
    constexpr double _2_PI = 2.0 * PI;


    template<typename SampleType, typename An, typename Bn>
    SampleBuffer<SampleType> generate(size_t sampleRate, size_t resolution, size_t size, SampleType a0, An&& a, Bn&& b, SampleType frequency = 1, SampleType phaseShift = 0){
        SampleBuffer<SampleType> buffer{};

        auto anCosx = [&, N=resolution, f=frequency](SampleType x){
            const auto w = _2_PI * f;
            SampleType sum{};
            for(int n = 0; n < N; n++){
                sum += a(n) * std::cos(w * x * n);
            }
            return sum;
        };

        auto bnSinx = [&, N=resolution, f=frequency](SampleType x){
            const auto w = 2.0 * PI * f;
            SampleType sum{};
            for(int n = 0; n < N; n++){
                sum += b(n) * std::sin(w * x * n);
            }
            return sum;
        };

        float dx = 1.0/static_cast<SampleType>(sampleRate);
        for(int i = 0; i < size; i++){
            auto x = static_cast<SampleType>(i) * dx - phaseShift;
            float sample = a0 + anCosx(x) - bnSinx(x);
            buffer.add(sample);
        }
    }

    template<typename SampleType>
    SampleBuffer<SampleType> pulse(size_t sampleRate, size_t resolution, size_t size, SampleType pulseWidth, SampleType period,  SampleType Amplitude = 1, SampleType frequency = 1, SampleType phaseShift = 0) {
        auto d = pulseWidth/period;
        auto a0 = Amplitude * d;
        auto An = [d, A=Amplitude](SampleType n){ return ((2.0 * A)/(n * PI)) * std::sin(n * PI * d); };
        auto Bn = [](SampleType n){ return SampleType{}; };
        return generate(sampleRate, resolution, size, a0, An, Bn, frequency, phaseShift);
    }


}