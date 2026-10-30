// ==============================================================================
// PluginProcessor.cpp - SanMu Reverb 音频处理器核心实现
// ==============================================================================

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "params/StateCompatibility.h"
#include <cmath>

SanMuReverbAudioProcessor::SanMuReverbAudioProcessor(juce::File betaRecord)
    : AudioProcessor(BusesProperties()
                     .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      betaAccess(betaRecord),
      apvts(*this, nullptr, "Parameters", Params::createParameterLayout())
{
    state_compatibility::markCurrent(apvts.state);
}

void SanMuReverbAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    betaGate.prepare(sampleRate, samplesPerBlock, betaAccess.allowed());
    reverbProcessor.prepare(sampleRate, samplesPerBlock);
    stateResetRequested.store(false, std::memory_order_relaxed);
}

void SanMuReverbAudioProcessor::releaseResources()
{
    reverbProcessor.reset();
}

void SanMuReverbAudioProcessor::reset()
{
    reverbProcessor.reset();
}

double SanMuReverbAudioProcessor::getTailLengthSeconds() const
{
    const auto read = [this](const juce::String& id, double fallback, double low, double high)
    {
        const auto* raw = apvts.getRawParameterValue(id);
        const double value = raw != nullptr ? raw->load(std::memory_order_relaxed) : fallback;
        return std::isfinite(value) ? juce::jlimit(low, high, value) : fallback;
    };
    const int category = juce::roundToInt(read(Params::IDs::CATEGORY, 0.0, 0.0, 3.0));
    const int mode = juce::roundToInt(read(Params::IDs::MODE, 0.0, 0.0, 2.0));
    double modeScale = 1.0;
    if (category == 0 && mode == 2) modeScale = 1.15;
    if (category == 2 && mode == 1) modeScale = 1.15;
    if (category == 2 && mode == 2) modeScale = 1.35;
    if (category == 3 && mode == 2) modeScale = 1.20;

    // 按当前面板的最长标称频段预留两倍 RT60 与 1s 起音余量。
    // 这是宿主停止渲染的保守估算，不能作为声学衰减已完成校准的证据。
    const double decay = read(Params::IDs::DECAY, 3.99, 0.2, 20.0);
    const double bass = read(Params::IDs::BASS_MULT, 1.5, 1.0, 4.0);
    return juce::jlimit(1.0, 220.0, 2.0 * decay * bass * modeScale + 1.0);
}

bool SanMuReverbAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
}

void SanMuReverbAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& /*midiMessages*/)
{
    processAudio(buffer, apvts.getRawParameterValue(Params::IDs::BYPASS)->load() > .5f);
}

void SanMuReverbAudioProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    processAudio(buffer, true);
}

void SanMuReverbAudioProcessor::processAudio(juce::AudioBuffer<float>& buffer, bool bypass)
{
    if (!betaGate.begin(buffer, getMainBusNumOutputChannels(), getLatencySamples(), betaAccess.allowed())) return;
    sanmu_beta::AudioGate::Scope betaScope { betaGate, buffer };

    juce::ScopedNoDenormals noDenormals;

    // 状态恢复可能来自非音频线程；在这里清空旧尾音，避免并发改写延迟线。
    if (stateResetRequested.exchange(false, std::memory_order_acquire))
        reverbProcessor.reset();

    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // 从 APVTS 读取参数快照打包给 DSP 引擎
    dsp_engines::ReverbParameters p;

    if (auto* raw = apvts.getRawParameterValue(Params::IDs::CATEGORY))
        p.category = juce::roundToInt(raw->load());
    if (auto* raw = apvts.getRawParameterValue(Params::IDs::MODE))
        p.mode = juce::roundToInt(raw->load());

    if (auto* raw = apvts.getRawParameterValue(Params::IDs::DECAY))
        p.decay = raw->load();
    if (auto* raw = apvts.getRawParameterValue(Params::IDs::PREDELAY))
        p.predelay = raw->load();
    if (auto* raw = apvts.getRawParameterValue(Params::IDs::MIX))
        p.mix = raw->load();

    if (auto* raw = apvts.getRawParameterValue(Params::IDs::HIGH_FREQ))
        p.highFreq = raw->load();
    if (auto* raw = apvts.getRawParameterValue(Params::IDs::HIGH_SHELF))
        p.highShelf = raw->load();
    if (auto* raw = apvts.getRawParameterValue(Params::IDs::BASS_FREQ))
        p.bassFreq = raw->load();
    if (auto* raw = apvts.getRawParameterValue(Params::IDs::BASS_MULT))
        p.bassMult = raw->load();

    if (auto* raw = apvts.getRawParameterValue(Params::IDs::SIZE))
        p.size = raw->load();
    if (auto* raw = apvts.getRawParameterValue(Params::IDs::ATTACK))
        p.attack = raw->load();

    if (auto* raw = apvts.getRawParameterValue(Params::IDs::EARLY_DIFF))
        p.earlyDiff = raw->load();
    if (auto* raw = apvts.getRawParameterValue(Params::IDs::LATE_DIFF))
        p.lateDiff = raw->load();

    if (auto* raw = apvts.getRawParameterValue(Params::IDs::MOD_RATE))
        p.modRate = raw->load();
    if (auto* raw = apvts.getRawParameterValue(Params::IDs::MOD_DEPTH))
        p.modDepth = raw->load();

    if (auto* raw = apvts.getRawParameterValue(Params::IDs::LOW_CUT))
        p.lowCut = raw->load();
    if (auto* raw = apvts.getRawParameterValue(Params::IDs::HIGH_CUT))
        p.highCut = raw->load();

    reverbProcessor.process(buffer, p, bypass);
}

juce::AudioProcessorEditor* SanMuReverbAudioProcessor::createEditor()
{
    return new SanMuReverbAudioProcessorEditor(*this);
}

void SanMuReverbAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty("editorState", getEditorState(), nullptr);
    state_compatibility::markCurrent(state);
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void SanMuReverbAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (data == nullptr || sizeInBytes <= 0) return;
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
    {
        auto state=juce::ValueTree::fromXml(*xmlState);
        if (!state_compatibility::isValid(state, apvts)) return;
        state_compatibility::migrate(state);
        setEditorState(state.getProperty("editorState").toString());
        apvts.replaceState(state);
        stateResetRequested.store(true, std::memory_order_release);
    }
}

// ---- JUCE 插件入口导出 ----
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SanMuReverbAudioProcessor();
}
