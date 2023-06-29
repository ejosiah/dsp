#pragma once

#include <tuple>
#include "dsp.h"
#include <cmath>
#include <iostream>

namespace dsp {

    using Poles = std::tuple<double, double, double, double, double>;

    std::tuple<Coefficients, Coefficients> computeCoefficients(FilterType filter, double passBandRipple, int poles, double cutoffFrequency);

    Poles computePoles(FilterType filter, double passBandRipple, int numPoles, int pole, double cutoffFrequency){
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

            std::cout << "\n\nvalues at line 1200\n";
            std::cout << "\tRP = " << rPole << "\n";
            std::cout << "\tiP = " << iPole << "\n";
            std::cout << "\tES = " << ES << "\n";
            std::cout << "\tVX = " << VX << "\n";
            std::cout << "\tKX = " << KX << "\n\n";
        }

        // s-domain to z-domain conversion
        auto T = 2.0 * std::tan(0.5);
        auto W = 2.0 * PI * cutoffFrequency;
        auto M = rPole * rPole + iPole * iPole;
        auto D = 4.0 - 4.0 * rPole * T + (M * T) * (M * T);
        auto X0 = (T * T)/D;
        auto X1 = 2.0 * X0;
        auto X2 = X0;
        auto Y1 = (8.0 - 2 * ((M * T) * (M * T)))/D;
        auto Y2 = (-4.0 -4.0 * rPole * T - ((M * T) * (M * T)))/D;

        std::cout << "values at line 1310\n";
        std::cout << "\tT  = " << T << "\n";
        std::cout << "\tW  = " << W << "\n";
        std::cout << "\tM  = " << M << "\n";
        std::cout << "\tD  = " << D << "\n";
        std::cout << "\tX0 = " << X0 << "\n";
        std::cout << "\tX1 = " << X1 << "\n";
        std::cout << "\tX2 = " << X2 << "\n";
        std::cout << "\tY1 = " << Y1 << "\n";
        std::cout << "\tY2 = " << Y2 << "\n\n";


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
}