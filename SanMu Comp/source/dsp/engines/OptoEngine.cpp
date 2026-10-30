#include "OptoEngine.h"

namespace dsp_engines
{
namespace
{
// 本引擎独立实现，无跨引擎处理函数或状态。
struct BiquadCoeffs { double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0; };
BiquadCoeffs highpass(double hz, double sr)
{
    const double w = 2 * juce::MathConstants<double>::pi * std::clamp(hz, 10.0, sr * 0.45) / sr;
    const double cs = std::cos(w), alpha = std::sin(w) / std::sqrt(2.0), a0 = 1 + alpha;
    return {(1 + cs) / (2 * a0), -(1 + cs) / a0, (1 + cs) / (2 * a0), -2 * cs / a0, (1 - alpha) / a0};
}
double filter(double x, double& s1, double& s2, const BiquadCoeffs& c)
{
    const double y = c.b0 * x + s1;
    s1 = c.b1 * x - c.a1 * y + s2;
    s2 = c.b2 * x - c.a2 * y;
    return y;
}
double gain(double db) { return std::pow(10.0, db * 0.05); }
double level(double amplitude) { return 20 * std::log10(std::max(amplitude, 1.0e-12)); }
double gainKnobDb(double knob)
{
    knob = std::clamp(knob, 0.0, 100.0);
    // 保留 40 刻度为 0dB 的原工作点，低端连续衔接到 -80dB。
    const double atOne = -60 + 60 * std::pow(1.0 / 40, 0.75);
    if (knob < 1) return -80 + knob * (atOne + 80);
    return knob <= 40 ? -60 + 60 * std::pow(knob / 40, 0.75)
                      : 40 * std::pow((knob - 40) / 60, 1.2);
}
double prDrive(double knob)
{
    const double mapped = -10 + 48 * std::pow(knob / 100, 1.65);
    // 0 表示关闭检波驱动；取消原 0.5 刻度处的不连续跳变。
    return knob < 1 ? -80 + knob * (70 + 48 * std::pow(0.01, 1.65)) : mapped;
}
}

OptoEngine::OptoEngine() { reset(); }
void OptoEngine::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate > 0 ? spec.sampleRate : 48000;
    reset();
}
void OptoEngine::reset()
{
    feedbackSample.fill(0); scHpS1.fill(0); scHpS2.fill(0); detectorPower.fill(0);
    fastReduction.fill(0); slowReduction.fill(0); totalReduction.fill(0);
    dcBlockX1.fill(0); dcBlockY1.fill(0);
    smoothedPr = std::clamp(double(optoPeakReduction.load(std::memory_order_relaxed)), 0.0, 100.0);
    smoothedGainDb = gainKnobDb(makeupDb.load(std::memory_order_relaxed));
    smoothedTrimDb = outputTrimDb.load(std::memory_order_relaxed);
    smoothedMix = std::clamp(double(mixPercent.load(std::memory_order_relaxed)) / 100, 0.0, 1.0);
    smoothedLink = std::clamp(double(stereoLink.load(std::memory_order_relaxed)) / 100, 0.0, 1.0);
    smoothedMode = isLimit.load(std::memory_order_relaxed) ? 1 : 0;
    smoothedScMix = sidechainHpHz.load(std::memory_order_relaxed) > 10 ? 1 : 0;
    currentGrDb.store(0, std::memory_order_relaxed);
}
float OptoEngine::getCurrentGrDb() const noexcept { return currentGrDb.load(std::memory_order_relaxed); }

void OptoEngine::process(juce::AudioBuffer<float>& buffer)
{
    const int channels = std::min(2, buffer.getNumChannels());
    if (channels == 0 || buffer.getNumSamples() == 0) return;
    const auto coefficient = [&](double seconds) { return -std::expm1(-1 / (sampleRate * seconds)); };
    const double smooth = coefficient(0.005), detectorCoeff = coefficient(0.0015);
    const double fastRelease = coefficient(0.085), memoryCharge = coefficient(0.400), memoryRelease = coefficient(1.500);
    const double pr = std::clamp(double(optoPeakReduction.load(std::memory_order_relaxed)), 0.0, 100.0);
    const double output = gainKnobDb(makeupDb.load(std::memory_order_relaxed));
    const double trim = outputTrimDb.load(std::memory_order_relaxed);
    const double mix = std::clamp(double(mixPercent.load(std::memory_order_relaxed)) / 100, 0.0, 1.0);
    const double link = std::clamp(double(stereoLink.load(std::memory_order_relaxed)) / 100, 0.0, 1.0);
    const double mode = isLimit.load(std::memory_order_relaxed) ? 1 : 0;
    const double hp = sidechainHpHz.load(std::memory_order_relaxed);
    const double hpMix = hp > 10 ? 1 : 0;
    const auto hpCoeffs = highpass(hp, sampleRate);
    // 固定 3.8Hz 一阶直流隔离，奈奎斯特增益归一；与输出旋钮无关。
    const double dcPole = std::exp(-2 * juce::MathConstants<double>::pi * 3.8 / sampleRate);
    const double dcScale = (1 + dcPole) * 0.5;
    auto* left = buffer.getWritePointer(0);
    auto* right = channels == 2 ? buffer.getWritePointer(1) : nullptr;
    for (int n = 0; n < buffer.getNumSamples(); ++n)
    {
        smoothedPr += smooth * (pr - smoothedPr);
        smoothedGainDb += smooth * (output - smoothedGainDb);
        smoothedTrimDb += smooth * (trim - smoothedTrimDb);
        smoothedMix += smooth * (mix - smoothedMix);
        smoothedLink += smooth * (link - smoothedLink);
        smoothedMode += smooth * (mode - smoothedMode);
        smoothedScMix += smooth * (hpMix - smoothedScMix);
        const double raw[] = {left[n], right ? right[n] : left[n]};
        double detected[2] {};
        for (size_t c = 0; c < static_cast<size_t>(channels); ++c)
        {
            const double filtered = filter(feedbackSample[c], scHpS1[c], scHpS2[c], hpCoeffs);
            const double sidechain = feedbackSample[c] + smoothedScMix * (filtered - feedbackSample[c]);
            detectorPower[c] += detectorCoeff * (sidechain * sidechain - detectorPower[c]);
            detected[c] = level(std::sqrt(std::max(0.0, detectorPower[c]))) + prDrive(smoothedPr);
        }
        if (right)
        {
            const double maximum = std::max(detected[0], detected[1]);
            detected[0] += smoothedLink * (maximum - detected[0]);
            detected[1] += smoothedLink * (maximum - detected[1]);
        }
        const double slope = 2 + 7 * smoothedMode, knee = 16 - 8 * smoothedMode;
        for (size_t c = 0; c < static_cast<size_t>(channels); ++c)
        {
            const double over = std::max(0.0, detected[c] + 24);
            const double target = std::min(60.0, slope * over * over / (over + knee));
            // 按当前曲线斜率补偿反馈加速，避免 LIMIT 的启动比 COMP 快数倍。
            const double derivative = slope * over * (over + 2 * knee) / ((over + knee) * (over + knee));
            const double attack = coefficient(0.010 * (1 + derivative));
            fastReduction[c] += (target > fastReduction[c] ? attack : fastRelease) * (target - fastReduction[c]);
            // 慢状态只储存持续衰减的一部分；快状态下降后不再充入，防止尾部反弹。
            const double memoryTarget = 0.45 * std::min(target, fastReduction[c]);
            slowReduction[c] += (memoryTarget > slowReduction[c] ? memoryCharge : memoryRelease)
                                * (memoryTarget - slowReduction[c]);
            totalReduction[c] = std::max(fastReduction[c], slowReduction[c]);
            const double attenuated = raw[c] * gain(-totalReduction[c]);
            feedbackSample[c] = attenuated;
            const double blocked = dcScale * (attenuated - dcBlockX1[c]) + dcPole * dcBlockY1[c];
            dcBlockX1[c] = attenuated; dcBlockY1[c] = blocked;
            const double wet = blocked * gain(smoothedGainDb);
            (c == 0 ? left : right)[n] = static_cast<float>((wet * smoothedMix + raw[c] * (1 - smoothedMix)) * gain(smoothedTrimDb));
        }
    }
    currentGrDb.store(static_cast<float>(channels == 2 ? std::max(totalReduction[0], totalReduction[1]) : totalReduction[0]),
                      std::memory_order_relaxed);
}
}
