#pragma once

#include <numeric>
#include <audio/choc_SampleBuffers.h>

namespace audio {

    using real_t = float;
    constexpr auto MaxUint32 = std::numeric_limits<uint32_t>::max();
    constexpr auto Maxint32 = std::numeric_limits<int32_t>::max();

    template<typename T, typename U>
    constexpr T as(U u){ return static_cast<T>(u); }

    using InterleavedView = choc::buffer::InterleavedView<real_t>;
    using InterleavedBuffer = choc::buffer::InterleavedBuffer<real_t>;
    using MonoBuffer = choc::buffer::MonoBuffer<real_t>;
    using MonoView = choc::buffer::MonoView<real_t>;

    constexpr real_t PI = 3.1415926535897932384626433832795;
    constexpr real_t TWO_PI = 6.283185307179586476925286766559;
}