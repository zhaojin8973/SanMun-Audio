// ==============================================================================
// DelayEngine.h - 4 大专属延迟物理引擎统一基类与参数契约
//
// 铁律：
//   4 大引擎之间 100% 物理独立与代码隔离，无任何共用状态或相互干扰。
// ==============================================================================

#pragma once

#include <juce_core/juce_core.h>

namespace dsp_engines
{

struct EngineProcessContext
{
    float delayTimeSamplesL { 0.0f };
    float delayTimeSamplesR { 0.0f };
    float feedback          { 0.35f }; // 0.0 ~ 0.985
    int   topology          { 0 };     // 0: Stereo, 1: PingPong, 2: Cross
    float dampLowcutHz      { 20.0f };
    float dampHighcutHz     { 14000.0f };
    bool  freeze            { false };
    float modParam          { 0.0f };  // Tape Drive / BBD Warmth / Diffusion Amount
    float wowParam          { 0.0f };  // Tape Wow 深度
};

class DelayEngine
{
public:
    virtual ~DelayEngine() = default;

    virtual void prepare(double sampleRate, int maxBlockSize) = 0;
    virtual void reset() = 0;

    // 核心音频采样级步进处理接口（保证实时零开销与无锁绝对安全）
    virtual void processStereoSample(float inL, float inR,
                                     float& outL, float& outR,
                                     const EngineProcessContext& ctx) noexcept = 0;
};

} // namespace dsp_engines
