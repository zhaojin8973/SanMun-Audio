// ==============================================================================
// AllpassFilter.h - 规范型 Schroeder 全通扩散器原语
//
// 拓扑形式：
//   H(z) = (-g + z^-D) / (1 - g * z^-D)
// 模长 |H(e^jw)| 严格恒等于 1，实现 100% 能量守恒全通扩散。
// 用于 Diffused 模式将清晰离散延迟逐步打散为水洗混响尾音。
// ==============================================================================

#pragma once

#include "DelayLine.h"

namespace dsp_primitives
{

class AllpassFilter
{
public:
    AllpassFilter() = default;

    void prepare(int maxDelaySamples)
    {
        delayLine.prepare(maxDelaySamples + 16);
        delayLine.reset();
        nominalDelay = static_cast<float>(maxDelaySamples);
        feedback = 0.5f;
    }

    void reset() noexcept
    {
        delayLine.reset();
    }

    void setDelay(float samples) noexcept
    {
        nominalDelay = std::max(1.0f, samples);
    }

    void setFeedback(float g) noexcept
    {
        feedback = juce::jlimit(-0.95f, 0.95f, g);
    }

    [[nodiscard]] inline float process(float input, float modOffsetSamples = 0.0f) noexcept
    {
        const float curDelay = std::max(1.0f, nominalDelay + modOffsetSamples);
        const float delayed = delayLine.readHermite(curDelay);

        const float w = input + feedback * delayed;
        const float out = delayed - feedback * w;

        delayLine.write(w);
        return out;
    }

private:
    DelayLine delayLine;
    float nominalDelay = 100.0f;
    float feedback = 0.5f;
};

} // namespace dsp_primitives
