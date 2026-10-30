// ==============================================================================
// PluginProcessor.cpp - 压缩器插件音频处理器实现
// ==============================================================================

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "dsp/engines/VoxEngine.h"

#include <cmath>

// ==============================================================================
// 构造函数 / 析构函数
// ==============================================================================

SanMuCompAudioProcessor::SanMuCompAudioProcessor(juce::File betaRecord)
    : juce::AudioProcessor(BusesProperties()
          .withInput("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , betaAccess(betaRecord),
      apvts(*this, &undoManager, "Parameters", Params::createParameterLayout())
{
    undoManager.beginNewTransaction();
    cacheParameterPointers();
    defaultState = apvts.copyState();
}

SanMuCompAudioProcessor::~SanMuCompAudioProcessor() = default;

// ==============================================================================
// 生命周期
// ==============================================================================

void SanMuCompAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    betaGate.prepare(sampleRate, samplesPerBlock, betaAccess.allowed(),
                     dsp_engines::VoxEngine::getLatencySamples(sampleRate, true));
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels      = static_cast<juce::uint32>(getTotalNumOutputChannels());

    const bool smooth = voxSmoothParam && voxSmoothParam->load() >= .5f;
    compressor.setVoxSmooth(smooth);
    compressor.prepare(spec);
    const int initialMode = modeParam ? int(std::lround(modeParam->load())) : Params::ModeClean;
    setLatencySamples(initialMode == Params::ModeVOX ? dsp_engines::VoxEngine::getLatencySamples(sampleRate, smooth) : 0);
}

void SanMuCompAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    processAudio(buffer, false);
}

void SanMuCompAudioProcessor::processAudio(juce::AudioBuffer<float>& buffer, bool hostBypass)
{

    juce::ScopedNoDenormals noDenormals;

    // 清除未使用的输出声道
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
    {
        if (totalNumInputChannels == 1 && i == 1)
            buffer.copyFrom(i, 0, buffer, 0, 0, buffer.getNumSamples());
        else
            buffer.clear(i, 0, buffer.getNumSamples());
    }

    if (!betaGate.begin(buffer, getMainBusNumOutputChannels(), getLatencySamples(), betaAccess.allowed())) return;
    sanmu_beta::AudioGate::Scope betaScope { betaGate, buffer };

    // 拦截宿主异常浮点，避免污染滤波器和包络状态。
    for (int c = 0; c < buffer.getNumChannels(); ++c)
        for (int n = 0; n < buffer.getNumSamples(); ++n)
            if (!std::isfinite(buffer.getSample(c, n))) buffer.setSample(c, n, 0.0f);

    if (getSampleRate() <= 0.0)
        return;

    // ---- 从 APVTS 读取参数，按模式映射到统一压缩参数 ----
    const int mode = modeParam ? static_cast<int>(std::lround(*modeParam))
                               : Params::ModeClean;
    compressor.setMode(mode);
    const bool smooth = voxSmoothParam && voxSmoothParam->load() >= .5f;
    compressor.setVoxSmooth(smooth);
    const int latency = mode == Params::ModeVOX ? dsp_engines::VoxEngine::getLatencySamples(getSampleRate(), smooth) : 0;
    if (getLatencySamples() != latency) setLatencySamples(latency);

    float threshold = Params::Defaults::THRESHOLD;
    float ratio     = Params::Defaults::RATIO;
    float attack    = Params::Defaults::ATTACK;
    float release   = Params::Defaults::RELEASE;
    float makeup    = Params::Defaults::MAKEUP;

    switch (mode)
    {
        case Params::ModeVOX:
        {
            const float gate = voxGateParam ? static_cast<float>(*voxGateParam) : -80.0f;
            const float comp = voxCompParam ? static_cast<float>(*voxCompParam) : 0.0f;
            const float gain = voxGainParam ? static_cast<float>(*voxGainParam) : 0.0f;
            const float mix  = voxMixParam  ? static_cast<float>(*voxMixParam)  : 100.0f;
            compressor.setVoxGate(gate);
            compressor.setVoxComp(comp);
            compressor.setVoxGain(gain);
            compressor.setVoxMix(mix);
            break;
        }
        case Params::ModeVCA:
            if (vcaThresholdParam) threshold = *vcaThresholdParam;
            if (vcaRatioParam)     ratio     = *vcaRatioParam;
            if (vcaAttackParam)    attack    = *vcaAttackParam;
            if (vcaReleaseParam)   release   = *vcaReleaseParam;
            if (vcaMakeupParam)    makeup    = *vcaMakeupParam;
            break;
        case Params::ModeFET:
        {
            // 1176 独立参数：Input 驱动量、Ratio 档位、反向时间、Output 增益
            const float input = fetInputParam ? static_cast<float>(*fetInputParam) : Params::Defaults::FET_INPUT;
            const int ratioIdx = fetRatioParam
                ? static_cast<int>(std::lround(static_cast<float>(*fetRatioParam))) : 0;
            compressor.setFetDrive(input);
            compressor.setFetRatioIndex(juce::jlimit(0, 4, ratioIdx));
            attack  = fetAttackParam  ? static_cast<float>(*fetAttackParam)  : Params::Defaults::FET_ATTACK;
            release = fetReleaseParam ? static_cast<float>(*fetReleaseParam) : Params::Defaults::FET_RELEASE;
            makeup  = fetOutputParam  ? static_cast<float>(*fetOutputParam)  : Params::Defaults::FET_OUTPUT;
            break;
        }
        case Params::ModeOpto:
        {
            // LA-2A 独立参数：Peak Reduction 是侧链驱动增益，Gain 是补偿，Limit 是模式切换
            const float pr = optoPeakReductionParam
                ? static_cast<float>(*optoPeakReductionParam) : 0.0f;
            const bool isLimit = optoIsLimitParam ? (*optoIsLimitParam >= 0.5f) : false;
            compressor.setOptoPeakReduction(pr);
            compressor.setOptoIsLimit(isLimit);
            makeup  = optoGainParam ? static_cast<float>(*optoGainParam) : Params::Defaults::OPTO_GAIN;
            break;
        }
    }

    compressor.setThreshold(threshold);
    compressor.setRatio(ratio);
    compressor.setAttack(attack);
    compressor.setRelease(release);
    compressor.setMakeup(makeup);
    if (mode == Params::ModeVCA && vcaMixParam) compressor.setMix(*vcaMixParam);
    if (mode == Params::ModeFET && fetMixParam) compressor.setMix(*fetMixParam);
    if (mode == Params::ModeOpto && optoMixParam) compressor.setMix(*optoMixParam);
    if (mode >= 1 && mode <= 3)
    {
        compressor.setSidechainHp(modeSc[mode]->load());
        compressor.setSidechainEnabled(false);
        compressor.setOutputTrim(modeTrim[mode]->load());
        compressor.setStereoLink(modeLink[mode]->load());
    }
    if (mode == Params::ModeFET && headroomParam) compressor.setHeadroom(*headroomParam);
    if (globalBypassParam)
        compressor.setGlobalBypass(hostBypass || *globalBypassParam >= 0.5f);

    // ---- 从宿主同步工程 BPM 与拍号 ----
    double currentBpm = 120.0;
    int timeSigNum = 4, timeSigDen = 4;
    if (auto* playHead = getPlayHead())
    {
        if (auto pos = playHead->getPosition())
        {
            if (pos->getBpm())
                currentBpm = *pos->getBpm();
            if (pos->getTimeSignature())
            {
                timeSigNum = pos->getTimeSignature()->numerator;
                timeSigDen = pos->getTimeSignature()->denominator;
            }
        }
    }

    // ---- 处理音频 ----
    compressor.process(buffer, currentBpm, timeSigNum, timeSigDen);
}

void SanMuCompAudioProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    processAudio(buffer, true);
}

void SanMuCompAudioProcessor::releaseResources()
{
    compressor.reset();
}

// ==============================================================================
// 总线配置
// ==============================================================================

bool SanMuCompAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& mainIn  = layouts.getMainInputChannelSet();
    const auto& mainOut = layouts.getMainOutputChannelSet();

    // 1. 支持 Mono -> Mono（Pro Tools 单声道音轨/Aux 核心格式）
    if (mainIn == juce::AudioChannelSet::mono() && mainOut == juce::AudioChannelSet::mono())
        return true;

    // 2. 支持 Stereo -> Stereo（Pro Tools 立体声音轨核心格式）
    if (mainIn == juce::AudioChannelSet::stereo() && mainOut == juce::AudioChannelSet::stereo())
        return true;

    // 3. 支持 Mono -> Stereo（单声道转立体声扩展）
    if (mainIn == juce::AudioChannelSet::mono() && mainOut == juce::AudioChannelSet::stereo())
        return true;

    return false;
}

// ==============================================================================
// 状态保存/恢复
// ==============================================================================

void SanMuCompAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void SanMuCompAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml != nullptr)
    {
        if (xml->hasTagName(apvts.state.getType()))
        {
            apvts.replaceState(Params::migrateLegacyState(juce::ValueTree::fromXml(*xml)));
        }
    }
}

// ==============================================================================
// 编辑器
// ==============================================================================

juce::AudioProcessorEditor* SanMuCompAudioProcessor::createEditor()
{
    return new SanMuCompAudioProcessorEditor(*this);
}

// ==============================================================================
// 参数指针缓存
// ==============================================================================

void SanMuCompAudioProcessor::cacheParameterPointers()
{
    modeSc = { nullptr, apvts.getRawParameterValue(Params::IDs::VCA_SIDECHAIN_HP),
        apvts.getRawParameterValue(Params::IDs::FET_SIDECHAIN_HP), apvts.getRawParameterValue(Params::IDs::OPTO_SIDECHAIN_HP) };
    modeTrim = { nullptr, apvts.getRawParameterValue(Params::IDs::VCA_OUTPUT_TRIM),
        apvts.getRawParameterValue(Params::IDs::FET_OUTPUT_TRIM), apvts.getRawParameterValue(Params::IDs::OPTO_OUTPUT_TRIM) };
    modeLink = { nullptr, apvts.getRawParameterValue(Params::IDs::VCA_STEREO_LINK),
        apvts.getRawParameterValue(Params::IDs::FET_STEREO_LINK), apvts.getRawParameterValue(Params::IDs::OPTO_STEREO_LINK) };
    modeParam            = apvts.getRawParameterValue(Params::IDs::MODE);
    thresholdParam       = apvts.getRawParameterValue(Params::IDs::THRESHOLD);
    ratioParam           = apvts.getRawParameterValue(Params::IDs::RATIO);
    attackParam          = apvts.getRawParameterValue(Params::IDs::ATTACK);
    releaseParam         = apvts.getRawParameterValue(Params::IDs::RELEASE);
    kneeParam            = apvts.getRawParameterValue(Params::IDs::KNEE);
    makeupParam          = apvts.getRawParameterValue(Params::IDs::MAKEUP);
    mixParam             = apvts.getRawParameterValue(Params::IDs::MIX);
    fetMixParam          = apvts.getRawParameterValue(Params::IDs::FET_MIX);
    optoMixParam         = apvts.getRawParameterValue(Params::IDs::OPTO_MIX);
    outputTrimParam      = apvts.getRawParameterValue(Params::IDs::OUTPUT_TRIM);
    headroomParam        = apvts.getRawParameterValue(Params::IDs::HEADROOM);
    sidechainEnabledParam = apvts.getRawParameterValue(Params::IDs::SIDECHAIN_ENABLED);
    sidechainHpParam     = apvts.getRawParameterValue(Params::IDs::SIDECHAIN_HP);
    stereoLinkParam      = apvts.getRawParameterValue(Params::IDs::STEREO_LINK);
    autoGainParam        = apvts.getRawParameterValue(Params::IDs::AUTO_GAIN);
    oversampleParam      = apvts.getRawParameterValue(Params::IDs::OVERSAMPLE);
    themeParam           = apvts.getRawParameterValue(Params::IDs::THEME);
    globalBypassParam    = apvts.getRawParameterValue(Params::IDs::GLOBAL_BYPASS);

    fetInputParam        = apvts.getRawParameterValue(Params::IDs::FET_INPUT);
    fetRatioParam        = apvts.getRawParameterValue(Params::IDs::FET_RATIO);
    fetAttackParam       = apvts.getRawParameterValue(Params::IDs::FET_ATTACK);
    fetReleaseParam      = apvts.getRawParameterValue(Params::IDs::FET_RELEASE);
    fetOutputParam       = apvts.getRawParameterValue(Params::IDs::FET_OUTPUT);
    optoPeakReductionParam = apvts.getRawParameterValue(Params::IDs::OPTO_PEAK_REDUCTION);
    optoGainParam        = apvts.getRawParameterValue(Params::IDs::OPTO_GAIN);
    optoIsLimitParam     = apvts.getRawParameterValue(Params::IDs::OPTO_IS_LIMIT);

    vcaThresholdParam    = apvts.getRawParameterValue(Params::IDs::VCA_THRESHOLD);
    vcaRatioParam        = apvts.getRawParameterValue(Params::IDs::VCA_RATIO);
    vcaAttackParam       = apvts.getRawParameterValue(Params::IDs::VCA_ATTACK);
    vcaReleaseParam      = apvts.getRawParameterValue(Params::IDs::VCA_RELEASE);
    vcaMakeupParam       = apvts.getRawParameterValue(Params::IDs::VCA_MAKEUP);
    vcaScHpParam         = apvts.getRawParameterValue(Params::IDs::VCA_SIDECHAIN_HP);
    vcaMixParam          = apvts.getRawParameterValue(Params::IDs::VCA_MIX);

    voxGateParam         = apvts.getRawParameterValue(Params::IDs::VOX_GATE);
    voxCompParam         = apvts.getRawParameterValue(Params::IDs::VOX_COMP);
    voxGainParam         = apvts.getRawParameterValue(Params::IDs::VOX_GAIN);
    voxMixParam          = apvts.getRawParameterValue(Params::IDs::VOX_MIX);
    voxSmoothParam       = apvts.getRawParameterValue(Params::IDs::VOX_SMOOTH);
}

void SanMuCompAudioProcessor::notifyHostAllParameters()
{
    for (auto* param : getParameters())
    {
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*>(param))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost(p->getValue());
            p->endChangeGesture();
        }
    }
}

// ==============================================================================
// A/B 对比
// ==============================================================================

// A/B 只比较声音；主题和显示偏好沿用当前实例，不从旧声音快照恢复。
static juce::ValueTree withCurrentEditorPreferences(const juce::ValueTree& snapshot, const juce::ValueTree& current)
{
    auto restored = snapshot.createCopy();
    auto sourceTheme = current.getChildWithProperty("id", Params::IDs::THEME);
    auto targetTheme = restored.getChildWithProperty("id", Params::IDs::THEME);
    if (sourceTheme.isValid() && targetTheme.isValid())
        targetTheme.setProperty("value", sourceTheme.getProperty("value"), nullptr);
    for (int i = 0; i < current.getNumProperties(); ++i)
    {
        const auto key = current.getPropertyName(i);
        if (key.toString().startsWith("ui_") || key.toString() == "listen_target_gr")
            restored.setProperty(key, current.getProperty(key), nullptr);
    }
    return restored;
}

// 复制当前声音到另一个比较槽，不切换当前声音。
void SanMuCompAudioProcessor::copyActiveSnapshotToOther()
{
    if (activeSnapshot == ActiveSnapshot::B) snapshotA = apvts.copyState();
    else
    {
        snapshotB = apvts.copyState();
        if (activeSnapshot == ActiveSnapshot::None)
        { snapshotA = apvts.copyState(); activeSnapshot = ActiveSnapshot::A; }
    }
}

void SanMuCompAudioProcessor::clickA()
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

    if (activeSnapshot == ActiveSnapshot::B)
        snapshotB = apvts.copyState();

    if (activeSnapshot == ActiveSnapshot::None)
        snapshotA = apvts.copyState();

    if (!snapshotA.isValid())
        snapshotA = apvts.copyState();

    apvts.replaceState(withCurrentEditorPreferences(snapshotA, apvts.copyState()));
    notifyHostAllParameters();
    activeSnapshot = ActiveSnapshot::A;
}

void SanMuCompAudioProcessor::clickB()
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

// ==============================================================================
// AI 方案智能灌入
// ==============================================================================

void SanMuCompAudioProcessor::applyPresetRecommendation(const ai::CompressorPresetRecommendation& rec)
{
    // 三种格式的 JUCE 包装层在处理块时持有同一锁；短参数事务不会暴露半套方案。
    const juce::ScopedLock lock(getCallbackLock());
    undoManager.beginNewTransaction();

    auto setParam = [this](const juce::String& paramId, float value) {
        if (auto* p = apvts.getParameter(paramId))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost(p->convertTo0to1(value));
            p->endChangeGesture();
        }
    };

    // 先准备目标模式的完整参数，最后切换模式。

    // 推荐只修改选中的模式。
    if (rec.mode == Params::ModeVOX)
    {
    setParam(Params::IDs::VOX_COMP, rec.voxComp);
    setParam(Params::IDs::VOX_GATE, rec.voxGate);
    setParam(Params::IDs::VOX_GAIN, rec.voxGain);
    setParam(Params::IDs::VOX_MIX,  rec.voxMix);
    setParam(Params::IDs::VOX_SMOOTH, rec.voxSmooth ? 1.0f : 0.0f);

    }
    else if (rec.mode == Params::ModeVCA)
    {
    const float vcaRel = (rec.vcaRelease <= 0.0f) ? 2000.0f : rec.vcaRelease;
    setParam(Params::IDs::VCA_THRESHOLD,    rec.vcaThreshold);
    setParam(Params::IDs::VCA_RATIO,        rec.vcaRatio);
    setParam(Params::IDs::VCA_ATTACK,       rec.vcaAttack);
    setParam(Params::IDs::VCA_RELEASE,      vcaRel);
    setParam(Params::IDs::VCA_MAKEUP,       rec.vcaMakeup);
    setParam(Params::IDs::VCA_SIDECHAIN_HP, rec.vcaScHp);
    setParam(Params::IDs::VCA_OUTPUT_TRIM, 0.0f);
    setParam(Params::IDs::VCA_STEREO_LINK, 100.0f);
    setParam(Params::IDs::VCA_MIX,          rec.vcaMix);

    }
    else if (rec.mode == Params::ModeFET)
    {
    setParam(Params::IDs::FET_INPUT,        rec.fetInput);
    setParam(Params::IDs::FET_RATIO,        static_cast<float>(rec.fetRatioIdx));
    setParam(Params::IDs::FET_ATTACK,       rec.fetAttack);
    setParam(Params::IDs::FET_RELEASE,      rec.fetRelease);
    setParam(Params::IDs::FET_OUTPUT,       rec.fetOutput);
    setParam(Params::IDs::FET_SIDECHAIN_HP, rec.fetScHp);
    setParam(Params::IDs::FET_OUTPUT_TRIM, 0.0f);
    setParam(Params::IDs::FET_STEREO_LINK, 100.0f);
    setParam(Params::IDs::HEADROOM, 16.0f);
    setParam(Params::IDs::FET_MIX,          rec.fetMix);

    }
    else if (rec.mode == Params::ModeOpto)
    {
    setParam(Params::IDs::OPTO_PEAK_REDUCTION, rec.optoPR);
    setParam(Params::IDs::OPTO_GAIN,           rec.optoGain);
    setParam(Params::IDs::OPTO_IS_LIMIT,       rec.optoIsLimit ? 1.0f : 0.0f);
    setParam(Params::IDs::OPTO_OUTPUT_TRIM, 0.0f);
    setParam(Params::IDs::OPTO_STEREO_LINK, 100.0f);
    setParam(Params::IDs::OPTO_SIDECHAIN_HP, rec.optoScHp);
    setParam(Params::IDs::OPTO_MIX,             rec.optoMix);
    }
    setParam(Params::IDs::MODE, static_cast<float>(rec.mode));
    apvts.copyState(); // 同步刷新本次参数事务，让一次撤销完整恢复。
    undoManager.beginNewTransaction();
}

void SanMuCompAudioProcessor::loadSingleRecommendation(const ai::SingleRecommendationResponse& resp)
{
    applyPresetRecommendation(resp.preset);
    snapshotA = apvts.copyState();
    activeSnapshot = ActiveSnapshot::A;
}

void SanMuCompAudioProcessor::updateTrackProperties(const TrackProperties& properties)
{
    compressor.getAcousticExtractor().setHostTrackName(properties.name.value_or(juce::String()));
}

// ==============================================================================
// JUCE 插件入口点
// ==============================================================================

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SanMuCompAudioProcessor();
}
