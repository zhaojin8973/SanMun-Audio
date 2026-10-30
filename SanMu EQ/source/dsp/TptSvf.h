// ==============================================================================
// TptSvf.h - 三输出 TPT 状态变量滤波器（Andrew Simper 技术，双精度状态）
// ==============================================================================
//
// 这是为 SVF/TPT 拓扑升级准备的独立滤波器类：
// - 同时产生 lowpass / bandpass / highpass 三个输出；
// - 内部状态累加使用 double（消除单精度量化噪声与极限环）；
// - 通过三个输出混合得到 Bell / LowShelf / HighShelf；
// - LowCut = highpass，HighCut = lowpass（12dB/oct，多级级联在 EqBand 完成）。
//
// 系数与 JUCE 的 StateVariableTPTFilter 一致：
//   g  = tan(pi * fc / sr)
//   R2 = 1 / Q（即阻尼）
//   h  = 1 / (1 + R2*g + g^2)

#pragma once

#include <juce_dsp/juce_dsp.h>

#include <cmath>

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
            // 增益对称型 Bell：带通增益通过 R2 归一化并乘以 (gainLinear - 1)
            case Type::Bell:      out = in + (gainLinear - 1.0) * R2 * yBP; break;
            // Andrew Simper 工业级标准 Low/High Shelf 拓扑（带通阻尼补偿与预扭曲）
            // 彻底消除由于零点失配造成的 1k~8k 中频凹陷/串扰 BUG
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

    // 纯函数：用模拟原型传递函数求给定滤波器的幅度响应（dB），
    // 不依赖滤波器状态，可供 UI 线程批量计算频响曲线。
    static double magnitudeDb(Type type, double cutoff, double q,
                              double gainDb, double freqHz)
    {
        const double gainLinear = juce::Decibels::decibelsToGain(
            static_cast<float>(gainDb), -60.0f);
        const double safeQ = juce::jmax(0.01, q);
        const double A = std::sqrt(juce::jmax(1e-6, gainLinear));

        double wc = 2.0 * juce::MathConstants<double>::pi * cutoff;
        double R2 = 1.0 / safeQ;

        if (type == Type::Bell)
        {
            R2 = 1.0 / (safeQ * A);
        }
        else if (type == Type::LowShelf)
        {
            wc /= std::sqrt(A);
            R2 = 1.0 / safeQ;
        }
        else if (type == Type::HighShelf)
        {
            wc *= std::sqrt(A);
            R2 = 1.0 / safeQ;
        }

        const double w  = 2.0 * juce::MathConstants<double>::pi * freqHz;

        const double denomRe = wc * wc - w * w;
        const double denomIm = R2 * wc * w;
        const double denomSq = denomRe * denomRe + denomIm * denomIm;
        if (denomSq < 1e-24)
            return -200.0;

        // LP = wc² / D，HP = -w² / D，BP = j·wc·w / D
        const double lpRe =  wc * wc * denomRe / denomSq;
        const double lpIm = -wc * wc * denomIm / denomSq;
        const double hpRe = -w * w * denomRe / denomSq;
        const double hpIm =  w * w * denomIm / denomSq;
        const double bpRe =  wc * w * denomIm / denomSq;
        const double bpIm =  wc * w * denomRe / denomSq;

        double re = 0.0, im = 0.0;
        switch (type)
        {
            case Type::LowPass:   re = lpRe; im = lpIm; break;
            case Type::HighPass:  re = hpRe; im = hpIm; break;
            case Type::BandPass:  re = bpRe; im = bpIm; break;
            case Type::Bell:
                re = 1.0 + (gainLinear - 1.0) * bpRe * R2;
                im =        (gainLinear - 1.0) * bpIm * R2;
                break;
            case Type::LowShelf:
                re = 1.0 + R2 * (A - 1.0) * bpRe + (gainLinear - 1.0) * lpRe;
                im =        R2 * (A - 1.0) * bpIm + (gainLinear - 1.0) * lpIm;
                break;
            case Type::HighShelf:
                re = 1.0 + R2 * (A - 1.0) * bpRe + (gainLinear - 1.0) * hpRe;
                im =        R2 * (A - 1.0) * bpIm + (gainLinear - 1.0) * hpIm;
                break;
        }

        const double mag = std::sqrt(re * re + im * im);
        return 20.0 * std::log10(std::max(mag, 1e-10));
    }

    // 实例便捷方法：用当前参数求幅度响应（dB）
    double getMagnitudeAtFrequency(double freqHz) const
    {
        return magnitudeDb(type, cutoff, q,
                           20.0 * std::log10(std::max(gainLinear, 1e-9)),
                           freqHz);
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
