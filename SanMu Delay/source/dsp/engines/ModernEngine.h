// ==============================================================================
// ModernEngine.h - 现代超清高保真延迟引擎
//
// 特性：
//   1. 32 位浮点纯净延迟线，零染色、零失真
//   2. 4 点 3 阶 Hermite 分数插值
//   3. 环内双精度 TPT-SVF 线性阻尼高低切
//   4. 专有 Freeze 软限幅安全保护
// ==============================================================================

#pragma once

#include "DelayEngine.h"
#include "../primitives/DelayLine.h"
#include "../primitives/TptSvf.h"

namespace dsp_engines
{

class ModernEngine : public DelayEngine
{
public:
    ModernEngine();
    ~ModernEngine() override = default;

    void prepare(double sampleRate, int maxBlockSize) override;
    void reset() override;

    void processStereoSample(float inL, float inR,
                             float& outL, float& outR,
                             const EngineProcessContext& ctx) noexcept override;

private:
    double currentSampleRate { 48000.0 };

    dsp_primitives::DelayLine delayLineL;
    dsp_primitives::DelayLine delayLineR;

    dsp_primitives::TptSvf filterLowCutL;
    dsp_primitives::TptSvf filterLowCutR;
    dsp_primitives::TptSvf filterHighCutL;
    dsp_primitives::TptSvf filterHighCutR;

    float lastLowCutHz { -1.0f };
    float lastHighCutHz { -1.0f };
};

} // namespace dsp_engines
