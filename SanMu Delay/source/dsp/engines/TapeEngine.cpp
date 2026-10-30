// ==============================================================================
// TapeEngine.cpp - 经典 1970 开盘磁带延迟物理引擎实现
// ==============================================================================

#include "TapeEngine.h"
#include <cmath>

namespace dsp_engines
{

TapeEngine::TapeEngine()
{
    filterLowCutL.setType(dsp_primitives::TptSvf::Type::HighPass);
    filterLowCutR.setType(dsp_primitives::TptSvf::Type::HighPass);
    filterHighCutL.setType(dsp_primitives::TptSvf::Type::LowPass);
    filterHighCutR.setType(dsp_primitives::TptSvf::Type::LowPass);
}

void TapeEngine::prepare(double sampleRate, int /*maxBlockSize*/)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 48000.0;

    // 延迟线最大容量支持 5.0 秒 (含足够插值余量)
    const int maxDelaySamples = static_cast<int>(currentSampleRate * 5.0) + 64;
    delayLineL.prepare(maxDelaySamples);
    delayLineR.prepare(maxDelaySamples);

    filterLowCutL.setSampleRate(currentSampleRate);
    filterLowCutR.setSampleRate(currentSampleRate);
    filterHighCutL.setSampleRate(currentSampleRate);
    filterHighCutR.setSampleRate(currentSampleRate);

    reset();
}

void TapeEngine::reset()
{
    delayLineL.reset();
    delayLineR.reset();

    filterLowCutL.reset();
    filterLowCutR.reset();
    filterHighCutL.reset();
    filterHighCutR.reset();

    dcStateL = 0.0f;
    dcStateR = 0.0f;
    dcPrevInL = 0.0f;
    dcPrevInR = 0.0f;

    lfoPhaseWow = 0.0f;
    lfoPhaseFlutter = 0.0f;

    lastLowCutHz = -1.0f;
    lastHighCutHz = -1.0f;
}

void TapeEngine::processStereoSample(float inL, float inR,
                                    float& outL, float& outR,
                                    const EngineProcessContext& ctx) noexcept
{
    // 1. 动态阻尼参数更新（磁带默认高频自然衰减稍快）
    const float effHighCut = std::min(ctx.dampHighcutHz, 12000.0f);
    if (std::abs(ctx.dampLowcutHz - lastLowCutHz) > 0.1f)
    {
        lastLowCutHz = ctx.dampLowcutHz;
        filterLowCutL.setCutoff(lastLowCutHz);
        filterLowCutR.setCutoff(lastLowCutHz);
    }

    if (std::abs(effHighCut - lastHighCutHz) > 0.1f)
    {
        lastHighCutHz = effHighCut;
        filterHighCutL.setCutoff(lastHighCutHz);
        filterHighCutR.setCutoff(lastHighCutHz);
    }

    // 2. Wow & Flutter 物理走带抖动发生器
    // Wow: 0.9 Hz 慢漂移；Flutter: 5.3 Hz 细微抖动
    const float twoPi = 6.28318530717958647692f;
    const float wowInc = twoPi * 0.9f / static_cast<float>(currentSampleRate);
    const float fltInc = twoPi * 5.3f / static_cast<float>(currentSampleRate);

    lfoPhaseWow += wowInc;
    if (lfoPhaseWow > twoPi) lfoPhaseWow -= twoPi;

    lfoPhaseFlutter += fltInc;
    if (lfoPhaseFlutter > twoPi) lfoPhaseFlutter -= twoPi;

    const float wowMod = std::sin(lfoPhaseWow);
    const float fltMod = std::sin(lfoPhaseFlutter) * 0.35f;

    // Wow 深度按百分比缩放 (0 ~ 100% -> 0 ~ 45 采样偏移，约 1.0ms 物理走带微漂移)
    const float wowDepthSamples = (ctx.wowParam * 0.01f) * 45.0f;
    const float modOffsetL = (wowMod + fltMod) * wowDepthSamples;
    // 右声道轻微相位差 (45度) 营造磁带立体声呼吸感
    const float modOffsetR = (std::sin(lfoPhaseWow + 0.785f) + fltMod) * wowDepthSamples;

    const float actualDelayL = std::max(2.0f, ctx.delayTimeSamplesL + modOffsetL);
    const float actualDelayR = std::max(2.0f, ctx.delayTimeSamplesR + modOffsetR);

    // 3. 从延迟线读取
    const float delayedL = delayLineL.readHermite(actualDelayL);
    const float delayedR = delayLineR.readHermite(actualDelayR);

    outL = delayedL;
    outR = delayedR;

    // 4. 环内磁带 EQ 阻尼滤波
    float fbL = filterLowCutL.processSample(delayedL);
    fbL = filterHighCutL.processSample(fbL);

    float fbR = filterLowCutR.processSample(delayedR);
    fbR = filterHighCutR.processSample(fbR);

    // 5. 环内磁饱和 (Drive: 0% -> 1.0x, 100% -> 8.5x 显著磁饱和驱动)
    const float driveGain = 1.0f + (ctx.modParam * 0.01f) * 7.5f;
    fbL = tapeSaturate(fbL, driveGain, ctx.modParam);
    fbR = tapeSaturate(fbR, driveGain, ctx.modParam);

    // 6. 环内 15Hz DC 阻断器 (y[n] = x[n] - x[n-1] + R*y[n-1])
    constexpr float R_DC = 0.998f;
    const float dcOutL = fbL - dcPrevInL + R_DC * dcStateL;
    dcPrevInL = fbL;
    dcStateL = dcOutL;
    fbL = dcOutL;

    const float dcOutR = fbR - dcPrevInR + R_DC * dcStateR;
    dcPrevInR = fbR;
    dcStateR = dcOutR;
    fbR = dcOutR;

    // 7. 反馈路由矩阵 (Stereo / Ping-Pong / Cross)
    float inFeedL = fbL;
    float inFeedR = fbR;
    float actualInL = inL;
    float actualInR = inR;

    if (ctx.topology == 1) // PingPong
    {
        inFeedL = fbR;
        inFeedR = fbL;
        const float monoIn = 0.5f * (inL + inR);
        actualInL = monoIn;
        actualInR = 0.0f;
    }
    else if (ctx.topology == 2) // Cross
    {
        inFeedL = 0.75f * fbL + 0.25f * fbR;
        inFeedR = 0.25f * fbL + 0.75f * fbR;
    }

    // 8. 反馈写入
    if (ctx.freeze)
    {
        delayLineL.write(std::tanh(inFeedL * 0.999f));
        delayLineR.write(std::tanh(inFeedR * 0.999f));
    }
    else
    {
        delayLineL.write(actualInL + inFeedL * ctx.feedback);
        delayLineR.write(actualInR + inFeedR * ctx.feedback);
    }
}

} // namespace dsp_engines
