#include "VcaEngine.h"

namespace dsp_engines
{
namespace
{
// 本引擎私有数学与侧链实现，不跨引擎调用。
struct BiquadCoeffs
{
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
    float a1 = 0.0f, a2 = 0.0f;
};

inline BiquadCoeffs makeSidechainHpCoeffs(float hpHz, double sr)
{
    BiquadCoeffs c;
    if (sr <= 0.0) return c;
    const double fc = std::clamp(static_cast<double>(hpHz), 10.0, sr * 0.45);
    const double w0 = 2.0 * juce::MathConstants<double>::pi * fc / sr;
    const double cosw0 = std::cos(w0);
    const double sinw0 = std::sin(w0);
    const double alpha = sinw0 / (2.0 * 0.7071067811865475); // Q = 1/sqrt(2) Butterworth
    const double a0 = 1.0 + alpha;

    c.b0 = static_cast<float>((1.0 + cosw0) * 0.5 / a0);
    c.b1 = static_cast<float>(-(1.0 + cosw0) / a0);
    c.b2 = static_cast<float>((1.0 + cosw0) * 0.5 / a0);
    c.a1 = static_cast<float>(-2.0 * cosw0 / a0);
    c.a2 = static_cast<float>((1.0 - alpha) / a0);
    return c;
}

inline float processSidechainHp(float x, float& s1, float& s2, const BiquadCoeffs& c)
{
    const float y = c.b0 * x + s1;
    s1 = c.b1 * x - c.a1 * y + s2;
    s2 = c.b2 * x - c.a2 * y;
    return y;
}

inline float ampToDb(float amp)
{
    return (amp > 1.0e-7f) ? (20.0f * std::log10(amp)) : -140.0f;
}

inline float dbToGain(float db)
{
    return (db > -140.0f) ? std::pow(10.0f, db * 0.05f) : 0.0f;
}

// 小信号三阶系数接近原式，但高电平始终单调；这是经验软饱和，不代表芯片模型。
float outputColour(float x, float gr)
{
    const double shape = 0.057 + 0.008 * std::clamp(double(gr) / 10.0, 0.0, 1.0);
    return static_cast<float>(double(x) / std::sqrt(1.0 + shape * double(x) * x));
}

}


VcaEngine::VcaEngine()
{
    reset();
}

void VcaEngine::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate > 0.0 ? spec.sampleRate : 48000.0;
    reset();
}

void VcaEngine::reset()
{
    scHpS1.fill(0.0f);
    scHpS2.fill(0.0f);
    vcaRmsState.fill(0.0f);
    vcaFastEnvDb.fill(0.0f);
    vcaSlowEnvDb.fill(0.0f);
    currentGrDb.store(0.0f, std::memory_order_relaxed);
}

float VcaEngine::getCurrentGrDb() const noexcept
{
    return currentGrDb.load(std::memory_order_relaxed);
}

float VcaEngine::computeVcaGrDb(float levelDb, float threshDb, float ratioVal) noexcept
{
    constexpr float kOverEasyWidth = 4.0f; // 4dB OverEasy 渐进软拐点
    const float halfKnee = kOverEasyWidth * 0.5f;
    const float over = levelDb - threshDb;

    if (over <= -halfKnee)
        return 0.0f;

    const float r = std::max(ratioVal, 1.0f);
    const float slope = 1.0f - 1.0f / r;

    if (over >= halfKnee)
        return slope * over;

    // 经典二次方渐进软拐点过渡
    const float t = over + halfKnee;
    return slope * (t * t) / (2.0f * kOverEasyWidth);
}

void VcaEngine::process(juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = std::min(buffer.getNumChannels(), 2);
    if (numSamples <= 0 || numChannels <= 0)
        return;

    const double sr = sampleRate > 0.0 ? sampleRate : 48000.0;
    const float rawThresh = thresholdDb.load(std::memory_order_relaxed);
    const float r         = ratio.load(std::memory_order_relaxed);
    const float makeup    = makeupDb.load(std::memory_order_relaxed);
    const float trimLin   = dbToGain(outputTrimDb.load(std::memory_order_relaxed));
    const float mixWet    = mixPercent.load(std::memory_order_relaxed) * 0.01f;
    const float mixDry    = 1.0f - mixWet;
    const float link      = stereoLink.load(std::memory_order_relaxed) * 0.01f;
    const float scHp      = sidechainHpHz.load(std::memory_order_relaxed);
    const bool  scOn      = sidechainEnabled.load(std::memory_order_relaxed) || (scHp > 10.0f);

    // 经典总线压缩比率-阈值微联动补偿（2:1 温和母带，4:1 经典总线，10:1 极限击打）
    float ratioOffsetDb = 0.0f;
    if (r <= 2.5f)
        ratioOffsetDb = 1.5f * (2.5f - r);
    else if (r >= 8.0f)
        ratioOffsetDb = -1.5f * std::min((r - 8.0f) / 12.0f, 1.0f);
    const float thresh = rawThresh + ratioOffsetDb;

    // 1. RMS 检波时间常数（10ms 能量窗口）
    const float rmsCoeff = 1.0f - std::exp(-1.0f / (0.010f * static_cast<float>(sr)));

    const float attSec = std::max(attackMs.load(std::memory_order_relaxed), 0.01f) * 0.001f;
    const float relVal = releaseMs.load(std::memory_order_relaxed);
    const bool  isAutoRel = (relVal >= 1200.0f);

    // 2. Attack 与 Release 弹道常数
    const float fastAttCoeff = 1.0f - std::exp(-1.0f / (attSec * static_cast<float>(sr)));
    // SSL 82E27 电路真实物理：慢速大电容 (6.8uF 对比 0.47uF) 充电速度慢 14.5 倍，仅在持续长音时充满
    const float slowAttCoeff = isAutoRel ? (1.0f - std::exp(-1.0f / (attSec * 14.5f * static_cast<float>(sr)))) : fastAttCoeff;
    
    // Auto 双时间常数模式 vs 手动单时间常数模式：
    const float fastRelSec = isAutoRel ? 0.080f : std::max(relVal * 0.001f, 0.001f);
    const float slowRelSec = isAutoRel ? 1.200f : std::max(relVal * 0.001f, 0.001f);

    const float fastRelCoeff = 1.0f - std::exp(-1.0f / (fastRelSec * static_cast<float>(sr)));
    const float slowRelCoeff = 1.0f - std::exp(-1.0f / (slowRelSec * static_cast<float>(sr)));

    const float makeupLin = dbToGain(makeup);
    const BiquadCoeffs scHpCoeffs = scOn ? makeSidechainHpCoeffs(scHp, sr) : BiquadCoeffs{};

    auto* chL = buffer.getWritePointer(0);
    auto* chR = (numChannels > 1) ? buffer.getWritePointer(1) : nullptr;

    for (int n = 0; n < numSamples; ++n)
    {
        const float xL = chL[n];
        const float xR = chR ? chR[n] : xL;

        float scL = scOn ? processSidechainHp(xL, scHpS1[0], scHpS2[0], scHpCoeffs) : xL;
        float scR = (scOn && chR) ? processSidechainHp(xR, scHpS1[1], scHpS2[1], scHpCoeffs) : (scOn ? scL : xR);

        // 1. 真有效值（True RMS）能量积分更新
        vcaRmsState[0] += rmsCoeff * (scL * scL - vcaRmsState[0]);
        vcaRmsState[1] += rmsCoeff * (scR * scR - vcaRmsState[1]);

        float lvlL = ampToDb(std::sqrt(std::max(vcaRmsState[0], 1e-12f)));
        float lvlR = ampToDb(std::sqrt(std::max(vcaRmsState[1], 1e-12f)));

        // 2. Oxford / Turbo 纯正立体声侧链检波（锁定立体声声场）
        if (chR && link > 0.0f)
        {
            const float linkedRms = std::sqrt(0.5f * (vcaRmsState[0] + vcaRmsState[1]));
            const float linkedLvl = ampToDb(std::max(linkedRms, 1e-12f));
            lvlL += (linkedLvl - lvlL) * link;
            lvlR += (linkedLvl - lvlR) * link;
        }

        // 3. 4dB OverEasy 目标 GR 计算
        const float targetGrL = computeVcaGrDb(lvlL, thresh, r);
        const float targetGrR = computeVcaGrDb(lvlR, thresh, r);

        for (int c = 0; c < numChannels; ++c)
        {
            const float target = (c == 0) ? targetGrL : targetGrR;
            float& fastEnv = vcaFastEnvDb[static_cast<size_t>(c)];
            float& slowEnv = vcaSlowEnvDb[static_cast<size_t>(c)];

            if (target > fastEnv)
                fastEnv += fastAttCoeff * (target - fastEnv);
            else
                fastEnv += fastRelCoeff * (target - fastEnv);

            if (isAutoRel)
            {
                if (target > slowEnv)
                    slowEnv += slowAttCoeff * (target - slowEnv);
                else
                    slowEnv += slowRelCoeff * (target - slowEnv);
            }
            else
            {
                slowEnv = fastEnv;
            }
        }

        // 4. 双时间常数真物理峰值合成（Auto 模式下瞬态快弹与持续稳准自然交接）
        const float grL = std::max(vcaFastEnvDb[0], vcaSlowEnvDb[0]);
        const float grR = chR ? std::max(vcaFastEnvDb[1], vcaSlowEnvDb[1]) : grL;

        const float linL = dbToGain(-grL);
        const float linR = dbToGain(-grR);

        // 输出补偿不再驱动非线性，避免调音量同时改变染色与压缩判断。
        const float ySatL = outputColour(xL * linL, grL) * makeupLin;
        const float yL = (ySatL * mixWet + xL * mixDry) * trimLin;
        chL[n] = yL;

        if (chR)
        {
            const float ySatR = outputColour(xR * linR, grR) * makeupLin;
            const float yR = (ySatR * mixWet + xR * mixDry) * trimLin;
            chR[n] = yR;
        }
    }

    const float maxGrL = std::max(vcaFastEnvDb[0], vcaSlowEnvDb[0]);
    const float maxGrR = chR ? std::max(vcaFastEnvDb[1], vcaSlowEnvDb[1]) : maxGrL;
    currentGrDb.store(std::max(maxGrL, maxGrR), std::memory_order_relaxed);
}

} // namespace dsp_engines
