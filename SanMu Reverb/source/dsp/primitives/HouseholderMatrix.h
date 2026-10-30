// ==============================================================================
// HouseholderMatrix.h - 严格幺正正交 Householder 矩阵变换原语
//
// 数学公式：A = I - (2 / N) * 1 * 1^T
// 性质：
//   1. 严格正交（A^T * A = I），能量绝对守恒
//   2. 算法复杂度 O(N)，仅需 2N 次浮点运算，比常规矩阵连乘 O(N^2) 快数倍
// ==============================================================================

#pragma once

#include <array>
#include <cstddef>

namespace dsp_primitives
{

template <size_t N>
class HouseholderMatrix
{
public:
    static inline void multiply(std::array<float, N>& x) noexcept
    {
        float sum = 0.0f;
        for (size_t i = 0; i < N; ++i)
            sum += x[i];

        const float factor = (2.0f / static_cast<float>(N)) * sum;

        for (size_t i = 0; i < N; ++i)
            x[i] -= factor;
    }

    // 支持 Late Diff (晚期残响扩散度) 动态调节：
    //   diffAmount = 0.0: 线路完全解耦独立 (无矩阵混合，呈现清晰离散回声)
    //   diffAmount = 1.0: 完整正交能量守恒 Householder 漫反射 (极速平滑雾化)
    static inline void multiplyWithDiffusion(std::array<float, N>& x, float diffAmount) noexcept
    {
        float sum = 0.0f;
        for (size_t i = 0; i < N; ++i)
            sum += x[i];

        const float factor = (2.0f / static_cast<float>(N)) * sum * juce::jlimit(0.0f, 1.0f, diffAmount);

        for (size_t i = 0; i < N; ++i)
            x[i] -= factor;
    }
};

} // namespace dsp_primitives
