// ==============================================================================
// ModernEngine.cpp - 现代超清高保真延迟引擎实现
// ==============================================================================

#include "ModernEngine.h"
#include <cmath>

namespace dsp_engines
{

ModernEngine::ModernEngine()
{
    filterLowCutL.setType(dsp_primitives::TptSvf::Type::HighPass);
    filterLowCutR.setType(dsp_primitives::TptSvf::Type::HighPass);
    filterHighCutL.setType(dsp_primitives::TptSvf::Type::LowPass);
    filterHighCutR.setType(dsp_primitives::TptSvf::Type::LowPass);
}

void ModernEngine::prepare(double sampleRate, int /*maxBlockSize*/)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 48000.0;

    // 延迟线最大容量支持 5.0 秒 (含足够插值余量，保障全音符与慢歌大延迟)
    const int maxDelaySamples = static_cast<int>(currentSampleRate * 5.0) + 64;
    delayLineL.prepare(maxDelaySamples);
    delayLineR.prepare(maxDelaySamples);

    filterLowCutL.setSampleRate(currentSampleRate);
    filterLowCutR.setSampleRate(currentSampleRate);
    filterHighCutL.setSampleRate(currentSampleRate);
    filterHighCutR.setSampleRate(currentSampleRate);

    reset();
}

void ModernEngine::reset()
{
    delayLineL.reset();
    delayLineR.reset();

    filterLowCutL.reset();
    filterLowCutR.reset();
    filterHighCutL.reset();
    filterHighCutR.reset();

    lastLowCutHz = -1.0f;
    lastHighCutHz = -1.0f;
}

void ModernEngine::processStereoSample(float inL, float inR,
                                      float& outL, float& outR,
                                      const EngineProcessContext& ctx) noexcept
{
    // 更新阻尼滤波器参数
    if (std::abs(ctx.dampLowcutHz - lastLowCutHz) > 0.1f)
    {
        lastLowCutHz = ctx.dampLowcutHz;
        filterLowCutL.setCutoff(lastLowCutHz);
        filterLowCutR.setCutoff(lastLowCutHz);
    }

    if (std::abs(ctx.dampHighcutHz - lastHighCutHz) > 0.1f)
    {
        lastHighCutHz = ctx.dampHighcutHz;
        filterHighCutL.setCutoff(lastHighCutHz);
        filterHighCutR.setCutoff(lastHighCutHz);
    }

    // 1. 从延迟线读取（4点 Hermite 样条插值）
    const float delayedL = delayLineL.readHermite(ctx.delayTimeSamplesL);
    const float delayedR = delayLineR.readHermite(ctx.delayTimeSamplesR);

    outL = delayedL;
    outR = delayedR;

    // 2. 环内阻尼滤波（低切防低频堆积 + 高切自然衰减）
    float fbL = filterLowCutL.processSample(delayedL);
    fbL = filterHighCutL.processSample(fbL);

    float fbR = filterLowCutR.processSample(delayedR);
    fbR = filterHighCutR.processSample(fbR);

    // 3. 反馈路由矩阵 (Stereo / Ping-Pong / Cross)
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

    // 4. 反馈增益与 Freeze 处理
    if (ctx.freeze)
    {
        // 冻结模式：截断新输入，环内无限循环并进行安全软限幅防发散
        fbL = std::tanh(inFeedL * 0.999f);
        fbR = std::tanh(inFeedR * 0.999f);
        delayLineL.write(fbL);
        delayLineR.write(fbR);
    }
    else
    {
        const float nextWriteL = actualInL + inFeedL * ctx.feedback;
        const float nextWriteR = actualInR + inFeedR * ctx.feedback;

        delayLineL.write(nextWriteL);
        delayLineR.write(nextWriteR);
    }
}

} // namespace dsp_engines
