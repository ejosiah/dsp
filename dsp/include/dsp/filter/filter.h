#pragma once

#include "filter.h"
#include "dsp/signal_buffer.h"

namespace dsp::filter {

    enum class Type{ FIR, IIR };


    struct MovingAverageFilter {
    public:
        static constexpr Type type = Type::FIR;

        MovingAverageFilter(size_t numPoints)
        : m_numPoints(numPoints + numPoints%2)
        {}

        template<typename SampleType>
        SampleBuffer<SampleType> apply(SampleBuffer<SampleType>& sampleBuffer) {
            const auto N = sampleBuffer.size();
            const auto M = m_numPoints;
            const auto& input = sampleBuffer;
            SampleBuffer<SampleType> output(N);

            SampleType sum{};
            for(int i = 0; i < M; i++){
                sum += input[i];
            }
            auto mid = M/2;
            output[mid] = sum / static_cast<SampleType>(M);

            for(auto i = mid + 1; i < N; i++){
                sum += input[i + mid] - input[i - mid - 1];
                output[i] = sum / static_cast<SampleType>(M);
            }

            return output;
        }

        void numPoints(size_t n) noexcept {
            m_numPoints = n;
        }

    private:
        size_t m_numPoints;
    };

    struct SincFilter {
    public:
        static constexpr Type type = Type::FIR;

        SincFilter(double cf);

        template<typename SampleType>
        SampleBuffer<SampleType> apply(SampleBuffer<SampleType>& sampleBuffer);

    private:
        double m_cutoffFrequency;
    };

}

