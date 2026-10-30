// ==============================================================================
// PluginProcessor.cpp - SanMu Delay 插件音频处理器实现
// ==============================================================================

#include "PluginProcessor.h"
#include "PluginEditor.h"

SanMuDelayAudioProcessor::SanMuDelayAudioProcessor(juce::File betaRecord)
    : juce::AudioProcessor(BusesProperties()
          .withInput("Input", juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)
          .withInput("Sidechain", juce::AudioChannelSet::stereo(), true))
    , betaAccess(betaRecord),
      apvts(*this, &undoManager, "Parameters", Params::createParameterLayout())
{
    defaultState = apvts.copyState();
    undoManager.beginNewTransaction();
}

void SanMuDelayAudioProcessor::notifyHostAllParameters()
{
    for (auto* p : getParameters())
        p->setValueNotifyingHost(p->getValue());
}

static juce::ValueTree withCurrentEditorPreferences(const juce::ValueTree& source, const juce::ValueTree& current)
{
    auto copy = source.createCopy();
    for (int i = 0; i < current.getNumProperties(); ++i)
    {
        const auto key = current.getPropertyName(i);
        if (key.toString().startsWith("ui_"))
            copy.setProperty(key, current.getProperty(key), nullptr);
    }
    if (auto themeSource = current.getChildWithProperty("id", Params::IDs::theme); themeSource.isValid())
    {
        if (auto themeTarget = copy.getChildWithProperty("id", Params::IDs::theme); themeTarget.isValid())
            themeTarget.setProperty("value", themeSource.getProperty("value"), nullptr);
    }
    return copy;
}

void SanMuDelayAudioProcessor::copyActiveSnapshotToOther()
{
    if (activeSnapshot != ActiveSnapshot::B)
        snapshotB = apvts.copyState();
    else if (activeSnapshot == ActiveSnapshot::B)
        snapshotA = apvts.copyState();
}

void SanMuDelayAudioProcessor::clickA()
{
    undoManager.beginNewTransaction();
    if (activeSnapshot == ActiveSnapshot::A)
    {
        if (defaultState.isValid())
        {
            apvts.replaceState(withCurrentEditorPreferences(defaultState, apvts.copyState()));
            notifyHostAllParameters();
        }
        activeSnapshot = ActiveSnapshot::None;
        return;
    }

    if (activeSnapshot == ActiveSnapshot::B || activeSnapshot == ActiveSnapshot::None)
        snapshotB = apvts.copyState();

    if (!snapshotA.isValid())
        snapshotA = defaultState.createCopy();

    apvts.replaceState(withCurrentEditorPreferences(snapshotA, apvts.copyState()));
    notifyHostAllParameters();
    activeSnapshot = ActiveSnapshot::A;
}

void SanMuDelayAudioProcessor::clickB()
{
    undoManager.beginNewTransaction();
    if (activeSnapshot == ActiveSnapshot::B)
    {
        if (defaultState.isValid())
        {
            apvts.replaceState(withCurrentEditorPreferences(defaultState, apvts.copyState()));
            notifyHostAllParameters();
        }
        activeSnapshot = ActiveSnapshot::None;
        return;
    }

    if (activeSnapshot == ActiveSnapshot::A || activeSnapshot == ActiveSnapshot::None)
        snapshotA = apvts.copyState();

    if (!snapshotB.isValid())
        snapshotB = defaultState.createCopy();

    apvts.replaceState(withCurrentEditorPreferences(snapshotB, apvts.copyState()));
    notifyHostAllParameters();
    activeSnapshot = ActiveSnapshot::B;
}

SanMuDelayAudioProcessor::~SanMuDelayAudioProcessor() = default;

void SanMuDelayAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    betaGate.prepare(sampleRate, samplesPerBlock, betaAccess.allowed());
    delayProcessor.prepare(sampleRate, samplesPerBlock);
}

void SanMuDelayAudioProcessor::releaseResources()
{
    delayProcessor.reset();
}

bool SanMuDelayAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    // 单声道与立体声插入均保持输入、输出声道数相同。
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();
    if (input != output
        || (input != juce::AudioChannelSet::mono() && input != juce::AudioChannelSet::stereo()))
        return false;

    // 侧链输入允许禁用、单声道或立体声
    const auto sc = layouts.getChannelSet(true, 1);
    if (!sc.isDisabled() && sc != juce::AudioChannelSet::stereo() && sc != juce::AudioChannelSet::mono())
        return false;

    return true;
}

void SanMuDelayAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& /*midiMessages*/)
{
    if (!betaGate.begin(buffer, getMainBusNumOutputChannels(), getLatencySamples(), betaAccess.allowed())) return;
    sanmu_beta::AudioGate::Scope betaScope { betaGate, buffer };

    juce::ScopedNoDenormals noDenormals;

    auto mainBus = getBusBuffer(buffer, true, 0);

    // 获取侧链总线（如果已启用且连接）
    const juce::AudioBuffer<float>* scPtr = nullptr;
    juce::AudioBuffer<float> scBuffer;

    auto* sidechainBus = getBus(true, 1);
    if (sidechainBus != nullptr && sidechainBus->isEnabled())
    {
        scBuffer = getBusBuffer(buffer, true, 1);
        scPtr = &scBuffer;
    }

    delayProcessor.process(mainBus, scPtr, getPlayHead(), apvts);
}

void SanMuDelayAudioProcessor::processBlockBypassed(juce::AudioBuffer<float>& /*buffer*/, juce::MidiBuffer& /*midiMessages*/)
{
    // 旁通路径保持原样输出
}

void SanMuDelayAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void SanMuDelayAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

juce::AudioProcessorEditor* SanMuDelayAudioProcessor::createEditor()
{
    return new SanMuDelayAudioProcessorEditor(*this);
}

// ---- JUCE 插件实例化入口 ----
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SanMuDelayAudioProcessor();
}
