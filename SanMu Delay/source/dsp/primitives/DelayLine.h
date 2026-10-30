// ==============================================================================
// DelayLine.h - 工业级高精度分数延迟线原语
//
// 特性：
//   1. 自动向上对齐至 2 的整数次幂（Power of 2），位运算掩码寻址（(idx + 1) & mask）
//   2. 4 点 3 阶 Hermite 分数插值（Catmull-Rom 样条），调制时高频零损失、零拉链杂音
//   3. 线性插值（用于超低 CPU 场景）与整数快速寻址
//   4. 强制 Anti-Denormal 防次正常数钳位（< 1e-15f 归零）
//   5. 实时音频线程绝对安全（Zero-Allocation, Zero-Lock）
// ==============================================================================

#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include <cmath>
#include <algorithm>

namespace dsp_primitives
{

class DelayLine
{
public:
    DelayLine() = default;

    void prepare(int maxDelaySamples)
    {
        int size = 16;
        while (size < maxDelaySamples + 16)
            size <<= 1;

        buffer.assign(static_cast<size_t>(size), 0.0f);
        mask = size - 1;
        writeIndex = 0;
    }

    void reset() noexcept
    {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        writeIndex = 0;
    }

    inline void write(float sample) noexcept
    {
        // 防次正常数（Anti-Denormal）
        if (std::abs(sample) < 1.0e-15f)
            sample = 0.0f;

        buffer[static_cast<size_t>(writeIndex)] = sample;
        writeIndex = (writeIndex + 1) & mask;
    }

    // ---- 整数无插值读取 ----
    [[nodiscard]] inline float readInt(int delaySamples) const noexcept
    {
        const int readIdx = (writeIndex - 1 - delaySamples) & mask;
        return buffer[static_cast<size_t>(readIdx)];
    }

    // ---- 线性插值读取 ----
    [[nodiscard]] inline float readLinear(float delaySamples) const noexcept
    {
        const int dInt = static_cast<int>(delaySamples);
        const float frac = delaySamples - static_cast<float>(dInt);

        const int idx0 = (writeIndex - 1 - dInt) & mask;
        const int idx1 = (idx0 - 1) & mask;

        const float y0 = buffer[static_cast<size_t>(idx0)];
        const float y1 = buffer[static_cast<size_t>(idx1)];

        return y0 + frac * (y1 - y0);
    }

    // ---- 4 点 3 阶 Hermite 插值读取（保真度与音质最优） ----
    [[nodiscard]] inline float readHermite(float delaySamples) const noexcept
    {
        const int dInt = static_cast<int>(delaySamples);
        const float frac = delaySamples - static_cast<float>(dInt);

        const int i1 = (writeIndex - 1 - dInt) & mask;
        const int i0 = (i1 + 1) & mask;
        const int i2 = (i1 - 1) & mask;
        const int i3 = (i1 - 2) & mask;

        const float ym1 = buffer[static_cast<size_t>(i0)];
        const float y0  = buffer[static_cast<size_t>(i1)];
        const float y1  = buffer[static_cast<size_t>(i2)];
        const float y2  = buffer[static_cast<size_t>(i3)];

        // 4-point, 3rd-order Hermite spline
        const float c0 = y0;
        const float c1 = 0.5f * (y1 - ym1);
        const float c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
        const float c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);

        return ((c3 * frac + c2) * frac + c1) * frac + c0;
    }

    [[nodiscard]] int getMask() const noexcept { return mask; }

private:
    std::vector<float> buffer;
    int mask = 0;
    int writeIndex = 0;
};

} // namespace dsp_primitives
