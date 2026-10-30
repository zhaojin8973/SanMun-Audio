// ==============================================================================
// BbdEngine.cpp - 1980 经典模拟桶链（Bucket-Brigade Device）延迟引擎实现
// ==============================================================================

#include "BbdEngine.h"
#include <cmath>

namespace dsp_engines
{

BbdEngine::BbdEngine()
{
    filterClockL.setType(dsp_primitives::TptSvf::Type::LowPass);
    filterClockR.setType(dsp_primitives::TptSvf::Type::LowPass);
    filterLowCutL.setType(dsp_primitives::TptSvf::Type::HighPass);
    filterLowCutR.setType(dsp_primitives::TptSvf::Type::HighPass);
}

void BbdEngine::prepare(double sampleRate, int /*maxBlockSize*/)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 48000.0;

    // 延迟线最大容量支持 5.0 秒 (含足够插值余量)
    const int maxDelaySamples = static_cast<int>(currentSampleRate * 5.0) + 64;
    delayLineL.prepare(maxDelaySamples);
    delayLineR.prepare(maxDelaySamples);

    filterClockL.setSampleRate(currentSampleRate);
    filterClockR.setSampleRate(currentSampleRate);
    filterLowCutL.setSampleRate(currentSampleRate);
    filterLowCutR.setSampleRate(currentSampleRate);

    reset();
}

void BbdEngine::reset()
{
    delayLineL.reset();
    delayLineR.reset();

    filterClockL.reset();
    filterClockR.reset();
    filterLowCutL.reset();
    filterLowCutR.reset();

    lastClockCutHz = -1.0f;
    lastLowCutHz = -1.0f;
}

void BbdEngine::processStereoSample(float inL, float inR,
                                   float& outL, float& outR,
                                   const EngineProcessContext& ctx) noexcept
{
    // 1. 物理时钟相关滤波模拟：
    // BBD 芯片在短延迟（如 50ms）时拥有宽频（~8kHz），长延迟（如 800ms）时截止频率降至 2.5k~3kHz
    const float delaySecL = ctx.delayTimeSamplesL / static_cast<float>(currentSampleRate);
    const float delaySecR = ctx.delayTimeSamplesR / static_cast<float>(currentSampleRate);
    const float avgDelaySec = 0.5f * (delaySecL + delaySecR);

    // 物理映射公式：f_c = clamp(2000.0f, 9000.0f, 6000.0f / (1.0f + 2.0f * avgDelaySec))
    float dynamicClockCut = 6500.0f / (1.0f + 1.8f * avgDelaySec);
    // 用户高切阻尼与之叠加取较小值
    dynamicClockCut = std::min(dynamicClockCut, ctx.dampHighcutHz);
    dynamicClockCut = std::max(1200.0f, dynamicClockCut);

    if (std::abs(dynamicClockCut - lastClockCutHz) > 0.5f)
    {
        lastClockCutHz = dynamicClockCut;
        filterClockL.setCutoff(lastClockCutHz);
        filterClockR.setCutoff(lastClockCutHz);
    }

    if (std::abs(ctx.dampLowcutHz - lastLowCutHz) > 0.1f)
    {
        lastLowCutHz = ctx.dampLowcutHz;
        filterLowCutL.setCutoff(lastLowCutHz);
        filterLowCutR.setCutoff(lastLowCutHz);
    }

    // 2. 从延迟线读取
    const float delayedL = delayLineL.readHermite(ctx.delayTimeSamplesL);
    const float delayedR = delayLineR.readHermite(ctx.delayTimeSamplesR);

    outL = delayedL;
    outR = delayedR;

    // 3. 环内时钟低通与低切滤波
    float fbL = filterClockL.processSample(delayedL);
    fbL = filterLowCutL.processSample(fbL);

    float fbR = filterClockR.processSample(delayedR);
    fbR = filterLowCutR.processSample(fbR);

    // 4. BBD 模拟压扩非线性染色 (ctx.modParam: 0 ~ 100% -> warmthAmount: 0.0 ~ 1.0)
    const float warmth = ctx.modParam * 0.01f;
    fbL = bbdSaturate(fbL, warmth);
    fbR = bbdSaturate(fbR, warmth);

    // 5. 反馈路由矩阵 (Stereo / Ping-Pong / Cross)
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

    // 6. 反馈写入
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
