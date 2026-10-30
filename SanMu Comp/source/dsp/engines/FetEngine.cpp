#include "FetEngine.h"

namespace dsp_engines
{
namespace
{
// 独立侧链滤波器；音频路径不经过此滤波器。
struct BiquadCoeffs
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
};
BiquadCoeffs highpass(float hz, double sr)
{
    const double w = 2 * juce::MathConstants<double>::pi * std::clamp(double(hz), 10.0, sr * 0.45) / sr;
    const double cs = std::cos(w), alpha = std::sin(w) / std::sqrt(2.0), a0 = 1 + alpha;
    return {float((1 + cs) / (2 * a0)), float(-(1 + cs) / a0), float((1 + cs) / (2 * a0)),
            float(-2 * cs / a0), float((1 - alpha) / a0)};
}
float filter(float x, float& s1, float& s2, const BiquadCoeffs& c)
{
    const float y = c.b0 * x + s1;
    s1 = c.b1 * x - c.a1 * y + s2;
    s2 = c.b2 * x - c.a2 * y;
    return y;
}
float toGain(float db) { return std::pow(10.0f, db * 0.05f); }
double toDb(float x) { return 20 * std::log10(std::max(double(std::abs(x)), 1.0e-12)); }
float inputDb(float knob)
{
    // 保留既有参数编号与工作点；低端连续衰减，消除原先 0.2 刻度处的跳变。
    return knob <= 0.2f ? -80.0f : knob < 1.0f ? -80.0f + (knob - 0.2f) * 63.0f / 0.8f : knob - 18.0f;
}
float outputDb(float knob)
{
    return knob <= 0.2f ? -80.0f : knob < 1.0f ? -80.0f + (knob - 0.2f) * 57.0f / 0.8f : knob - 24.0f;
}

// g = (base + k * F(level - g)) / (1 + k)。level-g 是当前受控侧链输出，
// F 的斜率为 R-1；解析解消除显式反馈延迟，不串联第二个增益跟随器。
double feedbackReduction(double levelAboveThreshold, double base, double slope, double k, double knee)
{
    const double over = levelAboveThreshold - base;
    const double target = over <= 0 ? 0 : over < knee ? slope * over * over / (2 * knee)
                                                           : slope * (over - knee / 2);
    if (target <= base) return base;
    double g = (base + k * slope * (levelAboveThreshold - knee / 2)) / (1 + k * (1 + slope));
    if (levelAboveThreshold - g < knee)
    {
        const double a = k * slope / (2 * knee), b = 1 + k;
        const double c = b * levelAboveThreshold - base;
        const double v = 2 * c / (b + std::sqrt(b * b + 4 * a * c));
        g = levelAboveThreshold - v;
    }
    return std::clamp(g, base, 60.0);
}
}

FetEngine::FetEngine() { reset(); }
void FetEngine::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate > 0 ? spec.sampleRate : 48000;
    reset();
}
void FetEngine::reset()
{
    scHpS1.fill(0); scHpS2.fill(0); gainReductionDb.fill(0);
    smoothedDriveDb = inputDb(fetDrive.load(std::memory_order_relaxed));
    smoothedMakeupDb = outputDb(makeupDb.load(std::memory_order_relaxed));
    currentGrDb.store(0, std::memory_order_relaxed);
}
float FetEngine::getCurrentGrDb() const noexcept { return currentGrDb.load(std::memory_order_relaxed); }

void FetEngine::process(juce::AudioBuffer<float>& buffer)
{
    const int channels = std::min(2, buffer.getNumChannels());
    if (channels == 0 || buffer.getNumSamples() == 0) return;
    const int ratioIndex = std::clamp(fetRatioIdx.load(std::memory_order_relaxed), 0, 4);
    constexpr double ratios[] = {4, 8, 12, 20, 20};
    constexpr double thresholds[] = {-20, -21, -22, -23.5, -26};
    const double r = ratios[ratioIndex];
    const double threshold = thresholds[ratioIndex] + headroomDb.load(std::memory_order_relaxed) - 16;
    const double attack = 0.0008 * std::pow(0.00002 / 0.0008,
        (std::clamp(attackMs.load(std::memory_order_relaxed), 1.f, 7.f) - 1) / 6);
    const double release = 1.1 * std::pow(0.05 / 1.1,
        (std::clamp(releaseMs.load(std::memory_order_relaxed), 1.f, 7.f) - 1) / 6);
    // 在软拐点之外补偿闭环斜率，令启动旋钮控制整体收敛时间。
    const double charge = std::expm1(1 / (sampleRate * attack)) / r;
    const double recovery = std::exp(-1 / (sampleRate * release));
    // 释放在超阈值检波幅度域进行指数衰减，而不是让 dB 衰减量直接乘系数。
    // g = scale * log(1 + excess)，深压缩时恢复斜率有界，避免压得越深、过零纹波越大。
    // 接近零衰减时仍为原释放时间常数；这是行为模型，不声称器件级复刻。
    const double releaseScale = (20 / std::log(10.0)) * (1 - 1 / r);
    const float parameterCoeff = float(-std::expm1(-1 / (sampleRate * 0.005)));
    const float drive = inputDb(std::clamp(fetDrive.load(std::memory_order_relaxed), 0.f, 48.f));
    const float output = outputDb(std::clamp(makeupDb.load(std::memory_order_relaxed), 0.f, 48.f));
    const float trim = toGain(outputTrimDb.load(std::memory_order_relaxed));
    const float wet = std::clamp(mixPercent.load(std::memory_order_relaxed) * 0.01f, 0.f, 1.f);
    const float link = std::clamp(stereoLink.load(std::memory_order_relaxed) * 0.01f, 0.f, 1.f);
    const float hp = sidechainHpHz.load(std::memory_order_relaxed);
    const bool scOn = sidechainEnabled.load(std::memory_order_relaxed) || hp > 10;
    const auto coefficients = scOn ? highpass(hp, sampleRate) : BiquadCoeffs{};
    auto* left = buffer.getWritePointer(0);
    auto* right = channels == 2 ? buffer.getWritePointer(1) : nullptr;
    for (int n = 0; n < buffer.getNumSamples(); ++n)
    {
        smoothedDriveDb += parameterCoeff * (drive - smoothedDriveDb);
        smoothedMakeupDb += parameterCoeff * (output - smoothedMakeupDb);
        const float inputGain = toGain(smoothedDriveDb), outputGain = toGain(smoothedMakeupDb);
        const float raw[] = {left[n], right ? right[n] : left[n]};
        float detector[2];
        for (size_t c = 0; c < static_cast<size_t>(channels); ++c)
            detector[c] = std::abs(scOn ? filter(raw[c] * inputGain, scHpS1[c], scHpS2[c], coefficients) : raw[c] * inputGain);
        if (right)
        {
            const float peak = std::max(detector[0], detector[1]);
            detector[0] += link * (peak - detector[0]);
            detector[1] += link * (peak - detector[1]);
        }
        for (size_t c = 0; c < static_cast<size_t>(channels); ++c)
        {
            const double base = gainReductionDb[c] + releaseScale * std::log(
                recovery + (1 - recovery) * std::exp(-gainReductionDb[c] / releaseScale));
            gainReductionDb[c] = feedbackReduction(toDb(detector[c]) - threshold, base, r - 1, charge,
                                                   ratioIndex == 4 ? 4.0 : 1.0);
            const float processed = raw[c] * inputGain * toGain(float(-gainReductionDb[c])) * outputGain;
            (c == 0 ? left : right)[n] = (processed * wet + raw[c] * (1 - wet)) * trim;
        }
    }
    currentGrDb.store(float(channels == 2 ? std::max(gainReductionDb[0], gainReductionDb[1]) : gainReductionDb[0]),
                      std::memory_order_relaxed);
}
}
