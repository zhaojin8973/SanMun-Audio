// ==============================================================================
// EqBand.h - 单个 EQ 频段
// 封装双二阶滤波器及其参数，支持 Cut 滤波器多级级联（12-48 dB/oct）
//
// 注意：不使用 ProcessorDuplicator，因为它不会将 state 更新传播到
// 每个声道的 IIR::Filter 实例，导致滤波器持续使用空系数→输出静音。
// ==============================================================================

#pragma once
#include "LinearMagnitude.h"

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include "TptSvf.h"
#include "DynamicBand.h"

#include <vector>
#include <memory>
#include <atomic>
#include <array>

/**
 * EqBand：单个参数 EQ 频段。
 *
 * 每个频段包含：
 * - 左右声道各 1-4 级 IIR 滤波器（Cut 类型支持多级级联）
 * - 滤波器类型、频率、增益、Q 值、开关状态
 * - 使用 JUCE 内置 getMagnitudeForFrequency() 做频响计算
 */
class EqBand
{
public:
    // ---- 最大级联级数（48dB/oct） ----
    static constexpr int MAX_STAGES = 4;

    // ---- 构造函数 ----
    EqBand() = default;

    // ---- 生命周期 ----
    /** 准备滤波器（设置采样率和最大块大小） */
    void prepare(const juce::dsp::ProcessSpec& spec);

    /** 重置滤波器状态（静音时调用） */
    void reset();

    // ---- 参数更新 ----
    /**
     * 更新频段参数。
     * 如果参数发生变化，会自动重新计算滤波器系数。
     * @returns 如果参数确实变化了返回 true（用于 UI 触发重绘）
     */
    bool update(int newType, bool newEnabled,
                float newFrequency, float newGain, float newQ,
                bool newSolo, int newSlope, int newPlacement,
                bool newPresent = true, bool newDynamic = false,
                float newDynRange = 0.0f, float newDynThreshold = -24.0f, bool newDynAuto = false, bool newDynUpward = false);

    // ---- 系数计算 ----
    /** 根据当前参数重新计算系数，同时更新所有级联滤波器。 */
    void recalculateCoefficients(double sampleRate);

    /** 每块把当前参数朝目标平滑过渡一步（参数域平滑，消除参数跳变爆音） */
    void advanceSmoothing(double sampleRate, int elapsedSamples = 128);

    // ---- 音频处理 ----
    /** 处理单个声道的一个采样块（由 EqProcessor 按声道调用） */
    void processChannel(int channel, juce::dsp::AudioBlock<float>& block);

    /** 处理单声道单个采样（由 EqProcessor M/S 处理调用） */
    float processSampleChannel(int channel, float sample);

    // ---- 频响计算（使用 JUCE 内置方法，保证正确性） ----
    /** 获取此频段在给定频率的幅度响应（dB） */
    double getMagnitudeAtFrequency(double freqHz, double sampleRate) const;

    /** 音频线程专用：直接读目标系数求幅度响应（无锁，仅供 Linear Phase FIR 设计） */
    double getMagnitudeAtFrequencyAudioThread(double freqHz, double sampleRate) const;
    void prepareAudioResponse(double sampleRate);
    double getPreparedMagnitudeAtFrequency(double freqHz) const;
    double getPreparedPowerAtFrequency(double frequencySquared) const;

    /** 获取 Cut 滤波器的真实共振峰频率（多级级联后由主级 Q 与 Butterworth 节共同决定） */
    double getCutPeakFrequency(double sampleRate) const;

    /** 获取 Cut 滤波器的当前斜率级数（1-4 = 12-48 dB/oct） */
    int getNumStages() const { return numStages; }
    float getDynamicReduction() const { return dynamicProcessor.getReduction(); }

    // ---- 公共数据成员（EqProcessor / UI 需要直接访问） ----
    // 注意：以下参数在音频线程写入、UI 线程读取，因此使用原子类型，
    // 避免数据竞争（UI 读到的可能是音频线程正在更新的值）。
    TptSvf svfL;   // 左声道主级滤波器
    TptSvf svfR;   // 右声道主级滤波器
    std::atomic<bool>   present  { true };     // M1 新增：槽位是否作为当前频段存在
    std::atomic<int>    type     { 0 };        // 滤波器类型 0=Bell, 1=LowShelf, 2=HighShelf, 3=LowCut, 4=HighCut
    std::atomic<bool>   enabled  { true };     // 频段开关
    std::atomic<bool>   solo     { false };    // 频段独听
    std::atomic<float>  frequency{ 1000.0f };  // 中心/截止频率（Hz）
    std::atomic<float>  gain     { 0.0f };     // 增益（dB，对 Cut 类型无效）
    std::atomic<float>  q        { 1.0f };     // Q 值（控制 Bell/Shelf 带宽、Cut 共振）
    std::atomic<int>    slope    { 0 };        // Cut 滤波器斜率：0=12, 1=24, 2=36, 3=48 dB/oct
    std::atomic<int>    placement{ 0 };        // 通道分布：0=Stereo, 1=Left, 2=Right, 3=Mid, 4=Side
    std::atomic<bool>   dynamicEnabled{ false };
    std::atomic<float>  dynamicRange{ 0.0f };
    std::atomic<float>  dynamicThreshold{ -24.0f };
    std::atomic<bool> dynamicUpward { false };
    DynamicBand dynamicProcessor;

private:
    // ---- 级联滤波器（Cut 类型多级用，最多 4 级即 1 主级 + 3 额外级，定长预分配零堆分配） ----
    std::array<TptSvf, MAX_STAGES - 1> extraSvfL;  // 左声道 2-4 级
    std::array<TptSvf, MAX_STAGES - 1> extraSvfR;  // 右声道 2-4 级
    int numStages = 1;                             // 当前级数（1-4）
    int activeExtraCount = 0;

    // ---- 频响计算 ----
    // 不可变 SVF 参数快照：主级 + 额外级。
    struct BandSvfParams
    {
        TptSvf::Type mainType = TptSvf::Type::LowPass;
        double       mainCutoff = 1000.0;
        double       mainQ       = 0.7071067811865476;
        double       mainGainDb  = 0.0;
        std::array<TptSvf::Type, MAX_STAGES - 1> extraType;
        std::array<double, MAX_STAGES - 1> extraQ;
        int extraCount = 0;
    };

    // 参数 → SVF 参数纯函数：音频线程与 UI 线程共用同一份计算逻辑，
    // 保证 UI 曲线与实际 DSP 处理完全一致。
    static BandSvfParams computeSvfParams(
        int type, float frequency, float gain, float q, int slope,
        double sampleRate);

    // UI 线程专用频响缓存：getMagnitudeAtFrequency 现在只在 UI 线程调用，
    // 参数不变时复用上次参数，避免每个频响点都重新计算。
    mutable BandSvfParams cachedResponseParams;
    mutable int   cachedResponseType      = -1;
    mutable int   cachedResponseSlope     = -1;
    mutable float cachedResponseFrequency = -1.0f;
    mutable float cachedResponseGain      = -999.0f;
    mutable float cachedResponseQ         = -1.0f;
    mutable double cachedResponseSampleRate = -1.0;

    BandSvfParams audioResponseParams;
    std::array<LinearMagnitude, 4> audioPowerResponse;

    // ---- 参数域平滑（ZeroLatency 模式） ----
    // 直接平滑频率/增益/Q 连续参数，每块用平滑参数重算系数，
    // 保证每个中间状态都是合法滤波器（极点恒在单位圆内）。
    float smoothedFreq = 1000.0f;
    float smoothedGain = 0.0f;
    float smoothedQ    = 1.0f;
    float targetFreq   = 1000.0f;
    float targetGain   = 0.0f;
    float targetQ      = 1.0f;
    bool  smoothingParamsReady = false;  // 首次初始化完成
    bool  smoothingActive      = false;  // 是否有待平滑的目标

    // ---- 变更检测 ----
    float previousFrequency  = -1.0f;
    float previousGain       = -999.0f;
    float previousQ          = -1.0f;
    int   previousType       = -1;
    bool  previousEnabled    = true;
    bool  previousSolo       = false;
    int   previousSlope      = -1;
    int   previousPlacement  = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EqBand)
};
