// ==============================================================================
// OnePoleFilters.h - 工业级 TPT 单极滤波与 3 频段精准声学回路阻尼
// ==============================================================================

#pragma once

#include <juce_core/juce_core.h>
#include <cmath>
#include <algorithm>

namespace dsp_primitives
{

// ---- 高精度单极低通滤波器 (TPT 拓扑保持结构) ----
class OnePoleLowpass
{
public:
    OnePoleLowpass() = default;

    void prepare(double sampleRate) noexcept
    {
        sr = sampleRate;
        reset();
    }

    void reset() noexcept
    {
        s = 0.0f;
    }

    void setCutoff(float cutoffHz) noexcept
    {
        const float fc = juce::jlimit(20.0f, static_cast<float>(sr * 0.49), cutoffHz);
        const float wd = 2.0f * juce::MathConstants<float>::pi * fc;
        const float wa = 2.0f * static_cast<float>(sr) * std::tan(wd / (2.0f * static_cast<float>(sr)));
        g = wa / (2.0f * static_cast<float>(sr));
        h = g / (1.0f + g);
    }

    [[nodiscard]] inline float process(float x) noexcept
    {
        const float v = (x - s) * h;
        const float y = v + s;
        s = y + v;
        if (std::abs(s) < 1.0e-15f) s = 0.0f;
        return y;
    }

private:
    double sr = 48000.0;
    float g = 0.0f;
    float h = 0.0f;
    float s = 0.0f;
};

// ---- 高精度单极高通滤波器 (TPT 拓扑保持结构) ----
class OnePoleHighpass
{
public:
    OnePoleHighpass() = default;

    void prepare(double sampleRate) noexcept
    {
        lp.prepare(sampleRate);
    }

    void reset() noexcept
    {
        lp.reset();
    }

    void setCutoff(float cutoffHz) noexcept
    {
        lp.setCutoff(cutoffHz);
    }

    [[nodiscard]] inline float process(float x) noexcept
    {
        return x - lp.process(x);
    }

private:
    OnePoleLowpass lp;
};

// ---- 反馈环路声学阻尼器 (Jot 连续频谱级联阻尼与 Bass Mult) ----
// 物理声学机制：
//   - 空气吸收对数衰减律：随频率连续二次方衰减 (f^2)，15kHz 最先消散 -> 8kHz 次之 -> 4kHz 柔和 -> 1kHz 稳定 4.0s
//   - 低频倍率 (BassMult)：在 BassFreq 以下自然延展至 T60 * BassMult (如 5~6s)
//   - 各线独立延迟归一化：消除小循环重复过度滤波
class LoopDampingFilter
{
public:
    LoopDampingFilter() = default;

    void prepare(double sampleRate) noexcept
    {
        sr = sampleRate;
        reset();
    }

    void reset() noexcept
    {
        yDamp = 0.0f;
        sBass = 0.0f;
    }

    void setAcousticDamping(float highFreqHz, float highShelfDb,
                            float bassFreqHz, float bassMult,
                            float lineDelaySec, float t60) noexcept
    {
        if (t60 <= 0.05f || lineDelaySec <= 0.0f)
        {
            damp = 0.0f;
            kBass = 0.0f;
            return;
        }

        // 1. 高频空气吸收连续衰减 (根据目标分频点精确推导单极极点 d)
        if (highShelfDb < -0.1f)
        {
            const float highT60 = std::max(0.1f, t60 * std::pow(10.0f, highShelfDb / 60.0f));
            const float deltaExpHigh = -6.907755f * lineDelaySec * (1.0f / highT60 - 1.0f / t60);
            const float r = juce::jlimit(0.15f, 0.999f, std::exp(deltaExpHigh));

            const float safeHighFreq = juce::jlimit(500.0f, static_cast<float>(sr * 0.45), highFreqHz);
            const float wc = 2.0f * juce::MathConstants<float>::pi * safeHighFreq / static_cast<float>(sr);
            const float cosWc = std::cos(wc);

            const float r2 = r * r;
            const float denom = 1.0f - r2;
            if (denom > 0.001f)
            {
                const float B = (1.0f - r2 * cosWc) / denom;
                if (B > 1.0001f)
                    damp = juce::jlimit(0.0f, 0.78f, B - std::sqrt(B * B - 1.0f));
                else
                    damp = 0.0f;
            }
            else
            {
                damp = 0.0f;
            }
        }
        else
        {
            damp = 0.0f;
        }

        // 2. 低频温暖度倍率 (BassMult)
        const float safeBassFreq = juce::jlimit(40.0f, 1200.0f, bassFreqHz);
        alphaBass = 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi * safeBassFreq / static_cast<float>(sr));

        const float safeBassMult = juce::jlimit(0.25f, 4.0f, bassMult);
        if (std::abs(safeBassMult - 1.0f) > 0.02f)
        {
            const float bassT60 = std::max(0.1f, t60 * safeBassMult);
            const float deltaExpBass = -6.907755f * lineDelaySec * (1.0f / bassT60 - 1.0f / t60);
            const float gRelBass = std::exp(deltaExpBass);
            kBass = juce::jlimit(-0.55f, 0.35f, gRelBass - 1.0f);
        }
        else
        {
            kBass = 0.0f;
        }
    }

    void setDamping(float highFreqHz, float highShelfDb, float lineDelaySec, float t60) noexcept
    {
        setAcousticDamping(highFreqHz, highShelfDb, 300.0f, 1.0f, lineDelaySec, t60);
    }

    void setDampingFrequency(float highFreqHz, float highShelfDb) noexcept
    {
        setAcousticDamping(highFreqHz, highShelfDb, 300.0f, 1.0f, 0.025f, 2.5f);
    }

    [[nodiscard]] inline float process(float x) noexcept
    {
        // 1. 连续一阶低通阻尼 (自然呈现高频先死、中频后死的空气吸收先后顺序)
        yDamp = x * (1.0f - damp) + yDamp * damp;
        if (std::abs(yDamp) < 1.0e-15f) yDamp = 0.0f;

        // 2. 低频补偿/增益 (延展低频尾音)
        sBass += alphaBass * (x - sBass);
        if (std::abs(sBass) < 1.0e-15f) sBass = 0.0f;

        float y = yDamp + kBass * sBass;
        if (std::abs(y) < 1.0e-15f) y = 0.0f;
        return y;
    }

private:
    double sr = 48000.0;
    float damp = 0.0f;
    float yDamp = 0.0f;

    float alphaBass = 0.05f;
    float kBass = 0.0f;
    float sBass = 0.0f;
};

} // namespace dsp_primitives
