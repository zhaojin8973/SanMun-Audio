// ==============================================================================
// TapeEngine.h - 经典 1970 开盘磁带延迟物理引擎
//
// 特性：
//   1. 环内磁带磁饱和建模（tanh 谐波饱和，越循环越醇厚，天然自限幅）
//   2. 磁头录放 EQ 特性（高频渐进滚降 + 环内 DC 阻断）
//   3. Wow 慢速微漂移与 Flutter 高频走带抖动（作用于 Hermite 读指针）
// ==============================================================================

#pragma once

#include "DelayEngine.h"
#include "../primitives/DelayLine.h"
#include "../primitives/TptSvf.h"

namespace dsp_engines
{

class TapeEngine : public DelayEngine
{
public:
    TapeEngine();
    ~TapeEngine() override = default;

    void prepare(double sampleRate, int maxBlockSize) override;
    void reset() override;

    void processStereoSample(float inL, float inR,
                             float& outL, float& outR,
                             const EngineProcessContext& ctx) noexcept override;

private:
    // 磁带非线性软饱和曲线（磁头磁滞软削波 + 二次谐波偏置）
    static inline float tapeSaturate(float x, float driveGain, float drivePct) noexcept
    {
        // 磁带输入驱动
        float driven = x * driveGain;

        // 磁头偏置产生不对称二次谐波 (随着 Drive 增加而显现温暖胶水感)
        const float bias = 0.15f * (drivePct * 0.01f);
        driven = driven + bias * (driven * driven - 0.06f);

        // 磁粉软饱和压缩 (tanh 软限幅)
        const float sat = std::tanh(driven);

        // 对数能量平滑补偿 (防止高驱动下声音过度发虚或爆音，保留醇厚密实感)
        const float comp = std::sqrt(driveGain * 0.65f + 0.35f);
        return sat / comp;
    }

    double currentSampleRate { 48000.0 };

    dsp_primitives::DelayLine delayLineL;
    dsp_primitives::DelayLine delayLineR;

    dsp_primitives::TptSvf filterLowCutL;
    dsp_primitives::TptSvf filterLowCutR;
    dsp_primitives::TptSvf filterHighCutL;
    dsp_primitives::TptSvf filterHighCutR;

    // 环内 DC 阻断器
    float dcStateL { 0.0f };
    float dcStateR { 0.0f };
    float dcPrevInL { 0.0f };
    float dcPrevInR { 0.0f };

    // Wow / Flutter LFO 相位
    float lfoPhaseWow { 0.0f };
    float lfoPhaseFlutter { 0.0f };

    float lastLowCutHz { -1.0f };
    float lastHighCutHz { -1.0f };
};

} // namespace dsp_engines
