#pragma once


#include <array>
#include <random>
#include <algorithm>
#include <limits>
#ifdef PINK_MEASURE
float pinkMax = -999.0;
float pinkMin =  999.0;
#endif

class PinkNoise{
public:
    PinkNoise(int numRows, unsigned seed = std::random_device()());

    [[nodiscard]]
    float next();

private:
    static constexpr long PINK_MAX_RANDOM_ROWS = 30;
    static constexpr long PINK_RANDOM_BITS = 24;
    static constexpr long PINK_RANDOM_SHIFT = ((sizeof(long)*8)-PINK_RANDOM_BITS);

    std::array<long, PINK_MAX_RANDOM_ROWS> m_rows;
    long m_runningSum;
    int m_index;
    int m_indexMask;
    float m_scalar;
    unsigned m_seed;
    std::function<long()> m_rng;
};