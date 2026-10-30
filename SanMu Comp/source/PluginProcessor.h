// ==============================================================================
// PluginProcessor.h - 压缩器插件音频处理器
// DAW 与插件的交互入口：管理参数、处理音频、保存/恢复状态
// ==============================================================================

#pragma once
#include "BetaAccess.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include "dsp/CompressorProcessor.h"
#include "ai/LlmClient.h"

/**
 * SanMuCompAudioProcessor：压缩器插件的核心 AudioProcessor 子类。
 * DAW 通过此类与插件交互：传输音频、发现自动化参数、保存/恢复状态。
 */
class SanMuCompAudioProcessor : public juce::AudioProcessor
{
public:
    sanmu_beta::Access betaAccess;
    sanmu_beta::AudioGate betaGate;
    SanMuCompAudioProcessor(juce::File betaRecord = {});
    ~SanMuCompAudioProcessor() override;

    // ---- AudioProcessor 生命周期 ----
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;
    void processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;
    void releaseResources() override;

    // ---- 总线配置 ----
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    // ---- 状态保存/恢复 ----
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // ---- 编辑器 ----
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    // ---- 插件身份 ----
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    // ---- 预设 ----
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    // ---- 宿主音轨属性同步 ----
    void updateTrackProperties(const TrackProperties& properties) override;

    // ---- 参数系统 ----
    juce::UndoManager undoManager;
    juce::AudioProcessorValueTreeState apvts;

    // ---- DSP 访问（供 Editor 使用）----
    CompressorProcessor& getCompressor() { return compressor; }
    ai::AcousticFeatureExtractor& getFeatureExtractor() { return compressor.getAcousticExtractor(); }

    // ---- A/B 对比 ----
    enum class ActiveSnapshot { None, A, B };
    void clickA();
    void clickB();
    void copyActiveSnapshotToOther();
    ActiveSnapshot getActiveSnapshot() const { return activeSnapshot; }

    // ---- AI 方案灌入 ----
    void applyPresetRecommendation(const ai::CompressorPresetRecommendation& rec);
    void loadSingleRecommendation(const ai::SingleRecommendationResponse& resp);
    void notifyHostAllParameters();

private:
    void processAudio(juce::AudioBuffer<float>& buffer, bool hostBypass);
    CompressorProcessor compressor;
    std::array<std::atomic<float>*, 4> modeSc {}, modeTrim {}, modeLink {};


    // 缓存的参数指针（APVTS 管理生命周期，不需要手动释放）
    std::atomic<float>* modeParam           = nullptr;
    std::atomic<float>* thresholdParam      = nullptr;
    std::atomic<float>* ratioParam          = nullptr;
    std::atomic<float>* attackParam         = nullptr;
    std::atomic<float>* releaseParam        = nullptr;
    std::atomic<float>* kneeParam           = nullptr;
    std::atomic<float>* makeupParam         = nullptr;
    std::atomic<float>* mixParam            = nullptr;
    std::atomic<float>* fetMixParam         = nullptr;
    std::atomic<float>* optoMixParam        = nullptr;
    std::atomic<float>* outputTrimParam     = nullptr;
    std::atomic<float>* headroomParam       = nullptr;
    std::atomic<float>* sidechainEnabledParam = nullptr;
    std::atomic<float>* sidechainHpParam    = nullptr;
    std::atomic<float>* stereoLinkParam     = nullptr;
    std::atomic<float>* autoGainParam       = nullptr;
    std::atomic<float>* oversampleParam     = nullptr;
    std::atomic<float>* themeParam          = nullptr;
    std::atomic<float>* globalBypassParam   = nullptr;

    // FET 独立参数（1176）
    std::atomic<float>* fetInputParam       = nullptr;
    std::atomic<float>* fetRatioParam       = nullptr;
    std::atomic<float>* fetAttackParam      = nullptr;
    std::atomic<float>* fetReleaseParam     = nullptr;
    std::atomic<float>* fetOutputParam      = nullptr;

    // Opto 独立参数（LA-2A）
    std::atomic<float>* optoPeakReductionParam = nullptr;
    std::atomic<float>* optoGainParam          = nullptr;
    std::atomic<float>* optoIsLimitParam       = nullptr;

    // VCA 独立参数（SSL 4000 G 总线）
    std::atomic<float>* vcaThresholdParam      = nullptr;
    std::atomic<float>* vcaRatioParam          = nullptr;
    std::atomic<float>* vcaAttackParam         = nullptr;
    std::atomic<float>* vcaReleaseParam        = nullptr;
    std::atomic<float>* vcaMakeupParam         = nullptr;
    std::atomic<float>* vcaScHpParam           = nullptr;
    std::atomic<float>* vcaMixParam            = nullptr;

    // VOX 独立参数（对标 RVox）
    std::atomic<float>* voxGateParam           = nullptr;
    std::atomic<float>* voxCompParam           = nullptr;
    std::atomic<float>* voxGainParam           = nullptr;
    std::atomic<float>* voxMixParam            = nullptr;
    std::atomic<float>* voxSmoothParam         = nullptr;

    /** 在 processBlock 前缓存参数指针 */
    void cacheParameterPointers();

    // ---- A/B 快照存储 ----
    juce::ValueTree snapshotA;
    juce::ValueTree snapshotB;
    juce::ValueTree defaultState;
    ActiveSnapshot activeSnapshot = ActiveSnapshot::None;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SanMuCompAudioProcessor)
};
