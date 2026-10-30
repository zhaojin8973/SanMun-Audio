#include "HallEngine.h"
#include <cmath>

namespace dsp_engines
{
void HallEngine::prepare(double sampleRate, int)
{
    const double rate = std::isfinite(sampleRate) && sampleRate >= 8000.0 ? sampleRate : 48000.0;
    field.prepare(rate);
    cathedral.prepare(rate);
    for (auto& parameter : parameters) parameter.reset(rate, 0.05);
    reset();
}

void HallEngine::reset()
{
    field.reset();
    cathedral.reset();
    initialized = false;
    tick = 0;
}

void HallEngine::process(const float* inL, const float* inR, float* outL, float* outR,
                         int numSamples, const ReverbParameters& p)
{
    const std::array<float, 14> requested {
        p.decay, p.predelay, p.highFreq, p.highShelf, p.bassFreq, p.bassMult,
        p.size, p.attack, p.earlyDiff, p.lateDiff, p.modRate, p.modDepth, p.lowCut, p.highCut
    };
    constexpr std::array<float, 14> lows { .2f, 0, 1000, -24, 50, .25f, 0, 0, 0, 0, .1f, 0, 10, 1000 };
    constexpr std::array<float, 14> highs { 20, 250, 20000, 0, 1000, 4, 100, 100, 100, 100, 10, 100, 500, 20000 };
    constexpr std::array<float, 14> defaults { 3.99f, 20, 6000, -24, 300, 1.5f, 100, 50, 100, 100, 2.53f, 38, 10, 8010 };
    for (std::size_t i = 0; i < parameters.size(); ++i)
    {
        const float value = std::isfinite(requested[i])
            ? std::clamp(requested[i], lows[i], highs[i]) : defaults[i];
        if (initialized) parameters[i].setTargetValue(value);
        else parameters[i].setCurrentAndTargetValue(value);
    }
    initialized = true;
    const bool useCathedral = p.mode == 2;
    for (int n = 0; n < numSamples; ++n)
    {
        std::array<float, 14> v {};
        for (std::size_t i = 0; i < parameters.size(); ++i) v[i] = parameters[i].getNextValue();
        if (tick == 0)
        {
            const sanmu_hall::Settings settings {v[0], v[1], v[2], v[3], v[4], v[5], v[6],
                                                 v[7], v[8], v[9], v[10], v[11], v[12], v[13]};
            if (useCathedral) cathedral.configure(settings);
            else field.configure(settings);
        }
        tick = (tick + 1) & 31u;
        const float left = std::isfinite(inL[n]) ? inL[n] : 0.0f;
        const float right = std::isfinite(inR[n]) ? inR[n] : 0.0f;
        const auto output = useCathedral ? cathedral.process(left, right) : field.process(left, right);
        outL[n] = output[0]; outR[n] = output[1];
    }
}
} // namespace dsp_engines
