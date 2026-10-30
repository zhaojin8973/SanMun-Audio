#pragma once
#include "TptSvf.h"

// 固定参数快照的模拟原型幅度；增益和频率预计算，逐频点无需对数或幂函数。
struct LinearMagnitude
{
    void prepare(TptSvf::Type nextType, double frequency, double q, double gainDb)
    {
        type = nextType;
        gain = std::pow(10., gainDb / 20.);
        const double rootGain = std::sqrt(std::max(1e-6, gain));
        double cutoff = frequency, damping = 1. / std::max(.01, q);
        if (type == TptSvf::Type::Bell) damping /= rootGain;
        else if (type == TptSvf::Type::LowShelf) cutoff /= std::sqrt(rootGain);
        else if (type == TptSvf::Type::HighShelf) cutoff *= std::sqrt(rootGain);
        cutoffSquared = cutoff * cutoff;
        dampingSquared = damping * damping * cutoffSquared;
        identity = std::abs(gainDb) < 1e-9 && (type == TptSvf::Type::Bell || type == TptSvf::Type::LowShelf || type == TptSvf::Type::HighShelf);
    }

    double power(double frequencySquared) const noexcept
    {
        if (identity) return 1.;
        const double real = cutoffSquared - frequencySquared;
        const double imaginarySquared = dampingSquared * frequencySquared;
        const double denominator = real * real + imaginarySquared;
        if (denominator < 1e-30) return 1e-20;
        double numerator = denominator;
        switch (type)
        {
            case TptSvf::Type::LowPass: numerator = cutoffSquared * cutoffSquared; break;
            case TptSvf::Type::HighPass: numerator = frequencySquared * frequencySquared; break;
            case TptSvf::Type::BandPass: numerator = cutoffSquared * frequencySquared; break;
            case TptSvf::Type::Bell: numerator = real * real + gain * gain * imaginarySquared; break;
            case TptSvf::Type::LowShelf:
            {
                const double r = gain * cutoffSquared - frequencySquared;
                numerator = r * r + gain * imaginarySquared; break;
            }
            case TptSvf::Type::HighShelf:
            {
                const double r = cutoffSquared - gain * frequencySquared;
                numerator = r * r + gain * imaginarySquared; break;
            }
        }
        return std::max(1e-20, numerator / denominator);
    }
    TptSvf::Type type = TptSvf::Type::Bell;
    double gain = 1, cutoffSquared = 1, dampingSquared = 1;
    bool identity = true;
};
