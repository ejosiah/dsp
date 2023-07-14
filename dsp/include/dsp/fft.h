#pragma once
#include <complex>
#include <cmath>
#include "constants.h"
#include <numeric>
#include <functional>

namespace dsp {
    enum class FFTType { FORWARD, INVERSE};

    int bitReverse(int x, int N) {
        int log2n = static_cast<int>(std::log2(N));
        int n = 0;
        for (int i=0; i < log2n; i++) {
            n <<= 1;
            n |= (x & 1);
            x >>= 1;
        }
        return n;
    }

    template<typename realType, FFTType type>
    constexpr std::complex<realType> unitComplex() {
        if constexpr (type == FFTType::FORWARD){
            return std::complex<realType>{0, 1};
        }else {
            return std::complex<realType>{0, -1};
        }
    }

    template<FFTType type = FFTType::FORWARD>
    void fft0(std::complex<double>* in, std::complex<double> *out, int log2n)
    {
        constexpr auto J = unitComplex<double, type>();

        int n = 1 << log2n;
        for (unsigned int i=0; i < n; ++i) {
            out[bitReverse(i, n)] = in[i];
        }
        for (int s = 1; s <= log2n; ++s) {
            int m = 1 << s;
            int m2 = m >> 1;
            std::complex<double> w(1, 0);
            std::complex<double> wm = exp(J * (PI / m2));
            for (int j=0; j < m2; ++j) {
                for (int k=j; k < n; k += m) {
                    std::complex<double> t = w * out[k + m2];
                    std::complex<double> u = out[k];
                    out[k] = u + t;
                    out[k + m2] = u - t;
                }
                w *= wm;
            }
        }
    }

    template<typename TimeDomainIterator, typename ComplexIterator, FFTType type = FFTType::FORWARD>
    void fft(TimeDomainIterator tdFirst, TimeDomainIterator tdLast, ComplexIterator c_out, int nf){
        auto size = std::distance(tdFirst, tdLast);

        // TODO remove this let caller handle and assert that size is power of 2
        auto log2n = static_cast<int>(std::ceil(std::log2(nf)));
        auto sizePowerOf2 = static_cast<int>(std::pow(2, log2n));

        std::vector<std::complex<double>> input{};
        for(auto next = tdFirst; next != tdLast; next++){
            std::complex<double> c{*next, 0};
            input.push_back(c);
        }

        auto padding = sizePowerOf2 - size;
        for(int i = 0; i < padding; i++){
            input.emplace_back(0, 0);
        }

        std::vector<std::complex<double>> output(sizePowerOf2);
        fft0<type>(input.data(), output.data(), log2n);

        for(int i = 0; i < sizePowerOf2; i++){
            *c_out = output[i];
            c_out++;
        }
    }

    std::vector<std::complex<double>> shift(std::vector<std::complex<double>> data){
        std::vector<std::complex<double>> output(data.size());
        const auto N = data.size();
        const auto mid = N/2;
        for(int i = 0; i < data.size(); i++){
            auto shiftedIndex = (i + mid)%N;
            output[shiftedIndex] = data[i];
        }
        return output;
    }
}