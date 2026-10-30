// ==============================================================================
// BbdEngine.h - 1980 经典模拟桶链（Bucket-Brigade Device）延迟引擎
//
// 特性：
//   1. 物理时钟相关滤波（延迟时间越长，抗混叠低通截止自动随之降低，带来纯正暗暖感）
//   2. 压扩芯片（Compander）非线性温暖染色与不对称软限幅
//   3. 环内阻尼与模拟颗粒感塑造
// ==============================================================================

#pragma once

#include "DelayEngine.h"
#include "../primitives/DelayLine.h"
#include "../primitives/TptSvf.h"

namespace dsp_engines
{

class BbdEngine : public DelayEngine
{
public:
    BbdEngine();
    ~BbdEngine() override = default;

    void prepare(double sampleRate, int maxBlockSize) override;
    void reset() override;

    void processStereoSample(float inL, float inR,
                             float& outL, float& outR,
                             const EngineProcessContext& ctx) noexcept override;

private:
    // BBD 模拟非线性压扩与管芯饱和
    static inline float bbdSaturate(float x, float warmthAmount) noexcept
    {
        // 轻微不对称二次谐波 + 软限幅压扩
        const float drive = 1.0f + warmthAmount * 1.5f;
        const float shaped = x * drive + 0.08f * warmthAmount * (x * x - 0.1f);
        return shaped / (1.0f + std::abs(shaped) * 0.4f);
    }

    double currentSampleRate { 48000.0 };

    dsp_primitives::DelayLine delayLineL;
    dsp_primitives::DelayLine delayLineR;

    dsp_primitives::TptSvf filterClockL;
    dsp_primitives::TptSvf filterClockR;
    dsp_primitives::TptSvf filterLowCutL;
    dsp_primitives::TptSvf filterLowCutR;

    float lastClockCutHz { -1.0f };
    float lastLowCutHz { -1.0f };
};

} // namespace dsp_engines
