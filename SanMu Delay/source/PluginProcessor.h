// ==============================================================================
// PluginProcessor.h - SanMu Delay 插件音频处理器
// DAW 与插件交互的核心入口：参数管理、主与侧链音频总线、状态序列化
// ==============================================================================

#pragma once
#include "BetaAccess.h"

#ifndef JucePlugin_Name
#define JucePlugin_Name "SanMu Delay"
#endif

#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/DelayProcessor.h"
#include "params/Parameters.h"

class SanMuDelayAudioProcessor : public juce::AudioProcessor
{
public:
    sanmu_beta::Access betaAccess;
    sanmu_beta::AudioGate betaGate;
    SanMuDelayAudioProcessor(juce::File betaRecord = {});
    ~SanMuDelayAudioProcessor() override;

    // ---- AudioProcessor 生命周期 ----
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;
    void processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    // ---- 总线布局支持（单声道/立体声主总线与可选外部侧链） ----
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    // ---- 状态保存与恢复 ----
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // ---- 编辑器 ----
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    // ---- 插件元信息 ----
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }

    // 延迟效果器必须上报尾长（10 秒），防止走带停止时回声被截断
    double getTailLengthSeconds() const override { return 10.0; }

    // 湿路延迟是效果本体，插件内部处理延迟严格为 0
    // getLatencySamples 保持默认 0

    // ---- 预设管理 ----
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    // ---- 参数系统与 DSP 访问 ----
    juce::UndoManager undoManager;
    juce::AudioProcessorValueTreeState apvts;

    dsp::DelayProcessor& getDelayProcessor() noexcept { return delayProcessor; }

    // ---- A/B 对比 ----
    enum class ActiveSnapshot { None, A, B };
    void clickA();
    void clickB();
    void copyActiveSnapshotToOther();
    ActiveSnapshot getActiveSnapshot() const noexcept { return activeSnapshot; }

    void notifyHostAllParameters();

private:
    dsp::DelayProcessor delayProcessor;
    ActiveSnapshot activeSnapshot { ActiveSnapshot::None };
    juce::ValueTree defaultState;
    juce::ValueTree snapshotA;
    juce::ValueTree snapshotB;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SanMuDelayAudioProcessor)
};
