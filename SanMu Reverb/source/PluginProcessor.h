// ==============================================================================
// PluginProcessor.h - SanMu Reverb 音频处理器核心
// ==============================================================================

#pragma once
#include "BetaAccess.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/ReverbProcessor.h"
#include "params/Parameters.h"

class SanMuReverbAudioProcessor final : public juce::AudioProcessor
{
public:
    sanmu_beta::Access betaAccess;
    sanmu_beta::AudioGate betaGate;
    SanMuReverbAudioProcessor(juce::File betaRecord = {});
    ~SanMuReverbAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorParameter* getBypassParameter() const override { return apvts.getParameter(Params::IDs::BYPASS); }
    juce::String getEditorState() const { const juce::ScopedLock lock(editorStateLock); return editorState; }
    void setEditorState(juce::String state) { const juce::ScopedLock lock(editorStateLock); editorState = std::move(state); }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "SanMu Reverb"; }

    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    ReverbProcessor& getReverb() noexcept { return reverbProcessor; }
    juce::AudioProcessorValueTreeState apvts;

private:
    void processAudio(juce::AudioBuffer<float>&, bool bypass);
    mutable juce::CriticalSection editorStateLock;
    juce::String editorState;
    ReverbProcessor reverbProcessor;
    std::atomic<bool> stateResetRequested { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SanMuReverbAudioProcessor)
};
