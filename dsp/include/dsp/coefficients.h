#pragma

namespace dsp {
    template<size_t Poles>
    struct Coefficients {
        std::array<double, Poles + 1u> a{};
        std::array<double, Poles + 1u> b{};
        std::array<double, Poles> x{};
        std::array<double, Poles> y{};

        double c0{1};
        double d0{0};

        template<typename SampleType>
        SampleBuffer<SampleType> operator()(const SampleBuffer<SampleType> &input);

        template<typename SampleType, size_t Capacity>
        void operator()(const SampleBuffer<SampleType> &input, CircularBuffer<SampleType, Capacity> &output);

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

        double c0{1};
        double d0{0};

        template<typename SampleType>
        SampleBuffer<SampleType> operator()(const SampleBuffer<SampleType> &input);

        template<typename SampleType, size_t Capacity>
        void operator()(const SampleBuffer<SampleType> &input, CircularBuffer<SampleType, Capacity> &output);

//        template<typename SampleType, size_t Capacity>
//        auto stream();
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
        const auto aN = A.size();
        const auto bN = B.size();
        SampleBuffer<SampleType> output(N);
        const auto &X = input;
        auto &Y = output;

        for (auto i = bN; i < N; i++) {
            for (auto j = 0; j < aN; j++) {
                Y[i] += A[j] * X[i - j];
            }

            for (auto j = 1; j < bN; j++) {
                Y[i] += B[j] * Y[i - j];
            }
            Y[i] = Y[i] * c0 + X[i] * d0;
        }

        return output;
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

//    template<size_t Poles>
//    template<typename SampleType, size_t Capacity>
//    auto Coefficients<Poles>::stream() {
//        return [A = a, B = b, prevInput=std::array<double, Poles>{}](const SampleBuffer<SampleType> &input, CircularBuffer<SampleType, Capacity> &output){
//            assert(B[0] == 0);
//            assert(!A.empty());
//            assert(!B.empty());
//
//            const auto N = input.size();
//            const auto aN = A.size();
//            const auto bN = B.size();
//            SampleBuffer<SampleType> X(input.size() + Poles);
//            auto dest = X.begin();
//            std::copy(prevInput.begin(), prevInput.end(), dest);
//            std::advance(dest, Poles);
//            std::copy(input.cbegin(), input.cend(), dest);
//
//            auto& Y = output;
//
//            for(auto i = 0; i < N; i++){
//                Y[i] = 0;
//                for(auto j = 0; j < aN; j++){
//                    Y[i] += A[j] * X[i + Poles - j];
//                }
//
//                for(auto j = 1; j < bN; j++){
//                    Y[i] += B[j] * Y[i - j];
//                }
//            }
//            auto start = input.cend();
//            std::advance(start, -Poles);
//            std::copy(start, input.cend(), prevInput.begin());
//        };
//    }

    template<typename SampleType>
    struct BQState {
        SampleType x1{}, x2{}, y1{}, y2{};
    };

    template<typename SampleType>
    SampleType processSample(BiQuad& bq, BQState<SampleType>& st, SampleType sample){
        auto y = bq.a0 * sample + bq.a1 * st.x1 + bq.a2 * st.x2;
            y +=                  bq.b1 * st.y1 + bq.b2 * st.y2;

        st.x2 = st.x1;
        st.x1 = sample;
        st.y2 = st.y1;
        st.y1 = y;
        return (y * bq.c0) + (sample * bq.d0);
    }

    template<typename SampleType>
    SampleBuffer<SampleType> Coefficients<2>::operator()(const SampleBuffer<SampleType> &input) {
        SampleBuffer<SampleType> output{};

        BQState<SampleType> state{};
        const auto N = input.size();
        for(int i = 0; i < N; i++){
            auto iSample = input[i];
            auto oSample = processSample(*this, state, iSample);
            output.add(oSample);
        }
        return output;
    }

    template<typename SampleType, size_t Capacity>
    void Coefficients<2>::operator()(const SampleBuffer<SampleType> &input, CircularBuffer<SampleType, Capacity> &output) {

        static BQState<SampleType> state{};
        const auto N = input.size();
        for(int i = 0; i < N; i++){
            auto iSample = input[i];
            auto oSample = processSample(*this, state, iSample);
            output[i] = oSample;
        }
    }
}