#include "ChamberEngine.h"

namespace dsp_engines
{
void ChamberEngine::prepare(double sampleRate, int)
{
    const double rate = std::isfinite(sampleRate) && sampleRate >= 8000 ? sampleRate : 48000;
    field.prepare(rate);
    for (auto& parameter : parameters) parameter.reset(rate, .06);
    reset();
}

void ChamberEngine::reset()
{
    field.reset();
    initialized = false;
    tick = 0;
    activeStyle = -1;
}

void ChamberEngine::process(const float* inL, const float* inR, float* outL, float* outR,
                            int numSamples, const ReverbParameters& p)
{
    const int style = p.mode == 1 ? 1 : 0;
    if (style != activeStyle)
    {
        field.reset();
        initialized = false;
        tick = 0;
        activeStyle = style;
    }
    const std::array<float, 14> requested { p.decay, p.predelay, p.highFreq, p.highShelf,
        p.bassFreq, p.bassMult, p.size, p.attack, p.earlyDiff, p.lateDiff,
        p.modRate, p.modDepth, p.lowCut, p.highCut };
    constexpr std::array<float, 14> low { .2f, 0, 1000, -24, 50, .25f, 0, 0, 0, 0, .1f, 0, 10, 1000 };
    constexpr std::array<float, 14> high { 20, 250, 20000, 0, 1000, 4, 100, 100, 100, 100, 10, 100, 500, 20000 };
    constexpr std::array<float, 14> fallback { 2, 15, 6000, -16, 300, 1, 100, 35, 100, 100, .5f, 20, 20, 9000 };
    for (std::size_t i = 0; i < parameters.size(); ++i)
    {
        const float value = std::isfinite(requested[i]) ? std::clamp(requested[i], low[i], high[i]) : fallback[i];
        if (initialized) parameters[i].setTargetValue(value);
        else parameters[i].setCurrentAndTargetValue(value);
    }
    initialized = true;
    for (int n = 0; n < numSamples; ++n)
    {
        std::array<float, 14> v {};
        for (std::size_t i = 0; i < v.size(); ++i) v[i] = parameters[i].getNextValue();
        if (tick == 0)
            field.configure({ v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7],
                              v[8], v[9], v[10], v[11], v[12], v[13] }, style);
        tick = (tick + 1) & 31u;
        const auto y = field.process(std::isfinite(inL[n]) ? inL[n] : 0,
                                     std::isfinite(inR[n]) ? inR[n] : 0);
        outL[n] = y[0];
        outR[n] = y[1];
    }
}
}
