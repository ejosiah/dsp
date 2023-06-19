#include "pink_noise.h"

PinkNoise::PinkNoise(int numRows, unsigned seed):  m_seed{ seed }
{
    long pmax = 0;
    m_index = 0;
    m_indexMask = (1 << numRows) - 1;
    /* Calculate maximum possible signed random value. Extra 1 for white noise always added. */
    pmax = (numRows + 1) * (1<<(PINK_RANDOM_BITS-1));
    m_scalar = 1.0f / static_cast<float>(pmax);

    std::fill_n(m_rows.begin(), m_rows.size(), 0);
    m_runningSum = 0;

    std::default_random_engine engine{m_seed};
    std::uniform_int_distribution<long> dist{ 0L, std::numeric_limits<long>::max() };
    m_rng = std::bind(dist, engine);
}

float PinkNoise::next()  {

    m_index = (m_index + 1) & m_indexMask;

    long newRandom;
    if(m_index != 0){

        /* Determine how many trailing zeros in PinkIndex. */
        /* This algorithm will hang if n==0 so test first. */
        int numZeros = 0;
        int n = m_index;
        while((n & 1) == 0){
            n = n >> 1;
            numZeros++;
        }
        /* Replace the indexed ROWS random value.
         * Subtract and add back to RunningSum instead of adding all the random
         * values together. Only one changes each time.
         */
        m_runningSum -= m_rows[numZeros];
        newRandom = m_rng() >> PINK_RANDOM_SHIFT;
        m_runningSum += newRandom;
        m_rows[numZeros] = newRandom;
    }

    /* Add extra white noise value. */
    newRandom = m_rng() >> PINK_RANDOM_SHIFT;
    auto sum = m_runningSum + newRandom;
    float output = m_scalar * static_cast<float>(sum);
#ifdef  PINK_MEASURE
    if(output > pinkMax) pinkMax = output;
    else if(output < pinkMin) pinkMin = output;
#endif
    return output;
}
