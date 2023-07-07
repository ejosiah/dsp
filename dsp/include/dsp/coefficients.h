#pragma once

#include <array>
#include "sample_buffer.h"

namespace dsp {
    template<size_t Poles>
    struct Coefficients {
        std::array<double, Poles + 1u> a{};
        std::array<double, Poles + 1u> b{};
        std::array<double, Poles + 1u> x{};
        std::array<double, Poles + 1u> y{};

        double c0{1};
        double d0{0};

        template<typename SampleType>
        SampleBuffer<SampleType> operator()(const SampleBuffer<SampleType> &input);

        template<typename SampleType>
        SampleType operator()(SampleType iSample);

        template<typename SampleType, size_t Capacity>
        void operator()(const SampleBuffer<SampleType> &input, CircularBuffer<SampleType, Capacity> &output);

        static constexpr size_t poles = Poles;

//        template<typename SampleType, size_t Capacity>
//        auto stream();
    };

    template<>
    struct Coefficients<2> {
        union {
            std::array<double, 3u> a{};
            struct {
                double a0;
                double a1;
                double a2;
            };
        };

        union {
            std::array<double, 3u> b{};
            struct {
                double b0;
                double b1;
                double b2;
            };
        };
        
        union {
            std::array<double, 3u> x{};
            struct {
                double x0;
                double x1;
                double x2;
            };
        };
        
        union {
            std::array<double, 3u> y{};
            struct {
                double y0;
                double y1;
                double y2;
            };
        };

        double c0{1};
        double d0{0};

        template<typename SampleType>
        SampleBuffer<SampleType> operator()(const SampleBuffer<SampleType> &input);

        template<typename SampleType>
        SampleType operator()(SampleType iSample);

        template<typename SampleType, size_t Capacity>
        void operator()(const SampleBuffer<SampleType> &input, CircularBuffer<SampleType, Capacity> &output);
    };

    using BiQuad = Coefficients<2>;
    using TwoPoleCoefficients = Coefficients<2>;
    using FourPoleCoefficients = Coefficients<4>;
    using SixPoleCoefficients = Coefficients<6>;

    template<size_t Poles>
    template<typename SampleType>
    SampleBuffer<SampleType> Coefficients<Poles>::operator()(const SampleBuffer<SampleType> &input) {
        auto &A = a;
        auto &B = b;
        assert(B[0] == 0);
        assert(!A.empty());
        assert(!B.empty());

        const auto N = input.size();
        SampleBuffer<SampleType> output(N);
        for(int i = 0; i < N; i++){
            output[i] = this->operator()(input[i]);
        }

        return output;
    }

    template<size_t Poles>
    template<typename SampleType>
    SampleType Coefficients<Poles>::operator()(SampleType iSample) {
        SampleType oSample{};

        auto N = a.size();
        x[0] = iSample;
        for(int i = 0; i < N; i++){
            oSample += a[i] * x[i];
        }
        for(int i = 1; i < N; i++){
            oSample += b[i] * y[i];
        }

        for(int i = (N - 1); i > 0; --i){
            x[i] = x[i - 1];
        }
        for(int i = (N - 1); i > 0; --i){
            y[i] = y[i - 1];
        }
        x[0] = 0;
        y[1] = oSample;

        return oSample * c0 + iSample * d0;
    }

    template<size_t Poles>
    template<typename SampleType, size_t Capacity>
    void Coefficients<Poles>::operator()(const SampleBuffer<SampleType> &input,
                                         CircularBuffer<SampleType, Capacity> &output) {
        auto &A = a;
        auto &B = b;
        assert(B[0] == 0);
        assert(!A.empty());
        assert(!B.empty());

        static std::array<double, Poles> prevInput{};

        const auto N = input.size();
        const auto aN = A.size();
        const auto bN = B.size();
        SampleBuffer<SampleType> X(input.size() + Poles);
        auto dest = X.begin();
        std::copy(prevInput.begin(), prevInput.end(), dest);
        std::advance(dest, Poles);
        std::copy(input.cbegin(), input.cend(), dest);

        auto &Y = output;

        for (auto i = 0; i < N; i++) {
            Y[i] = 0;
            for (auto j = 0; j < aN; j++) {
                Y[i] += A[j] * X[i + Poles - j];
            }

            for (auto j = 1; j < bN; j++) {
                Y[i] += B[j] * Y[i - j];
            }

            Y[i] = Y[i] * c0 + X[i] * d0;
        }
        auto start = input.cend();
        std::advance(start, -Poles);
        std::copy(start, input.cend(), prevInput.begin());
    }

    template<typename SampleType>
    SampleType Coefficients<2>::operator()(SampleType sample){
        auto y = a0 * sample + a1 * x1 + a2 * x2;
        y +=                  b1 * y1 + b2 * y2;

        x2 = x1;
        x1 = sample;
        y2 = y1;
        y1 = y;
        return (y * c0) + (sample * d0);
    }

    template<typename SampleType>
    SampleBuffer<SampleType> Coefficients<2>::operator()(const SampleBuffer<SampleType> &input) {
        SampleBuffer<SampleType> output{};

        const auto N = input.size();
        for(int i = 0; i < N; i++){
            auto iSample = input[i];
            auto oSample = this->operator()(iSample);
            output.add(oSample);
        }
        return output;
    }

    template<typename SampleType, size_t Capacity>
    void Coefficients<2>::operator()(const SampleBuffer<SampleType> &input, CircularBuffer<SampleType, Capacity> &output) {

        const auto N = input.size();
        for(int i = 0; i < N; i++){
            auto iSample = input[i];
            auto oSample = this->operator()(iSample);
            output[i] = oSample;
        }
    }
}