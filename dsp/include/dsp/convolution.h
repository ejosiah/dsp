#pragma once

#include "dsp.h"
#include "sample_buffer.h"

namespace dsp {

    template<typename SampleType, Domain domain>
    SampleBuffer<SampleType> convolve(const Signal<SampleType>& signal, const Kernel<SampleType>& kernel){
        if constexpr (domain == Domain::Frequency){
            throw std::runtime_error("Not yet implemented!");
        }else{
            SampleBuffer<SampleType> output(signal.size());
            const auto N = signal.size();
            const auto M = kernel.size();
            const auto& X = signal;
            const auto& H = kernel;
            auto& Y = output;


            for(auto j = M; j < N; j++){
                for(auto i = 0; i <= M; i++){
                    Y[j] = Y[j] + X[j - i] * H[i];
                }
            }

            return output;
        }
    }
}
