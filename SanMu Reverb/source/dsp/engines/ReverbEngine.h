// ==============================================================================
// ReverbEngine.h - 4 大专属混响引擎统一纯虚基类与参数契约
// ==============================================================================

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace dsp_engines
{

struct ReverbParameters
{
    int category = 0;   // 0: Plate, 1: Room, 2: Hall, 3: Chamber
    int mode = 0;       // 0, 1, 2 (各分类对应的 3 种专属算法)

    float decay = 2.5f;        // RT60 (s)
    float predelay = 10.0f;    // 初始延迟 (ms)
    float mix = 100.0f;        // 干湿比 (%)

    float highFreq = 6000.0f;  // 高频阻尼分频点 (Hz)
    float highShelf = -12.0f;  // 高频衰减深度 (dB)
    float bassFreq = 300.0f;   // 低频分频点 (Hz)
    float bassMult = 1.0f;     // 低频衰减倍率 (x)

    float size = 100.0f;       // 空间尺寸 (%)
    float attack = 0.0f;       // 起音上升速度 (%)

    float earlyDiff = 100.0f;  // 早期扩散 (%)
    float lateDiff = 100.0f;   // 晚期扩散 (%)

    float modRate = 0.5f;      // 调制速率 (Hz)
    float modDepth = 20.0f;    // 调制深度 (%)

    float lowCut = 20.0f;      // 低切滤波 (Hz)
    float highCut = 10000.0f;  // 高切滤波 (Hz)
};

class ReverbEngine
{
public:
    virtual ~ReverbEngine() = default;

    virtual void prepare(double sampleRate, int maxBlockSize) = 0;
    virtual void reset() = 0;
    virtual void process(const float* inL, const float* inR,
                         float* outL, float* outR,
                         int numSamples,
                         const ReverbParameters& params) = 0;
};

} // namespace dsp_engines
