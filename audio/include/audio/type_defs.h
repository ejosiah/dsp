#pragma once

#include <numeric>

namespace audio {
    using real_t = float;
    constexpr auto MaxUint32 = std::numeric_limits<uint32_t>::max();
    constexpr auto Maxint32 = std::numeric_limits<int32_t>::max();

    template<typename T, typename U>
    constexpr T as(U u){ return static_cast<T>(u); }
}