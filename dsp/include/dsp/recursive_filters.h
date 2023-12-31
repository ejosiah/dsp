#pragma once

#include "dsp.h"
#include "sample_buffer.h"
#include <deque>
#include <tuple>
#include "coefficients.h"

namespace dsp::recursive {

    using Poles = std::tuple<double, double, double, double, double>;

    struct chebyshev {

        template<size_t Poles>
        static Coefficients<Poles> computeCoefficients(FilterType filter, double passBandRipple, double cutoffFrequency);

    private:
        static Poles computePoles(FilterType filter, double passBandRipple, int numPoles, int pole, double cutoffFrequency);

    };


    template<size_t Poles>
    Coefficients<Poles>
    chebyshev::computeCoefficients(FilterType filter, double passBandRipple, double cutoffFrequency) {
        assert(cutoffFrequency >= 0.0 && cutoffFrequency <= 0.5);
        assert(passBandRipple >= 0.0 && passBandRipple <= 29.0);
        assert(filter == FilterType::LowPass || filter == FilterType::HighPass);
        static_assert(Poles % 2 == 0);
        static_assert(Poles >= 2 && Poles <= 20);

        const auto LH = filter;
        const auto FC = cutoffFrequency;
        const auto PR = passBandRipple;
        constexpr auto NP = Poles;


        std::vector<double> A(22);
        std::vector<double> B(22);
        std::vector<double> TA(22);
        std::vector<double> TB(22);

        A[2] = 1;
        B[2] = 1;

        for(auto P = 1; P <= NP/2; P++){
            auto [A0, A1, A2, B1, B2] = computePoles(LH, PR, NP, P, FC);

            for(int i = 0; i < 22; i++){
                TA[i] = A[i];
                TB[i] = B[i];
            }

            for(int i = 2; i < 22; i++){
                A[i] = A0 * TA[i] + A1 * TA[i - 1] + A2 * TA[i - 2];
                B[i] = TB[i] - B1 * TB[i - 1] - B2 * TB[i - 2];
            }
        }

        B[2] = 0;
        for(int i = 0; i < 20; i++){
            A[i] = A[i+2];
            B[i] = -B[i+2];
        }

        double sumA = 0;
        double sumB = 0;

        if(LH == FilterType::LowPass){
            for(int i = 0; i < 20; i++){
                sumA += A[i];
                sumB += B[i];
            }
        }else {
            for(int i = 0; i < 20; i++){
                sumA += A[i] * std::pow(-1, i);
                sumB += B[i] * std::pow(-1, i);
            }
        }

        auto gain = sumA / (1 - sumB);

        for(int i = 0; i < 20; i++){
            A[i] /= gain;
        }

        std::vector<double> outA(NP + 1);
        std::vector<double> outB(NP + 1);

        Coefficients<NP> coefficients{};

        auto first = A.begin();
        auto last = first;
        std::advance(last, NP + 1);
        std::copy(first, last, coefficients.a.begin());

        first = B.begin();
        last = first;
        std::advance(last, NP + 1);
        std::copy(first, last, coefficients.b.begin());

        return coefficients;

    }

    Poles chebyshev::computePoles(FilterType filter, double passBandRipple, int numPoles, int pole, double cutoffFrequency){
        const double x = PI/(numPoles * 2) + (pole - 1) * PI/numPoles;
        double rPole = -std::cos(x);
        double iPole = std::sin(x);

        // Warp from a circle to an ellipse
        if(passBandRipple != 0){
            auto ES = std::sqrt(std::pow((100.0 / (100.0 - passBandRipple)), 2) - 1);
            auto VX = (1.0 / numPoles) * std::log((1.0 / ES) + sqrt(1.0 / (ES * ES) + 1));
            auto KX = (1.0 / numPoles) * std::log((1.0 / ES) + sqrt(1.0 / (ES * ES) - 1));
            KX = (std::exp(KX) + std::exp(-KX)) * 0.5;
            rPole *= ((std::exp(VX) - std::exp(-VX)) / 2) / KX;
            iPole *= ((std::exp(VX) + std::exp(-VX)) / 2) / KX;
        }

        // s-domain to z-domain conversion
        auto T = 2.0 * std::tan(0.5);
        auto W = 2.0 * PI * cutoffFrequency;
        auto M = rPole * rPole + iPole * iPole;
        auto D = 4.0 - 4.0 * rPole * T + M * T * T;
        auto X0 = (T * T)/D;
        auto X1 = 2.0 * X0;
        auto X2 = X0;
        auto Y1 = (8.0 - 2 * M * T * T)/D;
        auto Y2 = (-4.0 -4.0 * rPole * T - M * T * T)/D;


        auto K = filter == FilterType::HighPass ?
                 -std::cos(W * 0.5 + 0.5)/std::cos(W * 0.5 - 0.5)
                                                : std::sin(-W * 0.5 + 0.5)/std::sin(W * 0.5 + 0.5);

        D = 1.0 + Y1 * K - Y2 * K * K;
        auto A0 = (X0 - X1 * K + X2 * K * K)/D;
        auto A1 = (-2.0 * X0 * K + X1 + X1*K*K - 2*X2*K)/D;
        auto A2 = (X0*K*K - X1*K + X2)/D;
        auto B1 = (2*K + Y1 + Y1*K*K - 2*Y2*K)/D;
        auto B2 = (-(K*K) - Y1*K + Y2)/D;

        if(filter == FilterType::HighPass){
            A1 = -A1;
            B1 = -B1;
        }

        return std::make_tuple(A0, A1, A2, B1, B2);
    }

    template<size_t Poles = 4>
    auto lowPassFilter(double cutoffFrequency){
        assert(cutoffFrequency >= 0 && cutoffFrequency <= 0.5);
        return chebyshev::computeCoefficients<Poles>(FilterType::LowPass, 0.5, cutoffFrequency);
    }

    template<size_t Poles = 4>
    auto highPassFilter(double cutoffFrequency){
        assert(cutoffFrequency >= 0 && cutoffFrequency <= 0.5);
        return chebyshev::computeCoefficients<Poles>(FilterType::HighPass, 0.5, cutoffFrequency);
    }

    auto bandPassFilter(double centerFrequency, double bandwidth){
        const auto cf = centerFrequency;
        const auto BW = bandwidth;
        const auto R = 1 - 3 * BW;
        const auto cos2pif = std::cos(2 * PI * cf);
        const auto K = (1 - 2 * R * cos2pif + R * R) / (2 - 2 * cos2pif);

        BiQuad biQuad{
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

        return biQuad;
    }

    auto bandRejectFilter(double centerFrequency, double bandwidth){
        const auto cf = centerFrequency;
        const auto BW = bandwidth;
        const auto R = 1 - 3 * BW;
        const auto cos2pif = std::cos(2 * PI * cf);
        const auto K = (1 - 2 * R * cos2pif + R * R) / (2 - 2 * cos2pif);

        BiQuad biQuad{
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

        return biQuad;
    }

    auto lowShelf(double frequency, double gain){
        auto u = std::pow(10.0f, gain / 20);
        auto w = frequency;
        auto v = 4.0 / (1 + u);
        auto x = v * std::tan(w / 2);
        auto y = (1 - x) / (1 + x);
        BiQuad bq;
        bq.a[0] = (1 - y)/2;
        bq.a[1] = bq.a[0];
        bq.a[2] = 0;
        bq.b[1] = y;
        bq.b[2] = 0;
        bq.c0 = u - 1;
        bq.d0 = 1.0f;

        return bq;
    }

    auto highShelf(double frequency, double gain){
        auto u = std::pow(10, gain / 20);
        auto w = frequency;
        auto v = (1 + u) / 4;
        auto x = v * std::tan(w / 2);
        auto y = (1 - x) / (1 + x);
        BiQuad bq{};
        bq.a[0] = (1 + y)/2;
        bq.a[1] = -bq.a[0];
        bq.b[1] = y;
        bq.c0 = u - 1;
        bq.d0 = 1;

        return bq;
    }

    auto peakingFilter(double frequency, double gain, double q){
        auto u = std::pow(10, gain / 20);
        auto v = 4 / (1 + u);
        auto w = frequency;
        auto x = std::tan(w / (2 * q));
        auto vx = v * x;
        auto y = .5 * ((1 - vx) / (1 + vx));
        auto z = (.5 + y) * std::cos(w);

        BiQuad bq{};
        bq.a[0] = .5 - y;
        bq.a[2] = -bq.a[0];
        bq.b[1] = 2 * z;
        bq.b[2] = -2 * y;
        bq.c0 = u - 1;
        bq.d0 = 1;

        return bq;
    }
}