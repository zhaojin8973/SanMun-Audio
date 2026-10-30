#pragma once

#include "dsp/ReverbProcessor.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

// 测量只描述信号，不替代参考录音与人工听审。
namespace hall_measurement
{
using Audio = juce::AudioBuffer<float>;
using Params = dsp_engines::ReverbParameters;

inline Params baseline()
{
    Params p;
    p.category = 2;
    p.mode = 0;
    p.mix = 100.0f;
    p.predelay = 0.0f;
    p.modDepth = 0.0f;
    return p;
}

inline Audio impulse(double sr, double seconds, double amplitude = 0.1,
                     double onsetSeconds = 0.0, int channels = 2)
{
    Audio a(channels, static_cast<int>(std::ceil(sr * seconds)));
    a.clear();
    const int onset = static_cast<int>(std::round(onsetSeconds * sr));
    if (onset >= 0 && onset < a.getNumSamples())
        for (int c = 0; c < channels; ++c)
            a.setSample(c, onset, static_cast<float>(amplitude));
    return a;
}

// 固定 xorshift32 与显式整数映射，避免不同标准库分布实现的差异。
inline Audio noise(double sr, double inputSeconds, double tailSeconds,
                   float amplitude = 0.1f, uint32_t seed = 4242)
{
    Audio a(2, static_cast<int>(std::ceil(sr * (inputSeconds + tailSeconds))));
    a.clear();
    uint32_t state = seed == 0 ? 1 : seed;
    for (int n = 0; n < std::min(a.getNumSamples(), static_cast<int>(sr * inputSeconds)); ++n)
    {
        state ^= state << 13; state ^= state >> 17; state ^= state << 5;
        const float sample = (static_cast<float>(state >> 8) / 8388608.0f - 1.0f) * amplitude;
        a.setSample(0, n, sample);
        a.setSample(1, n, sample);
    }
    return a;
}

inline Audio render(const Audio& input, double sr, int block, const Params& p,
                    int preparedBlock = 0)
{
    ReverbProcessor processor;
    processor.prepare(sr, preparedBlock > 0 ? preparedBlock : block);
    Audio result;
    result.makeCopyOf(input);
    for (int pos = 0; pos < result.getNumSamples(); pos += block)
    {
        const int n = std::min(block, result.getNumSamples() - pos);
        Audio slice(result.getArrayOfWritePointers(), result.getNumChannels(), pos, n);
        processor.process(slice, p);
    }
    return result;
}

inline double energy(const Audio& a, int start = 0, int end = -1)
{
    end = end < 0 ? a.getNumSamples() : std::min(end, a.getNumSamples());
    double sum = 0;
    for (int c = 0; c < a.getNumChannels(); ++c)
        for (int i = std::max(0, start); i < end; ++i)
        { const double x = a.getSample(c, i); sum += x * x; }
    return sum;
}

inline double correlation(const Audio& a, int start, int end)
{
    end = std::min(end, a.getNumSamples());
    if (a.getNumChannels() < 2 || end <= start) return std::numeric_limits<double>::quiet_NaN();
    double sl = 0, sr = 0, ll = 0, rr = 0, lr = 0;
    for (int n = start; n < end; ++n)
    {
        const double l = a.getSample(0, n), r = a.getSample(1, n);
        sl += l; sr += r; ll += l * l; rr += r * r; lr += l * r;
    }
    const double count = end - start;
    ll -= sl * sl / count; rr -= sr * sr / count; lr -= sl * sr / count;
    return ll > 1e-24 && rr > 1e-24 ? lr / std::sqrt(ll * rr)
                                  : std::numeric_limits<double>::quiet_NaN();
}

inline double maxDifference(const Audio& a, const Audio& b)
{
    if (a.getNumChannels() != b.getNumChannels() || a.getNumSamples() != b.getNumSamples())
        return std::numeric_limits<double>::infinity();
    double difference = 0;
    for (int c = 0; c < a.getNumChannels(); ++c)
        for (int n = 0; n < a.getNumSamples(); ++n)
            difference = std::max(difference, std::abs(static_cast<double>(a.getSample(c, n)) - b.getSample(c, n)));
    return difference;
}

inline int firstNonzero(const Audio& a, double threshold = 1e-9)
{
    for (int n = 0; n < a.getNumSamples(); ++n)
        for (int c = 0; c < a.getNumChannels(); ++c)
            if (std::abs(a.getSample(c, n)) > threshold) return n;
    return -1;
}

inline double energyTime(const Audio& a, double sr, double fraction, double endSeconds)
{
    const int end = std::min(a.getNumSamples(), static_cast<int>(endSeconds * sr));
    const double total = energy(a, 0, end);
    if (!(total > 0)) return std::numeric_limits<double>::quiet_NaN();
    double accumulated = 0;
    for (int n = 0; n < end; ++n)
    {
        for (int c = 0; c < a.getNumChannels(); ++c)
        { const double x = a.getSample(c, n); accumulated += x * x; }
        if (accumulated >= total * fraction) return n / sr;
    }
    return (end - 1) / sr;
}

inline double centroid(const Audio& a, double sr, double endSeconds)
{
    const int end = std::min(a.getNumSamples(), static_cast<int>(endSeconds * sr));
    double e = 0, weighted = 0;
    for (int n = 0; n < end; ++n)
        for (int c = 0; c < a.getNumChannels(); ++c)
        { const double x = a.getSample(c, n), square = x * x; e += square; weighted += square * n / sr; }
    return e > 0 ? weighted / e : std::numeric_limits<double>::quiet_NaN();
}

// 归一化回声密度：窗口内超过自身 RMS 的样本比例 / 高斯参考值。
// 静音返回 NaN；它是扩散代理量，单独不能证明听感平滑。
inline double echoDensity(const Audio& a, int start, int end)
{
    end = std::min(end, a.getNumSamples());
    const int count = (end - start) * a.getNumChannels();
    if (count <= 0) return std::numeric_limits<double>::quiet_NaN();
    const double rms = std::sqrt(energy(a, start, end) / count);
    if (rms <= 1e-12) return std::numeric_limits<double>::quiet_NaN();
    int over = 0;
    for (int n = start; n < end; ++n)
        for (int c = 0; c < a.getNumChannels(); ++c)
            if (std::abs(a.getSample(c, n)) > rms) ++over;
    return over / (count * 0.31731050786291415);
}

struct DecayFit
{
    double rt60 = std::numeric_limits<double>::quiet_NaN();
    double rSquared = std::numeric_limits<double>::quiet_NaN();
    bool valid = false;
};

inline std::vector<double> backwardEnergy(const Audio& a)
{
    std::vector<double> edc(static_cast<size_t>(a.getNumSamples()) + 1, 0.0);
    for (int n = a.getNumSamples() - 1; n >= 0; --n)
    {
        edc[static_cast<size_t>(n)] = edc[static_cast<size_t>(n + 1)];
        for (int c = 0; c < a.getNumChannels(); ++c)
        { const double x = a.getSample(c, n); edc[static_cast<size_t>(n)] += x * x; }
    }
    return edc;
}

// 仅当余量达到 -45 dB 且 -5 至 -25 dB 拟合线性度足够时报告 T20 外推值。
// 只适用于脉冲响应；音乐/噪声输出不得称为 RT60。
inline DecayFit fitT20(const Audio& a, double sr)
{
    DecayFit fit;
    const auto edc = backwardEnergy(a);
    if (!(edc[0] > 1e-24)) return fit;
    double sx = 0, sy = 0, sxx = 0, sxy = 0, syy = 0;
    int count = 0;
    const int stride = std::max(1, static_cast<int>(sr * 0.001));
    for (int n = 0; n < a.getNumSamples(); n += stride)
    {
        const double db = 10.0 * std::log10(std::max(1e-30, edc[static_cast<size_t>(n)] / edc[0]));
        if (db > -5.0 || db < -25.0) continue;
        const double t = n / sr;
        sx += t; sy += db; sxx += t * t; sxy += t * db; syy += db * db; ++count;
    }
    if (count < 20) return fit;
    const double xx = sxx - sx * sx / count, yy = syy - sy * sy / count;
    const double xy = sxy - sx * sy / count;
    if (xx <= 0 || yy <= 0 || xy >= 0) return fit;
    fit.rt60 = -60.0 / (xy / xx);
    fit.rSquared = xy * xy / (xx * yy);
    const int reserve = std::max(0, a.getNumSamples() - static_cast<int>(0.1 * sr));
    const double endRatio = edc[static_cast<size_t>(reserve)] / edc[0];
    fit.valid = fit.rSquared >= 0.95 && endRatio < std::pow(10.0, -4.5);
    return fit;
}
}
