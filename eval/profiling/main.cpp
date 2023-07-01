#include <benchmark/benchmark.h>
#include <string>
#include <dsp/chebyshev.h>
#include <dsp/util.h>

static auto computeCoefficients = dsp::memorize<std::tuple<dsp::Coefficients, dsp::Coefficients>>(dsp::chebyshev::computeCoefficients);

void ComputeCoefficients(){
    dsp::chebyshev::computeCoefficients(dsp::FilterType::LowPass, 0.5, 4, 0.01);
}

void ComputeCoefficientsCached(){
    computeCoefficients(dsp::FilterType::LowPass, 0.5, 4, 0.01);
}

static void BM_ComputeCoefficients(benchmark::State& state) {
    // Perform setup here
    for (auto _ : state) {
        // This code gets timed
        ComputeCoefficients();
    }
}

static void BM_ComputeCoefficientsCached(benchmark::State& state) {
    // Perform setup here
    for (auto _ : state) {
        // This code gets timed
        ComputeCoefficientsCached();
    }
}

static constexpr int n = 10000;

int array[n][n];

static void BM_RowMajorTraversal(benchmark::State& state) {
    for (auto _: state) {
        for (int i = 0; i < n; ++i) {
            for (int j = 0; j < n; ++j) {
                array[i][j] += static_cast<int>(std::sqrt(std::hash<int>()(j * n + i)));
            }
        }
    }
}


static void BM_ColumnMajorTraversal(benchmark::State& state){
    for(auto _ : state){
        for(int i = 0; i < n; ++i){
            for(int j = 0; j < n; ++j){
                array[j][i] += static_cast<int>(std::sqrt(std::hash<int>()(i * n + j)));
            }
        }
    }
}
// Register the function as a benchmark
BENCHMARK(BM_ColumnMajorTraversal);
BENCHMARK(BM_RowMajorTraversal);
// Run the benchmark
BENCHMARK_MAIN();