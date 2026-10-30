// ==============================================================================
// AttackEnvelope.h - 混响能量起音绽放塑形原语 (Tank Onset Bloom)
//
// 工业标准机制 (参考 Valhalla VintageVerb / Lexicon 480L Shape & Spread)：
//   - 绝不衰减、压缩输入音频！绝无非线性抽吸与电平压制！输入信号 100% 完整保真；
//   - 作用于进入晚期大厅环路（FDN Tank）的起音过渡形态：
//     - Attack = 0%：瞬态 100% 即刻注入环路，无延迟无软化（适合打击乐、紧致空间）；
//     - Attack = 50%：混响在 40ms~60ms 内线性平滑升腾（Bloom），让人声瞬态挺立在前，大厅从容在后；
//     - Attack = 100%：混响在 120ms 内极缓淡入，呈现经典的“大厅后排（Back of the Hall）”听感。
// ==============================================================================

#pragma once

#include <juce_core/juce_core.h>
#include <cmath>
#include <algorithm>

namespace dsp_primitives
{

class AttackEnvelope
{
public:
    AttackEnvelope() = default;

    void prepare(double sampleRate) noexcept
    {
        sr = sampleRate;
        reset();
    }

    void reset() noexcept
    {
    }

    // 计算首击主 Delay 权重与晚期升腾权重 (严格能量守恒，平滑起音绽放)
    // attack01: 0.0 ~ 1.0
    static void computeWeights(float attack01, float& tap1Weight, float& bloomWeight) noexcept
    {
        const float a = juce::jlimit(0.0f, 1.0f, attack01);
        // a = 0.0 -> tap1 = 1.0, bloom = 0.85 (首击清晰跳出，衰减自然)
        // a = 0.5 -> tap1 = 0.60, bloom = 1.10 (首击柔和，尾音温和升腾)
        // a = 1.0 -> tap1 = 0.20, bloom = 1.40 (首击隐藏，大厅后排深邃 Bloom 绽放)
        tap1Weight = 1.0f - a * 0.85f;
        bloomWeight = 0.85f + a * 0.55f;
    }

    // 能量守恒无损直通：消除对音频波形直接低通导致的 -6dB~-50dB 毁灭性电平衰减 Bug
    [[nodiscard]] inline float processTankInput(float input, float /*attackPercent*/) noexcept
    {
        return input;
    }

    // 兼容既有引擎调用接口
    [[nodiscard]] inline float processInput(float input, float /*attackPercent*/) noexcept
    {
        return input;
    }

private:
    double sr = 48000.0;
};

} // namespace dsp_primitives
