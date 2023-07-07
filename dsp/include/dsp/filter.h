#pragma once

#include "filter.h"
#include "dsp.h"
#include "dsp/sample_buffer.h"
#include "constants.h"
#include <cmath>
#include <numeric>

namespace dsp::filter {

    enum class Type{ FIR, IIR };


    struct MovingAverageFilter {
    public:
        static constexpr Type type = Type::FIR;

        MovingAverageFilter(size_t numPoints = 1);

        template<typename SampleType>
        SampleBuffer<SampleType> operator()(SampleBuffer<SampleType>& sampleBuffer);

        template<typename SampleType>
        SampleBuffer<SampleType> apply(SampleBuffer<SampleType>& sampleBuffer);

        void numPoints(size_t n);

    private:
        size_t m_numPoints;
    };

    template<InversionType InversionType = InversionType::None>
    struct SincFilter {
    public:
        static constexpr Type type = Type::FIR;

        SincFilter() = default;

        SincFilter(double cf, size_t length);

        void cutoffFrequency(double cf);

        void length(size_t length);

        template<typename SampleType>
        SampleBuffer<SampleType> apply(SampleBuffer<SampleType>& sampleBuffer);


        [[nodiscard]]
        std::vector<double> kernel() const;

    private:
        double m_cutoffFrequency{0};
        std::vector<double> m_kernel{};
        size_t m_length{0};
    };

//==============================================================================
//        _        _           _  _
//     __| |  ___ | |_   __ _ (_)| | ___
//    / _` | / _ \| __| / _` || || |/ __|
//   | (_| ||  __/| |_ | (_| || || |\__ \ _  _  _
//    \__,_| \___| \__| \__,_||_||_||___/(_)(_)(_)
//
//   Code beyond this point is implementation detail...
//
//==============================================================================
    MovingAverageFilter::MovingAverageFilter(size_t numPoints)
            : m_numPoints(numPoints | 1) {}

    void MovingAverageFilter::numPoints(size_t n) {
        m_numPoints = n;
    }

    template<typename SampleType>
    SampleBuffer <SampleType> MovingAverageFilter::operator()(SampleBuffer <SampleType> &sampleBuffer) {
        return apply(sampleBuffer);
    }

    template<typename SampleType>
    SampleBuffer <SampleType> MovingAverageFilter::apply(SampleBuffer <SampleType> &sampleBuffer) {
        const auto N = sampleBuffer.size();
        const auto M = m_numPoints;
        const auto &input = sampleBuffer;
        SampleBuffer <SampleType> output(N);

        SampleType sum{};
        for (int i = 0; i < M; i++) {
            sum += input[i];
        }
        auto mid = M / 2;
        output[mid] = sum / static_cast<SampleType>(M);

        for (auto i = mid + 1; i < (N - mid); i++) {
            sum += input[i + mid] - input[i - mid - 1];
            output[i] = sum / static_cast<SampleType>(M);
        }

        return output;
    }

    template<InversionType InversionType>
     SincFilter<InversionType>::SincFilter(double cf, size_t length)
            : m_cutoffFrequency(cf)
            , m_length(length)
    {
        m_kernel = sinc<InversionType>(cf, int(length));
    }

    template<InversionType InversionType>
    void SincFilter<InversionType>::cutoffFrequency(double cf) {
        m_cutoffFrequency = cf;
        sinc(m_cutoffFrequency, m_length);
    }

    template<InversionType InversionType>
    void SincFilter<InversionType>::length(size_t length) {
        m_length = length;
        sinc(m_cutoffFrequency, m_length);
    }

    template<InversionType InversionType>
    template<typename SampleType>
    SampleBuffer<SampleType> SincFilter<InversionType>::apply(SampleBuffer<SampleType> &sampleBuffer) {
        SampleBuffer<SampleType> output(sampleBuffer.size());
        const auto& X = sampleBuffer;
        const auto& H = m_kernel;
        auto& Y = output;
        const auto N = sampleBuffer.size();
        const auto M = m_kernel.size() - 1;

        for(auto j = M; j < N; j++){
            for(auto i = 0; i <= M; i++){
                Y[j] = Y[j] + X[j - i] * H[i];
            }
        }

        return output;
    }

    template<InversionType InversionType>
    std::vector<double> SincFilter<InversionType>::kernel() const {
        return m_kernel;
    }
}

