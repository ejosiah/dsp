#pragma once

#include <complex>
#include <vector>

//namespace dsp {
//
//    class fft {
//    public:
//        enum class Type { FORWARD, INVERSE};
//
//        template<typename Iterator>
//        void compute(Iterator first, Iterator last, Type type){
//            auto size = std::distance(first, last);
//            if(m_input.empty() || m_input.size() != size){
//                m_input.resize(size);
//                m_output.resize(size);
//            }
//        }
//
//    private:
//        std::vector<std::complex<double>> m_input;
//        std::vector<std::complex<double>> m_output;
//
//    };
//}