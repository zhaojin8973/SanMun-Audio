#pragma once
#include "LinearPhaseEq.h"

// 多采样率线性相位引擎：低频在低采样率支路使用长时间窗，
// 各支路只处理互补的矩阵修正量，干声使用同一条精确延迟线。
// 参数快照的生命周期覆盖整个增量设计；音频回调内不分配、不加锁。
class PrecisionLinearEq
{
public:
    using MatrixSampler = LinearPhaseEq::MatrixSampler;
    PrecisionLinearEq();
    ~PrecisionLinearEq();
    void prepare(double sampleRate, int quality);
    void reset(bool preserveKernel = true);
    void beginRebuild(MatrixSampler sampler, void* context);
    void rebuild(MatrixSampler sampler, void* context);
    bool isRebuildPending() const noexcept;
    void processBlock(juce::AudioBuffer<float>& buffer, bool designTick);
    int getLatencySamples() const noexcept;
    void setPendingDynamicGain(int band, float gain) noexcept;
    float getAppliedReduction(int band) const noexcept;
    // 诊断读取真实分支核与抗混叠响应，不以目标曲线代替测量。
    double response(double frequency, int path) const;
    static int latencyFor(double sampleRate, int quality);
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
