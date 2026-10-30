// ==============================================================================
// PluginProcessor.h - 插件音频处理器
// DAW 与插件的交互入口：管理参数、处理音频、保存/恢复状态
// ==============================================================================

#pragma once
#include "BetaAccess.h"

#ifndef JucePlugin_Name
#define JucePlugin_Name "SanMuQ6"
#endif

#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/EqProcessor.h"

/**
 * SanMuQ6AudioProcessor：EQ 插件的核心 AudioProcessor 子类。
 *
 * DAW（Pro Tools / Logic / REAPER 等）通过此类与插件交互：
 * - 传输音频数据（processBlock）
 * - 发现自动化参数（APVTS）
 * - 保存/恢复插件状态（getStateInformation / setStateInformation）
 */
class SanMuQ6AudioProcessor : public juce::AudioProcessor,
                              private juce::AsyncUpdater,
                              private juce::ValueTree::Listener
{
public:
    sanmu_beta::Access betaAccess;
    sanmu_beta::AudioGate betaGate;
    // ---- 构造函数 / 析构函数 ----
    SanMuQ6AudioProcessor(juce::File betaRecord = {});
    ~SanMuQ6AudioProcessor() override;

    // ---- AudioProcessor 生命周期 ----
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;
    void releaseResources() override;

    // ---- 总线配置 ----
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    // ---- 状态保存/恢复（DAW 工程文件 + 预设） ----
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // ---- 编辑器 ----
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    // 宿主在消息线程提供轨道信息；不属于预设或音频参数。
    void updateTrackProperties(const TrackProperties& properties) override { trackName = properties.name.value_or(juce::String()); }
    juce::String getTrackName() const { return trackName; }

    // ---- 插件身份 ----
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    // 尾音按当前实际线性相位引擎的延迟上报；日常档沿用旧引擎，高精度/极限档使用多采样率引擎。
    double getTailLengthSeconds() const override { return eqModeParam && eqModeParam->load() >= .5f && getSampleRate() > 0 ? 2.0 * eqProcessor.getLinearLatency() / getSampleRate() : 0.0; }

    // ---- 预设 ----
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    // ---- 参数系统 ----
    /** 撤销/重做历史（第三十九轮：拖动、输入、切换等参数修改可撤销） */
    juce::UndoManager undoManager;
    /** 参数树：所有可自动化参数的存储 */
    juce::AudioProcessorValueTreeState apvts;

    // ---- DSP 访问（供 Editor 使用） ----
    EqProcessor& getEqProcessor() { return eqProcessor; }
    int getLinearLegacyMask() const noexcept { return linearLegacyMask.load(std::memory_order_relaxed); }
    bool isLinearDynamicAllowed(int index) const noexcept { return (getLinearLegacyMask() & (1 << index)) == 0; }
    void enableLinearDynamicBand(int index);
    int getLinearQuality() const noexcept { return linearQuality.load(std::memory_order_relaxed); }
    void setLinearQuality(int quality);

    // ---- A/B 对比 ----
    enum class ActiveSnapshot { None, A, B };
    void clickA();
    void clickB();
    void switchToSnapshot(ActiveSnapshot target);
    void copyActiveToOtherSnapshot();
    ActiveSnapshot getActiveSnapshot() const { return activeSnapshot; }

private:
    std::atomic<int> linearLegacyMask{0};
    std::atomic<int> linearQuality{0};
    void refreshLinearCompatibility();
    void valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeRedirected(juce::ValueTree&) override;
    juce::String trackName;
    EqProcessor eqProcessor;

    // 缓存的参数指针（避免 processBlock 中每次查表）
    // 这些指针由 APVTS 管理生命周期，不需要手动释放
    struct BandParamPtrs
    {
        std::atomic<float>* present   = nullptr;
        std::atomic<float>* enabled   = nullptr;
        std::atomic<float>* type      = nullptr;
        std::atomic<float>* freq      = nullptr;
        std::atomic<float>* gain      = nullptr;
        std::atomic<float>* q         = nullptr;
        std::atomic<float>* solo      = nullptr;
        std::atomic<float>* slope     = nullptr;
        std::atomic<float>* placement = nullptr;
        std::atomic<float>* dynamic = nullptr;
        std::atomic<float>* dynAuto = nullptr;
        std::atomic<float>* dynUpward = nullptr;
        std::atomic<float>* dynRange = nullptr;
        std::atomic<float>* dynThreshold = nullptr;
    };
    std::array<BandParamPtrs, Params::NUM_BANDS> bandParams;
    std::atomic<float>* inputGainLParam  = nullptr;
    std::atomic<float>* inputGainRParam  = nullptr;
    std::atomic<float>* outputGainLParam = nullptr;
    std::atomic<float>* outputGainRParam = nullptr;
    std::atomic<float>* phaseInvertParam   = nullptr;
    std::atomic<float>* eqModeParam        = nullptr;
    std::atomic<float>* globalBypassParam  = nullptr;
    std::atomic<float>* autoGainParam      = nullptr;
    std::atomic<float>* characterModeParam = nullptr;

    // 最近一次上报 DAW 的延迟（用于检测模式切换变化）
    std::atomic<int> lastReportedLatency{-1};

    // ---- 延迟异步上报（音频回调内不直接 setLatencySamples） ----
    // 宿主对音频线程内改变延迟敏感（可能静音/重建图失败），
    // 通过 AsyncUpdater 在消息线程上报
    void handleAsyncUpdate() override;

    /** 在 processBlock 前缓存参数指针（首次或参数重建后调用） */
    void cacheParameterPointers();

    // ---- A/B 快照存储 ----
    juce::ValueTree snapshotA;
    juce::ValueTree snapshotB;
    juce::ValueTree defaultState;  // 构建时保存的干净默认参数
    ActiveSnapshot activeSnapshot = ActiveSnapshot::None;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SanMuQ6AudioProcessor)
};
