#pragma once

#include <complex>
#include <cmath>

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
        constexpr double PI = 3.1415926535897932384626433832795;
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
    void fft(TimeDomainIterator tdFirst, TimeDomainIterator tdLast, ComplexIterator c_out){
        auto size = std::distance(tdFirst, tdLast);
        auto log2n = static_cast<int>(std::ceil(std::log2(size)));
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

    template<typename T>
    inline std::vector<T> evenIndices(const std::vector<T>& v){
        auto n = v.size();
        std::vector<T> result;
        result.reserve(n/2);

        for(int i = 0; i < n; i +=2){
            result.push_back(v[i]);
        }
        return result;
    }

    template<typename T>
    inline std::vector<T> oddIndices(const std::vector<T>& v){
        auto n = v.size();
        std::vector<T> result;
        result.reserve(n/2);

        for(int i = 1; i < n; i += 2){
            result.push_back(v[i]);
        }

        return result;
    }

    inline std::vector<std::complex<double>> fft2(const std::vector<std::complex<double>>& p){
        auto n = p.size();
        if(n <= 1) return p;

        constexpr double PI = 3.1415926535897932384626433832795;

        auto even = evenIndices(p);
        auto odd = oddIndices(p);

        auto yEven = fft2(even);
        auto yOdd = fft2(odd);
        static std::complex<double> J{0, 1};
        auto dw = std::exp(J * 2.0 * PI/static_cast<double>(n));
        std::complex<double> w{1, 0};

        std::vector<std::complex<double>> y(n);
        for(int k = 0; k < n/2; k++){
            y[k] = yEven[k] + w * yOdd[k];
            y[k + n/2] = yEven[k] - w * yOdd[k];
            w *= dw;
        }

        return y;
    }

}