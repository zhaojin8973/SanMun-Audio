// ==============================================================================
// EarlyReflections.h - 12 抽头真实声学房间早期反射树原语
// ==============================================================================

#pragma once

#include "DelayLine.h"
#include "AllpassFilter.h"
#include <array>

namespace dsp_primitives
{

class EarlyReflections
{
public:
    EarlyReflections() = default;

    void prepare(double sampleRate)
    {
        sr = sampleRate;
        delayLineL.prepare(static_cast<int>(sr * 0.15)); // 最大 150ms
        delayLineR.prepare(static_cast<int>(sr * 0.15));
        diffuserL.prepare(128);
        diffuserR.prepare(128);
        reset();
    }

    void reset() noexcept
    {
        delayLineL.reset();
        delayLineR.reset();
        diffuserL.reset();
        diffuserR.reset();
    }

    // 12 个真实房间几何抽头基准时延 (ms) 与增益
    struct Tap
    {
        float delayMs;
        float gainL;
        float gainR;
    };

    static constexpr size_t kNumTaps = 12;

    void process(float inL, float inR, float sizePercent, float earlyDiffPercent,
                 float& outL, float& outR) noexcept
    {
        delayLineL.write(inL);
        delayLineR.write(inR);

        // 基础几何反射时延数据表 (ms, gainL, gainR) - 严格全正极性，杜绝单声道相位抵消
        static constexpr Tap taps[kNumTaps] = {
            {  4.3f,  0.82f,  0.18f },
            {  7.1f,  0.18f,  0.80f },
            { 11.5f,  0.70f,  0.28f },
            { 14.8f,  0.24f,  0.68f },
            { 19.2f,  0.56f,  0.50f },
            { 23.6f,  0.48f,  0.38f },
            { 28.1f,  0.32f,  0.45f },
            { 33.4f,  0.38f,  0.34f },
            { 39.0f,  0.30f,  0.26f },
            { 45.2f,  0.24f,  0.28f },
            { 52.0f,  0.20f,  0.22f },
            { 59.5f,  0.18f,  0.16f }
        };

        const float sizeScale = juce::jmap(sizePercent / 100.0f, 0.3f, 1.8f);
        float sumL = 0.0f;
        float sumR = 0.0f;

        for (size_t i = 0; i < kNumTaps; ++i)
        {
            const float delaySamples = taps[i].delayMs * 0.001f * sizeScale * static_cast<float>(sr);
            const float sL = delayLineL.readLinear(delaySamples);
            const float sR = delayLineR.readLinear(delaySamples);

            sumL += sL * taps[i].gainL;
            sumR += sR * taps[i].gainR;
        }

        // 早期扩散微调全通平滑 (Early Diff)
        const float diffG = (earlyDiffPercent / 100.0f) * 0.6f;
        diffuserL.setFeedback(diffG);
        diffuserR.setFeedback(diffG);

        outL = diffuserL.process(sumL);
        outR = diffuserR.process(sumR);
    }

private:
    double sr = 48000.0;
    DelayLine delayLineL;
    DelayLine delayLineR;
    AllpassFilter diffuserL;
    AllpassFilter diffuserR;
};

} // namespace dsp_primitives
