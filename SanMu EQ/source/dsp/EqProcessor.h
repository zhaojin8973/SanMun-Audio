// ==============================================================================
// EqProcessor.h - 24 槽位 EQ 处理器
// 编排 24 个 EqBand，串联处理音频，并为 UI 计算频响曲线 + 实时频谱
// ==============================================================================

#pragma once

#include "EqBand.h"
#include "LinearPhaseEq.h"
#include "PrecisionLinearEq.h"
#include "SpectrumAnalyser.h"
#include "../params/Parameters.h"

#include <juce_dsp/juce_dsp.h>

/**
 * EqProcessor：EQ 的 DSP 核心。
 *
 * 职责：
 * - 管理 24 个 EqBand 的级联
 * - 三种音色模式：Clean / Tube / Discrete —— 共享用户可预测的 EQ 幅频曲线，
 *   只在谐波与饱和特质上区分；它们都可以叠加独立的线性相位处理
 * - 线性相位不是第四种音色：设置中提供日常 / 高精度 / 极限三档，
 *   分别使用日常兼容引擎或多采样率高精度引擎，并按档位上报实际延迟
 * - 为 UI 提供合并后的频率响应数据
 * - 实时 FFT 频谱分析（Post-EQ，8192 点，对数频带 RMS 平均 + EMA 平滑 + 峰值保持）
 *   输出真实 dBFS 值，不做 Tilt/增益等形状改写，确保读数准确不误导
 */
class EqProcessor
{
public:
    // ---- 构造函数 ----
    EqProcessor();

    // ---- 生命周期 ----
    /** 准备所有频段、线性相位引擎和频谱分析（设置采样率、最大块大小） */
    void prepare(const juce::dsp::ProcessSpec& spec);

    /** 重置所有频段状态和线性相位引擎 */
    void reset();

    // ---- 参数更新 ----
    /**
     * 更新指定频段的参数。
     * 如果参数变化则重新计算系数。
     * 频段从 0 开始编号。
     */
    void updateBand(int index, int type, bool enabled,
                    float frequency, float gain, float q,
                    bool solo, int slope, int placement, double sampleRate,
                    bool present = true, bool dynamic = false,
                    float dynamicRange = 0.0f, float dynamicThreshold = -24.0f, bool dynamicAuto = false, bool dynamicUpward = false);

    /** 设置输入 L/R 增益（dB，EQ 前应用） */
    void setInputGainL(float gainDb) { inputGainL = juce::Decibels::decibelsToGain(gainDb); }
    void setInputGainR(float gainDb) { inputGainR = juce::Decibels::decibelsToGain(gainDb); }

    /** 设置输出 L/R 增益（dB，EQ 后应用；第九轮替代 输出增益+声像平衡） */
    void setOutputGainL(float gainDb) { outputGainL = juce::Decibels::decibelsToGain(gainDb); }
    void setOutputGainR(float gainDb) { outputGainR = juce::Decibels::decibelsToGain(gainDb); }

    /** 设置全局旁通（true=旁通 EQ 滤波，false=正常处理） */
    void setGlobalBypass(bool bypass) { globalBypass.store(bypass, std::memory_order_relaxed); }
    bool isGlobalBypassed() const { return globalBypass.load(std::memory_order_relaxed); }

    /** 设置自动增益补偿（true=开启根据 EQ 曲线反向补偿响度） */
    void setAutoGain(bool enabled) { autoGainEnabled.store(enabled, std::memory_order_relaxed); }
    bool isAutoGainEnabled() const { return autoGainEnabled.load(std::memory_order_relaxed); }

    /** 设置线性相位处理开关（0=关闭，1=开启）；音色由 character_mode 单独控制 */
    void setProcessingMode(int mode);
    void setCharacterMode(int mode) { characterMode.store(juce::jlimit(0, 2, mode), std::memory_order_relaxed); }

    // ---- 处理模式 ----
    enum class ProcessingMode { ZeroLatency, LinearPhase };

    // ---- 音频处理 ----
    /** 处理一个音频块：过采样 → 级联滤波 → 降采样 */
    void process(juce::AudioBuffer<float>& buffer);

    // ---- 频响曲线数据（供 UI 使用） ----
    /**
     * 计算所有频段在给定频率处的合并增益（dB）。
     * 在 UI 线程中通过 Timer 调用，不要在音频线程中调用。
     */
    float getMagnitudeAtFrequency(float freqHz, double sampleRate) const;

    /** 音频线程专用：读目标系数求整链响应（无锁，仅供 Linear Phase FIR 设计，按通道分别计算） */
    float getMagnitudeAtFrequencyAudioThread(float freqHz, double sampleRate, int channel = 0) const;

    /** 获取指定频段的只读引用 */
    const EqBand& getBand(int index) const { return bands[static_cast<size_t>(index)]; }
    float getAppliedDynamicReduction(int index) const
    {
        return displayedReduction[static_cast<size_t>(index)].load(std::memory_order_relaxed);
    }

    /** 检查是否有频段启用了独听 */
    bool isAnyBandSoloed() const;
    // 临时监听不属于声音参数，不能被预设或宿主自动化保存。
    void setAuditionBand(int index) { auditionBand.store(index >= 0 && index < Params::MAX_BANDS ? index : -1); }
    int getAuditionBand() const { return auditionBand.load(); }

    /** 获取过采样延迟（采样数） */
    int getLatencySamples() const { return latencySamples; }

    // ---- 输入电平数据（供 UI 电平表读取） ----
    float getInputLeftPeakDb() const  { return inputLeftPeakDb.load(); }
    float getInputRightPeakDb() const { return inputRightPeakDb.load(); }
    float getInputLeftRmsDb() const   { return inputLeftRmsDb.load(); }
    float getInputRightRmsDb() const  { return inputRightRmsDb.load(); }

    // ---- 输出电平数据（供 UI 电平表读取） ----
    float getLeftPeakDb() const  { return leftPeakDb.load(); }
    float getRightPeakDb() const { return rightPeakDb.load(); }
    float getLeftRmsDb() const   { return leftRmsDb.load(); }
    float getRightRmsDb() const  { return rightRmsDb.load(); }

    uint64_t getMeterSequence() const { return meterSequence.load(std::memory_order_relaxed); }
    void setSpectrumResolution(int choice) { spectrumAnalyser.setResolution(choice); }
    // ---- 实时频谱数据（供 UI 频响曲线绘制） ----
    // 准确性优先设计：
    //   8192 点 FFT（~5.4Hz 分辨率 @44.1k，窗口约 186ms，接近实时响应）、Post-EQ 采集、
    //   对数频带峰值检测、EMA 时间平滑、峰值保持
    //   输出真实 dBFS 值——无 Tilt 倾斜补偿、无显示增益，不改写频谱形状
    static constexpr int FFT_ORDER = 13;              // 2^13 = 8192 点
    static constexpr int FFT_SIZE = 1 << FFT_ORDER;
    static constexpr int FFT_WORK_SIZE = FFT_SIZE * 2; // JUCE 要求 2×size
    static constexpr int SPECTRUM_POINTS = 500;        // 与频响曲线点数一致
    // 2× 过采样倍数（ZeroLatency 级联路径）。DSP 系数设计与 UI 频响评估
    // 必须使用同一倍数，否则曲线会与实际听感错位。
    static constexpr double OVERSAMPLING_FACTOR = 2.0;

    /** 获取第 index 个频率点的平滑频谱幅值（dB），线程安全 */
    float getSpectrumPoint(int index) const
    {
        return spectrumAnalyser.get(index, 1);
    }
    float getInputSpectrumPoint(int index) const
    {
        return spectrumAnalyser.get(index, 0);
    }

    /** 获取第 index 个频率点的峰值保持幅值（dB），线程安全 */
    float getSpectrumPeakPoint(int index) const
    {
        return spectrumAnalyser.get(index, 2);
    }

    /** 判断频谱数据是否就绪（至少完成过一次 FFT） */
    bool isSpectrumReady() const { return spectrumAnalyser.ready(); }

private:
    // ---- 24 个 EQ 槽位 ----
    std::array<EqBand, Params::NUM_BANDS> bands;

    // ---- Solo 带通滤波器（持久实例，避免每块重建导致状态断裂/爆音） ----
    std::array<juce::dsp::IIR::Filter<float>, Params::NUM_BANDS> soloBpL;
    std::array<juce::dsp::IIR::Filter<float>, Params::NUM_BANDS> soloBpR;
    bool soloBpPrepared = false;

    // ---- 输入/输出 L/R 增益（线性增益因子，1.0 = 0dB） ----
    float inputGainL  = 1.0f;
    float inputGainR  = 1.0f;
    float outputGainL = 1.0f;
    float outputGainR = 1.0f;

    // ---- 采样率（用于 Solo 带通滤波器系数计算） ----
    double currentSampleRate = 44100.0;

    // ---- 音色模式与独立线性相位处理状态 ----
    ProcessingMode processingMode = ProcessingMode::ZeroLatency;
    std::unique_ptr<LinearPhaseEq> linearPhase;  // 线性相位引擎（prepare 预分配）
    std::array<std::unique_ptr<PrecisionLinearEq>, 3> precisionLinear;
    int linearQuality = 0;
public:
    void setLinearQuality(int quality);
    int getLinearQuality() const noexcept { return linearQuality; }
    int getLinearLatency() const noexcept
    {
        if (linearQuality == 0) return LinearPhaseEq::LATENCY;
        const auto& engine = precisionLinear[static_cast<size_t>(linearQuality)];
        return engine ? engine->getLatencySamples() : PrecisionLinearEq::latencyFor(currentSampleRate, linearQuality);
    }
private:
    bool firDirty = true;                        // 线性相位 FIR 需要重建（参数/模式变化时置位）
    int latencySamples = 0;                      // 上报 DAW 的延迟（随模式变化）
    std::array<std::atomic<float>, Params::MAX_BANDS> displayedReduction{};

    // ---- 2× 过采样（ZeroLatency 级联路径专用；高频双线性卷曲消除） ----
    // 系数按 2×sr 设计（updateBand/评估口径统一），延迟约 2-3 采样 ≈0.05ms
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    int oversamplingLatencySamples = 0;   // prepare 时从引擎读取（勿硬编码）

    // 按采样数节流，避免宿主块长改变动态响应速度。
    static constexpr double REBUILD_INTERVAL_SECONDS = 0.025;
    int linearPhaseRebuildCountdown = 0;
    int linearControlFill = 0;
    int minimumControlFill = 0;
    struct MatrixBandSnapshot
    {
        bool active = false;
        int type = 0;
        int placement = 0;
        float frequency = 1000.0f;
        float q = 1.0f;
        float dynamicGainDb = 0.0f;
        LinearMagnitude dynamicResponse;
    };
    std::array<MatrixBandSnapshot, Params::NUM_BANDS> linearBandSnapshot{};

    // ---- 输入电平（原子变量，音频线程写 / UI 线程读） ----
    std::atomic<float> inputLeftPeakDb  { -100.0f };
    std::atomic<float> inputRightPeakDb { -100.0f };
    std::atomic<float> inputLeftRmsDb   { -100.0f };
    std::atomic<float> inputRightRmsDb  { -100.0f };

    // ---- 输出电平（原子变量，音频线程写 / UI 线程读） ----
    std::atomic<float> leftPeakDb  { -100.0f };
    std::atomic<float> rightPeakDb { -100.0f };
    std::atomic<uint64_t> meterSequence {0};
    std::atomic<float> leftRmsDb   { -100.0f };
    std::atomic<float> rightRmsDb  { -100.0f };

    // ---- 内部方法 ----
    /** 线性相位 FIR 设计的静态幅度采样回调（返回指定声道整链 dB） */
    static float sampleMagnitude(float freqHz, double sr, int channel, void* ctx);
    static void sampleMatrix(float freqHz, double sr, float matrix[4], void* ctx);

    /** 正常模式处理（非 Solo）——内部辅助方法 */
    void processNormal(juce::dsp::AudioBlock<float>& block);
    void processMinimumPhase(juce::AudioBuffer<float>& buffer);

    /** Solo 模式处理——内部辅助方法，sampleRate 用于带通滤波器系数计算 */
    void processSolo(juce::AudioBuffer<float>& buffer, double sampleRate);
    void applyCharacter(juce::dsp::AudioBlock<float>& block);
    bool updateLinearDynamicControls(const juce::AudioBuffer<float>& buffer);
    void processLinear(juce::AudioBuffer<float>& buffer);
    void processLegacyLinear(juce::AudioBuffer<float>& buffer);

    /** 从输入缓冲区计算峰值和 RMS 电平（DSP 处理前调用） */
    void computeInputLevels(const juce::AudioBuffer<float>& buffer);

    /** 从输出缓冲区计算峰值和 RMS 电平 */
    void computeOutputLevels(const juce::AudioBuffer<float>& buffer);

    /** 累积采样到 FFT 环形缓冲区 */
    void accumulateSpectrum(const juce::AudioBuffer<float>& buffer);

    // ---- 零分配预分配成员缓冲区（prepare 时按 maximumBlockSize 分配） ----
    juce::AudioBuffer<float> dryBuffer;
    juce::AudioBuffer<float> soloTempBuffer;
    juce::AudioBuffer<float> linearPhaseDelayBuffer;
    int lpDelayWritePos = 0;

    SpectrumAnalyser spectrumAnalyser;
    std::atomic<int> characterMode{0};
    std::atomic<int> auditionBand{-1};
    juce::SmoothedValue<float> tubeMix, discreteMix;
    std::array<std::array<float, 2>, 2> characterPreviousInput{}, characterPreviousOutput{};

    // ---- 全局旁通与自动增益状态 ----
    std::atomic<bool> globalBypass{false};
    std::atomic<bool> autoGainEnabled{false};
    juce::SmoothedValue<float> bypassSmoother;     // 0.0=正常, 1.0=旁通 (20ms 平滑)
    juce::SmoothedValue<float> autoGainSmoother;   // 自动增益线性因子 (100ms 平滑)

    /** 实时计算感知响度偏移（dB）以供自动增益反向补偿 */
    float calculateAutoGainDb() const;
    bool autoGainDirty = true;
    float cachedAutoGain = 1;
    void getMatrixAtFrequencyAudioThread(float freqHz, double sampleRate, float matrix[4]) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EqProcessor)
};
