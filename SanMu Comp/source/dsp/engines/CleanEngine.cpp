#include "CleanEngine.h"

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

}


CleanEngine::CleanEngine()
{
    reset();
}

void CleanEngine::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate > 0.0 ? spec.sampleRate : 48000.0;
    reset();
}

void CleanEngine::reset()
{
    scHpS1.fill(0.0f);
    scHpS2.fill(0.0f);
    cleanEnvDb.fill(0.0f);
    currentGrDb.store(0.0f, std::memory_order_relaxed);
}

float CleanEngine::getCurrentGrDb() const noexcept
{
    return currentGrDb.load(std::memory_order_relaxed);
}

float CleanEngine::computeCleanGrDb(float levelDb, float threshDb, float ratioVal, float kneeWidthDb) noexcept
{
    const float halfKnee = kneeWidthDb * 0.5f;
    const float over = levelDb - threshDb;

    if (over <= -halfKnee)
        return 0.0f;

    const float r = std::max(ratioVal, 1.0f);
    const float slope = 1.0f - 1.0f / r;

    if (over >= halfKnee)
        return slope * over;

    // 二次方平滑软拐点插值
    const float t = over + halfKnee;
    const float k = std::max(kneeWidthDb, 1.0e-3f);
    return slope * (t * t) / (2.0f * k);
}

void CleanEngine::process(juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = std::min(buffer.getNumChannels(), 2);
    if (numSamples <= 0 || numChannels <= 0)
        return;

    const double sr = sampleRate > 0.0 ? sampleRate : 48000.0;
    const float thresh = thresholdDb.load(std::memory_order_relaxed);
    const float r      = ratio.load(std::memory_order_relaxed);
    const float knee   = kneeDb.load(std::memory_order_relaxed);
    const float makeup = makeupDb.load(std::memory_order_relaxed);
    const float trimLin = dbToGain(outputTrimDb.load(std::memory_order_relaxed));
    const float mixWet = mixPercent.load(std::memory_order_relaxed) * 0.01f;
    const float mixDry = 1.0f - mixWet;
    const float link   = stereoLink.load(std::memory_order_relaxed) * 0.01f;
    const float scHp   = sidechainHpHz.load(std::memory_order_relaxed);
    const bool  scOn   = sidechainEnabled.load(std::memory_order_relaxed) || (scHp > 10.0f);

    const float attSec = std::max(attackMs.load(std::memory_order_relaxed), 0.01f) * 0.001f;
    const float relSec = std::max(releaseMs.load(std::memory_order_relaxed), 1.0f) * 0.001f;
    const float attCoeff = 1.0f - std::exp(-1.0f / (attSec * static_cast<float>(sr)));
    const float relCoeff = 1.0f - std::exp(-1.0f / (relSec * static_cast<float>(sr)));

    const float makeupLin = dbToGain(makeup);
    const BiquadCoeffs scHpCoeffs = scOn ? makeSidechainHpCoeffs(scHp, sr) : BiquadCoeffs{};

    auto* chL = buffer.getWritePointer(0);
    auto* chR = (numChannels > 1) ? buffer.getWritePointer(1) : nullptr;

    for (int n = 0; n < numSamples; ++n)
    {
        const float xL = chL[n];
        const float xR = chR ? chR[n] : xL;

        // 1. 侧链交流信号处理
        float scL = scOn ? processSidechainHp(xL, scHpS1[0], scHpS2[0], scHpCoeffs) : xL;
        float scR = (scOn && chR) ? processSidechainHp(xR, scHpS1[1], scHpS2[1], scHpCoeffs) : (scOn ? scL : xR);

        // 2. 检波电平（dBFS）
        float lvlL = ampToDb(std::fabs(scL));
        float lvlR = ampToDb(std::fabs(scR));

        // 3. 立体声联动
        if (chR && link > 0.0f)
        {
            const float linked = std::max(lvlL, lvlR);
            lvlL += (linked - lvlL) * link;
            lvlR += (linked - lvlR) * link;
        }

        // 4. 静态目标 GR 计算
        const float targetGrL = computeCleanGrDb(lvlL, thresh, r, knee);
        const float targetGrR = computeCleanGrDb(lvlR, thresh, r, knee);

        // 5. dB 域球状平滑（无过零点失真）
        for (int c = 0; c < numChannels; ++c)
        {
            const float target = (c == 0) ? targetGrL : targetGrR;
            float& env = cleanEnvDb[static_cast<size_t>(c)];
            const float coeff = (target > env) ? attCoeff : relCoeff;
            env += coeff * (target - env);
        }

        // 6. 应用增益与干湿混合及最终纯净 Output Trim
        const float gainL = dbToGain(-cleanEnvDb[0]) * makeupLin;
        const float gainR = dbToGain(-cleanEnvDb[1]) * makeupLin;

        const float yL = ((xL * gainL) * mixWet + xL * mixDry) * trimLin;
        chL[n] = yL;

        if (chR)
        {
            const float yR = ((xR * gainR) * mixWet + xR * mixDry) * trimLin;
            chR[n] = yR;
        }
    }

    const float maxGr = std::max(cleanEnvDb[0], cleanEnvDb[1]);
    currentGrDb.store(maxGr, std::memory_order_relaxed);
}

} // namespace dsp_engines
