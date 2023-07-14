#pragma once


#include <cstdint>
#include <dsp/recursive_filters.h>
#include <random>
#include <functional>
#include <audio/choc_Oscillators.h>
#include <utility>
#include <vector>
#include <audio/circular_buffer.h>
#include <tuple>

using Range = std::tuple<float, float>;
using WhiteNoise = std::function<float()>;

WhiteNoise whiteNoise(){
    std::uniform_real_distribution<float> dist{-1, 1};
    std::default_random_engine engine{std::random_device{}()};
    return [=]() mutable {
        return dist(engine);
    };
}


class WindGust{
public:
    explicit WindGust(uint32_t sampleRate = 0)
    : m_sampleRate{sampleRate}
    , m_lop{ dsp::recursive::lowPassFilter<2>(0.5 / sampleRate) }
    , m_hip{ dsp::recursive::highPassFilter<2>(0) }
    , m_noise{whiteNoise()}
    {}

    float getSample(float iSample){
        auto& lop = m_lop;
        auto& hip = m_hip;
        auto& noise = m_noise;

        iSample *= 0.5;
        iSample *= iSample;
        iSample -= 0.125;

        auto oSample = hip(lop(noise())) * 50.f;

        return iSample * oSample;
    }

private:
    uint32_t m_sampleRate;
    dsp::BiQuad m_lop;
    dsp::BiQuad m_hip;
    WhiteNoise m_noise;
};

class WindSquall{
public:
    explicit WindSquall(uint32_t sampleRate = 0)
    : m_sampleRate{sampleRate}
    , m_lop{ dsp::recursive::lowPassFilter<2>(3.0 / sampleRate) }
    , m_hip{ dsp::recursive::highPassFilter<2>(0) }
    , m_noise{whiteNoise()}
    {}

    float getSample(float iSample) {
        auto& lop = m_lop;
        auto& hip = m_hip;
        auto& noise = m_noise;

        iSample = (std::max(0.4f, iSample) - 0.4f) * 8.0f;
        iSample *= iSample;

        auto oSample = hip(lop(noise())) * 20;

        return iSample * oSample;
    }
private:
    uint32_t m_sampleRate;
    dsp::BiQuad m_lop;
    dsp::BiQuad m_hip;
    WhiteNoise m_noise;
};

class WindSpeed{
public:
    explicit WindSpeed(uint32_t sampleRate = 0)
    : m_gust{ sampleRate }
    , m_squall{ sampleRate }
    , m_osc{}
    {
        m_osc.setFrequency(0.1, static_cast<float>(sampleRate));
    }

    float getSample() {
        auto wave = m_osc.getSample() + 1.f;
        wave *= 0.25;
        auto gust = m_gust.getSample(wave);
        auto squall = m_squall.getSample(wave);

        auto sample = wave + gust + squall;
        sample = std::clamp(sample, 0.f, 1.0f);

        return sample;
    }

private:
    choc::oscillator::Sine<float> m_osc;
    WindGust m_gust;
    WindSquall m_squall;
};


class WindGenerator{
public:
    explicit WindGenerator(uint32_t sampleRate = 0)
    : m_windSpeed{ sampleRate }
    , m_bp{ dsp::recursive::bandPassFilter(800.0f/sampleRate, 0.01) }
    , m_noise{whiteNoise()}
    {}

    float getSample(){
        auto ws = m_windSpeed.getSample();
        auto ns = m_bp(m_noise());
        auto sample = (ws + 0.2f) * ns;
        sample *= 0.3f;
        return sample;
    }

private:
    WindSpeed m_windSpeed{};
    dsp::BiQuad m_bp;
    WhiteNoise m_noise;
};

class Whistling{
public:
    explicit Whistling(float lower, float upper, float scale, float delay, uint32_t sampleRate, float offset = 0)
    : m_sampleRate{ static_cast<float>(sampleRate) }
    , m_period{ 1/static_cast<float>(sampleRate) }
    , m_delay{ delay / static_cast<float>(sampleRate) }
    , m_windSpeed{ sampleRate }
    , m_bp{ }
    , m_noise{whiteNoise()}
    , m_lower{ lower }
    , m_upper{ upper }
    , m_scale{ scale }
    , m_offset{ offset }
    {}

    float getSample() {
        static float period = 0;
        period += m_period;
        if(period < m_delay){
            return 0;
        }
        auto ws = m_windSpeed.getSample();
        auto fc = ws * m_lower + m_upper;
        fc /= m_sampleRate;
        auto bw = 60.f/m_sampleRate;
        auto bp = dsp::recursive::bandPassFilter(fc, bw);
        m_bp.a = bp.a;
        m_bp.b = bp.b;

        auto sample = m_bp(m_noise());
        sample *= (ws + m_offset) * (ws + m_offset);
        sample *= m_scale;
        return sample;
    }

private:
    WindSpeed m_windSpeed{};
    dsp::BiQuad m_bp;
    WhiteNoise m_noise;
    float m_delay{};
    float m_period;
    float m_sampleRate{};
    float m_lower{};
    float m_upper{};
    float m_scale{};
    float m_offset{};
};

class TreeLeaves{
public:
    TreeLeaves(uint32_t sampleRate)
    : m_delay{ 2000.f/static_cast<float>(sampleRate) }
    , m_period{ 1/static_cast<float>(sampleRate) }
    , m_lopL{ dsp::recursive::lowPassFilter<2>(0.1f/static_cast<float>(sampleRate)) }
    , m_lopH{ dsp::recursive::lowPassFilter<2>(4000.f/static_cast<float>(sampleRate)) }
    , m_hip{ dsp::recursive::highPassFilter<2>(200.f/static_cast<float>(sampleRate)) }
    , m_noise( whiteNoise() )
    , m_windSpeed( sampleRate )
    {}

    float getSample() {
        static float period = 0;
        period += m_period;
        if(period < m_delay){
            return 0;
        }
        auto wind = m_lopL(m_windSpeed.getSample());
        auto sample = 1.f - (wind * 0.4f + 0.3f);
        sample = (std::max(m_noise(), sample) - sample) * sample;
        sample = m_lopH(m_hip(sample));
        sample *= wind * 1.2f;

        return sample;
    }

private:
    float m_delay{};
    dsp::BiQuad m_lopL;
    dsp::BiQuad m_lopH;
    dsp::BiQuad m_hip;
    WhiteNoise m_noise;
    WindSpeed m_windSpeed{};
    float m_period;
};

class Howls{
public:
    Howls(uint32_t sampleRate, float delay, Range clip, float lopCF, float bpCF, float bpBW, float offset0, float offset1, float scale)
    : m_sampleRate{ static_cast<float>(sampleRate)}
    , m_clip{std::move(clip)}
    , m_lop( dsp::recursive::lowPassFilter<2>(lopCF/static_cast<float>(sampleRate)))
    , m_bp{ dsp::recursive::bandPassFilter( bpCF / static_cast<float>(sampleRate), bpBW / static_cast<float>(sampleRate) ) }
    , m_delay{ delay / static_cast<float>(sampleRate) }
    , m_period{ 1/static_cast<float>(sampleRate) }
    , m_offset0{ offset0 }
    , m_offset1{ offset1 }
    , m_scale{ scale }
    , m_windSpeed{ sampleRate }
    , m_noise{ whiteNoise() }
    , m_osc{}
    {}

    float getSample() {
        static float period = 0;
        period += m_period;
        if(period < m_delay){
            return 0;
        }
        const auto [l, h] = m_clip;
        float wind = std::clamp(m_windSpeed.getSample(), l, h) - m_offset0;
        wind = std::cosf(dsp::_2_PI * ((wind * 2.f) - 0.25f));
        wind = m_lop(wind);

        m_osc.setFrequency(wind * m_scale + m_offset1, m_sampleRate);
        auto sample = m_osc.getSample();
        sample = m_bp(m_noise()) * wind * 2.f * sample;

        return sample;
    }

private:
    dsp::BiQuad m_lop;
    dsp::BiQuad m_bp;
    Range m_clip;
    float m_offset0;
    float m_scale;
    float m_offset1;
    WindSpeed m_windSpeed;
    float m_period;
    float m_delay;
    float m_sampleRate;
    WhiteNoise m_noise;
    choc::oscillator::Sine<float> m_osc;
};