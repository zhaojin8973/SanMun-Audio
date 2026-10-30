// ==============================================================================
// DiffusedEngine.h - 水洗空间混响延迟引擎（Valhalla 风格）
//
// 特性：
//   1. 直接复用 SanMu Reverb 核心 Schroeder 规范型全通扩散原语（AllpassFilter）
//   2. 延迟反馈环内串联 4 级大时延互质阶数全通扩散器，回声随循环融化为丝滑 Hall 混响尾音
//   3. 扩散深度（Diffusion Amount）支持从 0%（锐利直通回声）至 100%（纯粹水洗混响）平滑混合
//   4. 环内 TPT-SVF 混响阻尼高低切
// ==============================================================================

#pragma once

#include "DelayEngine.h"
#include "../primitives/DelayLine.h"
#include "../primitives/AllpassFilter.h"
#include "../primitives/TptSvf.h"

namespace dsp_engines
{

class DiffusedEngine : public DelayEngine
{
public:
    DiffusedEngine();
    ~DiffusedEngine() override = default;

    void prepare(double sampleRate, int maxBlockSize) override;
    void reset() override;

    void processStereoSample(float inL, float inR,
                             float& outL, float& outR,
                             const EngineProcessContext& ctx) noexcept override;

private:
    double currentSampleRate { 48000.0 };

    dsp_primitives::DelayLine delayLineL;
    dsp_primitives::DelayLine delayLineR;

    // 环内级联 4 级 Schroeder 全通扩散网络 (左右声道采用互质大时延，打造高密度空间弥散)
    dsp_primitives::AllpassFilter apL1;
    dsp_primitives::AllpassFilter apL2;
    dsp_primitives::AllpassFilter apL3;
    dsp_primitives::AllpassFilter apL4;

    dsp_primitives::AllpassFilter apR1;
    dsp_primitives::AllpassFilter apR2;
    dsp_primitives::AllpassFilter apR3;
    dsp_primitives::AllpassFilter apR4;

    dsp_primitives::TptSvf filterLowCutL;
    dsp_primitives::TptSvf filterLowCutR;
    dsp_primitives::TptSvf filterHighCutL;
    dsp_primitives::TptSvf filterHighCutR;

    float lastLowCutHz { -1.0f };
    float lastHighCutHz { -1.0f };
};

} // namespace dsp_engines
