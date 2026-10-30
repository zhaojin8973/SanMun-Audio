// ==============================================================================
// TptSvf.h - 三输出 TPT 状态变量滤波器（Andrew Simper 技术，双精度状态）
// 来源：SanMu EQ 共享核心滤波原语
// 用于 SanMu Delay 反馈环路阻尼（HighCut / LowCut）及模拟频响整形
// ==============================================================================

#pragma once

#include <juce_dsp/juce_dsp.h>
#include <cmath>

namespace dsp_primitives
{

class TptSvf
{
public:
    enum class Type
    {
        LowPass,
        BandPass,
        HighPass,
        Bell,
        LowShelf,
        HighShelf
    };

    void setSampleRate(double sr)
    {
        sampleRate = sr;
        update();
    }

    void setType(Type newType)
    {
        type = newType;
    }

    void setCutoff(double newCutoffHz)
    {
        cutoff = juce::jlimit(1.0, sampleRate * 0.49, newCutoffHz);
        update();
    }

    void setQ(double newQ)
    {
        q = juce::jmax(0.01, newQ);
        update();
    }

    void setGainDb(double gainDb)
    {
        gainLinear = juce::Decibels::decibelsToGain(
            static_cast<float>(gainDb), -60.0f);
        update();
    }

    void reset()
    {
        s1 = 0.0;
        s2 = 0.0;
    }

    // 单声道处理一个采样：输入/输出用 float，内部状态用 double
    float processSample(float inputValue)
    {
        const double in = static_cast<double>(inputValue);

        const double yHP = h * (in - s1 * (g + R2) - s2);
        const double yBP = yHP * g + s1;
        s1 = yHP * g + yBP;
        const double yLP = yBP * g + s2;
        s2 = yBP * g + yLP;

        double out = 0.0;
        switch (type)
        {
            case Type::LowPass:   out = yLP; break;
            case Type::HighPass:  out = yHP; break;
            case Type::BandPass:  out = yBP; break;
            case Type::Bell:      out = in + (gainLinear - 1.0) * R2 * yBP; break;
            case Type::LowShelf:
            {
                const double A = std::sqrt(juce::jmax(1e-6, gainLinear));
                out = in + R2 * (A - 1.0) * yBP + (gainLinear - 1.0) * yLP;
                break;
            }
            case Type::HighShelf:
            {
                const double A = std::sqrt(juce::jmax(1e-6, gainLinear));
                out = in + R2 * (A - 1.0) * yBP + (gainLinear - 1.0) * yHP;
                break;
            }
        }

        return static_cast<float>(out);
    }

private:
    void update()
    {
        const double safeQ = juce::jmax(0.01, q);
        if (type == Type::Bell)
        {
            const double A = std::sqrt(juce::jmax(1e-6, gainLinear));
            g  = std::tan(juce::MathConstants<double>::pi * cutoff / sampleRate);
            R2 = 1.0 / (safeQ * A);
        }
        else if (type == Type::LowShelf)
        {
            const double A = std::sqrt(juce::jmax(1e-6, gainLinear));
            g  = std::tan(juce::MathConstants<double>::pi * cutoff / sampleRate) / std::sqrt(A);
            R2 = 1.0 / safeQ;
        }
        else if (type == Type::HighShelf)
        {
            const double A = std::sqrt(juce::jmax(1e-6, gainLinear));
            g  = std::tan(juce::MathConstants<double>::pi * cutoff / sampleRate) * std::sqrt(A);
            R2 = 1.0 / safeQ;
        }
        else
        {
            g  = std::tan(juce::MathConstants<double>::pi * cutoff / sampleRate);
            R2 = 1.0 / safeQ;
        }
        h  = 1.0 / (1.0 + R2 * g + g * g);
    }

    double sampleRate = 44100.0;
    double cutoff     = 1000.0;
    double q          = 0.7071067811865476;
    double gainLinear = 1.0;
    double g = 0.0, R2 = 0.0, h = 0.0;
    double s1 = 0.0, s2 = 0.0;
    Type type = Type::LowPass;
};

} // namespace dsp_primitives
