// ==============================================================================
// DiffusedEngine.cpp - 水洗空间混响延迟引擎实现
// ==============================================================================

#include "DiffusedEngine.h"
#include <cmath>

namespace dsp_engines
{

DiffusedEngine::DiffusedEngine()
{
    filterLowCutL.setType(dsp_primitives::TptSvf::Type::HighPass);
    filterLowCutR.setType(dsp_primitives::TptSvf::Type::HighPass);
    filterHighCutL.setType(dsp_primitives::TptSvf::Type::LowPass);
    filterHighCutR.setType(dsp_primitives::TptSvf::Type::LowPass);
}

void DiffusedEngine::prepare(double sampleRate, int /*maxBlockSize*/)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 48000.0;

    // 延迟线最大容量支持 5.0 秒 (含足够插值余量)
    const int maxDelaySamples = static_cast<int>(currentSampleRate * 5.0) + 64;
    delayLineL.prepare(maxDelaySamples);
    delayLineR.prepare(maxDelaySamples);

    // 缩放全通扩散器的时延到当前采样率（基准为 48kHz 下的大时延互质采样数，覆盖 14ms ~ 58ms）
    const float srScale = static_cast<float>(currentSampleRate / 48000.0);
    const int apMax = static_cast<int>(4000.0f * srScale) + 64;

    apL1.prepare(apMax);
    apL2.prepare(apMax);
    apL3.prepare(apMax);
    apL4.prepare(apMax);

    apR1.prepare(apMax);
    apR2.prepare(apMax);
    apR3.prepare(apMax);
    apR4.prepare(apMax);

    // 左声道互质大时延 (约 14.0ms, 26.0ms, 39.0ms, 53.0ms)
    apL1.setDelay(673.0f * srScale);
    apL2.setDelay(1249.0f * srScale);
    apL3.setDelay(1871.0f * srScale);
    apL4.setDelay(2543.0f * srScale);

    // 右声道微错位时延拓展立体声 (约 16.0ms, 29.1ms, 43.1ms, 58.1ms)
    apR1.setDelay(769.0f * srScale);
    apR2.setDelay(1399.0f * srScale);
    apR3.setDelay(2069.0f * srScale);
    apR4.setDelay(2789.0f * srScale);

    filterLowCutL.setSampleRate(currentSampleRate);
    filterLowCutR.setSampleRate(currentSampleRate);
    filterHighCutL.setSampleRate(currentSampleRate);
    filterHighCutR.setSampleRate(currentSampleRate);

    reset();
}

void DiffusedEngine::reset()
{
    delayLineL.reset();
    delayLineR.reset();

    apL1.reset();
    apL2.reset();
    apL3.reset();
    apL4.reset();

    apR1.reset();
    apR2.reset();
    apR3.reset();
    apR4.reset();

    filterLowCutL.reset();
    filterLowCutR.reset();
    filterHighCutL.reset();
    filterHighCutR.reset();

    lastLowCutHz = -1.0f;
    lastHighCutHz = -1.0f;
}

void DiffusedEngine::processStereoSample(float inL, float inR,
                                        float& outL, float& outR,
                                        const EngineProcessContext& ctx) noexcept
{
    // 1. 阻尼参数更新
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

    // 2. 扩散深度与干湿混合 (ctx.modParam: 0% ~ 100%)
    const float diffAmt = juce::jlimit(0.0f, 1.0f, ctx.modParam * 0.01f);
    // 内部全通扩散反馈增益：随着旋钮拉大，扩散器内部能量回荡加深 (0.35 ~ 0.75)
    const float diffFb = 0.35f + diffAmt * 0.40f;

    apL1.setFeedback(diffFb);
    apL2.setFeedback(diffFb * 0.96f);
    apL3.setFeedback(diffFb * 0.92f);
    apL4.setFeedback(diffFb * 0.88f);

    apR1.setFeedback(diffFb);
    apR2.setFeedback(diffFb * 0.96f);
    apR3.setFeedback(diffFb * 0.92f);
    apR4.setFeedback(diffFb * 0.88f);

    // 3. 从延迟线读取原始回声
    const float delayedL = delayLineL.readHermite(ctx.delayTimeSamplesL);
    const float delayedR = delayLineR.readHermite(ctx.delayTimeSamplesR);

    // 4. 通过 4 阶大时延全通扩散网络抹开为 Hall 混响尾音
    float smearedL = apL1.process(delayedL);
    smearedL = apL2.process(smearedL);
    smearedL = apL3.process(smearedL);
    smearedL = apL4.process(smearedL);

    float smearedR = apR1.process(delayedR);
    smearedR = apR2.process(smearedR);
    smearedR = apR3.process(smearedR);
    smearedR = apR4.process(smearedR);

    // 扩散融合：0% 保持完全清晰的原汁原味 Echo；100% 完全晕染为丝滑混响水洗尾音
    const float diffOutL = (1.0f - diffAmt) * delayedL + diffAmt * smearedL;
    const float diffOutR = (1.0f - diffAmt) * delayedR + diffAmt * smearedR;

    outL = diffOutL;
    outR = diffOutR;

    // 5. 环内阻尼滤波
    float fbL = filterLowCutL.processSample(diffOutL);
    fbL = filterHighCutL.processSample(fbL);

    float fbR = filterLowCutR.processSample(diffOutR);
    fbR = filterHighCutR.processSample(fbR);

    // 6. 反馈路由矩阵 (Stereo / Ping-Pong / Cross)
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

    // 7. 反馈写入
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
