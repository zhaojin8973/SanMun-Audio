// ==============================================================================
// PluginProcessor.cpp - 插件音频处理器实现
// ==============================================================================

#include "PluginProcessor.h"
#include "params/UpgradeState.h"
#if !defined(SANMU_NO_EDITOR_TEST)
#include "WebEqEditor.h"
#endif

#include <cmath>

// ==============================================================================
// 构造函数 / 析构函数
// ==============================================================================

SanMuQ6AudioProcessor::SanMuQ6AudioProcessor(juce::File betaRecord)
    : juce::AudioProcessor(BusesProperties()
          .withInput("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    // 构造 APVTS：传入 *this、UndoManager、参数标识符、参数布局
    // （第三十九轮：启用撤销/重做，参数修改自动进入 undo 历史）
    , betaAccess(betaRecord),
      apvts(*this, &undoManager, "Parameters", Params::createParameterLayout())
{
    // 默认每个独立手势/输入为一条撤销事务
    undoManager.beginNewTransaction();

    // 缓存参数指针，避免在音频线程中每次查表
    cacheParameterPointers();

    Params::markCurrentLinearState(apvts.state);
    apvts.state.addListener(this);
    // 保存默认参数状态（供 A/B 对比的 B 槽位使用）
    defaultState = apvts.copyState();
}

SanMuQ6AudioProcessor::~SanMuQ6AudioProcessor() { apvts.state.removeListener(this); }

void SanMuQ6AudioProcessor::refreshLinearCompatibility()
{
    linearQuality.store(juce::jlimit(0, 2, static_cast<int>(apvts.state.getProperty(Params::LINEAR_QUALITY, 0))), std::memory_order_relaxed);
    linearLegacyMask.store(static_cast<int>(apvts.state.getProperty(Params::LINEAR_LEGACY_MASK, 0))
                           & Params::ALL_LINEAR_BANDS, std::memory_order_relaxed);
}

void SanMuQ6AudioProcessor::valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& property)
{
    if (tree == apvts.state && (property == juce::Identifier(Params::LINEAR_LEGACY_MASK) || property == juce::Identifier(Params::LINEAR_QUALITY))) refreshLinearCompatibility();
}

void SanMuQ6AudioProcessor::valueTreeRedirected(juce::ValueTree&) { refreshLinearCompatibility(); }

void SanMuQ6AudioProcessor::setLinearQuality(int quality)
{
    const int next = juce::jlimit(0, 2, quality);
    // 设置面板不是 APVTS 自动化参数，立即更新缓存，避免消息线程状态通知延迟一帧。
    linearQuality.store(next, std::memory_order_relaxed);
    apvts.state.setProperty(Params::LINEAR_QUALITY, next, &undoManager);
    updateHostDisplay(ChangeDetails{}.withNonParameterStateChanged(true));
}

void SanMuQ6AudioProcessor::enableLinearDynamicBand(int index)
{
    if (index < 0 || index >= Params::MAX_BANDS) return;
    apvts.state.setProperty(Params::LINEAR_LEGACY_MASK, getLinearLegacyMask() & ~(1 << index), &undoManager);
    updateHostDisplay(ChangeDetails{}.withNonParameterStateChanged(true));
}

// ==============================================================================
// 生命周期
// ==============================================================================

void SanMuQ6AudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    betaGate.prepare(sampleRate, samplesPerBlock, betaAccess.allowed(),
                     juce::jmax(8192, PrecisionLinearEq::latencyFor(sampleRate, 2)));
    // 初始化 DSP 管线
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels      = static_cast<juce::uint32>(getTotalNumOutputChannels());

    eqProcessor.prepare(spec);
    eqProcessor.setLinearQuality(getLinearQuality());

    // 应用 EQ 处理模式（prepare 后线性相位引擎已就绪）
    eqProcessor.setProcessingMode(eqModeParam ? static_cast<int>(std::lround(*eqModeParam)) : 0);

    // 报告延迟给 DAW（便于自动延迟补偿）
    lastReportedLatency = eqProcessor.getLatencySamples();
    setLatencySamples(lastReportedLatency);
}

void SanMuQ6AudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    if (!betaGate.begin(buffer, getMainBusNumOutputChannels(), getLatencySamples(), betaAccess.allowed())) return;
    sanmu_beta::AudioGate::Scope betaScope { betaGate, buffer };

    juce::ScopedNoDenormals noDenormals; // 防止极小数导致 CPU 峰值

    // 清除未使用的输出声道（防止输出噪声）
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // ---- 从 APVTS 读取参数并更新频段 ----
    double sampleRate = getSampleRate();
    if (sampleRate <= 0.0) return;  // 尚未初始化

    eqProcessor.setLinearQuality(getLinearQuality());
    // 模式切换先设置动态检测器采样率，再配置本块频段参数。
    eqProcessor.setProcessingMode(eqModeParam ? static_cast<int>(std::lround(*eqModeParam)) : 0);

    for (int i = 0; i < Params::NUM_BANDS; ++i)
    {
        auto& p = bandParams[static_cast<size_t>(i)];

        // getRawParameterValue 返回归一化值 [0,1] 或实际值的指针
        // 对于 AudioParameterFloat，返回的是实际值（Hz / dB / Q）
        // 对于 AudioParameterBool，返回 0.0f 或 1.0f
        // 对于 AudioParameterChoice，返回选择的索引（float）
        if (p.type && p.freq && p.gain && p.q && p.enabled)
        {
            eqProcessor.updateBand(
                i,
                static_cast<int>(*p.type),      // 滤波器类型索引
                *p.enabled >= 0.5f,             // 开关（bool → float）
                *p.freq,                        // 频率（实际 Hz）
                *p.gain,                        // 增益（实际 dB）
                *p.q,                           // Q 值
                p.solo ? *p.solo >= 0.5f : false, // 独听开关
                p.slope ? static_cast<int>(*p.slope) : 0, // 斜率 (0-3)
                p.placement ? static_cast<int>(*p.placement) : 0, // 通道分布 (0-4)
                sampleRate,
                p.present ? *p.present >= 0.5f : (i < Params::LEGACY_NUM_BANDS),
                p.dynamic && *p.dynamic >= 0.5f
                    && (eqModeParam->load() < .5f || isLinearDynamicAllowed(i)),
                p.dynRange ? p.dynRange->load() : 0.0f,
                p.dynThreshold ? p.dynThreshold->load() : -24.0f,
                p.dynAuto ? p.dynAuto->load() >= .5f : false,
                p.dynUpward && p.dynUpward->load() >= .5f
            );
        }
    }

    // 更新输入 L/R 增益（EQ 前）
    if (inputGainLParam)
        eqProcessor.setInputGainL(*inputGainLParam);
    if (inputGainRParam)
        eqProcessor.setInputGainR(*inputGainRParam);

    // 更新输出 L/R 增益（EQ 后，替代 输出增益+声像平衡）
    if (outputGainLParam)
        eqProcessor.setOutputGainL(*outputGainLParam);
    if (outputGainRParam)
        eqProcessor.setOutputGainR(*outputGainRParam);

    // 更新全局旁通与自动增益
    if (globalBypassParam)
        eqProcessor.setGlobalBypass(*globalBypassParam >= 0.5f);
    if (autoGainParam)
        eqProcessor.setAutoGain(*autoGainParam >= 0.5f);
    if (characterModeParam)
        eqProcessor.setCharacterMode(static_cast<int>(std::lround(*characterModeParam)));

    // ---- 处理音频 ----
    eqProcessor.process(buffer);

    // ---- 相位反转 ----
    if (phaseInvertParam && *phaseInvertParam >= 0.5f)
        buffer.applyGain(-1.0f);

    // ---- 延迟变化时上报 DAW（仅模式切换瞬间，避免频繁调用） ----
    // 音频回调内直接 setLatencySamples 会触发宿主重建音频图，某些宿主
    // 会静音输出（用户实测：切到 Linear Phase 后立即无声、输入表不动）；
    // 改为消息线程异步上报，消除宿主时序问题
    const int currentLatency = eqProcessor.getLatencySamples();
    if (currentLatency != lastReportedLatency.exchange(currentLatency, std::memory_order_relaxed))
    {
        triggerAsyncUpdate();
    }
}

void SanMuQ6AudioProcessor::handleAsyncUpdate()
{
    // 消息线程：安全上报延迟（音频回调外）
    setLatencySamples(lastReportedLatency.load(std::memory_order_relaxed));
}

void SanMuQ6AudioProcessor::releaseResources()
{
    eqProcessor.reset();
}

// ==============================================================================
// 总线配置
// ==============================================================================

bool SanMuQ6AudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    // 同声道数主总线：单声道插入与原有立体声插入均可加载。
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();
    return input == output
        && (input == juce::AudioChannelSet::mono() || input == juce::AudioChannelSet::stereo());
}

// ==============================================================================
// 状态保存/恢复 - 将 APVTS 参数树序列化为 XML
// ==============================================================================

void SanMuQ6AudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    // 将 APVTS 的状态序列化为 XML 字符串
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    if (xml != nullptr)
    {
        xml->setAttribute("schemaVersion", "1.1");
        copyXmlToBinary(*xml, destData);
    }
}

void SanMuQ6AudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    // 从 XML 字符串恢复 APVTS 状态
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml != nullptr)
    {
        if (xml->hasTagName(apvts.state.getType()))
        {
            auto newTree = juce::ValueTree::fromXml(*xml);
            if (newTree.isValid())
            {
                // 检查版本与旧版 6 段状态自动迁移（R08）
                juce::String schema = xml->getStringAttribute("schemaVersion", "");
                if (schema.isEmpty())
                {
                    for (int i = 1; i <= Params::LEGACY_NUM_BANDS; ++i)
                    {
                        auto pid = Params::IDs::bandPresent(i);
                        if (!newTree.hasProperty(pid))
                            newTree.setProperty(pid, 1.0f, nullptr);
                    }
                    for (int i = Params::LEGACY_NUM_BANDS + 1; i <= Params::MAX_BANDS; ++i)
                    {
                        newTree.setProperty(Params::IDs::bandPresent(i), 0.0f, nullptr);
                        newTree.setProperty(Params::IDs::bandEnabled(i), 0.0f, nullptr);
                    }
                }

                Params::supplyUpgradeDefaults(newTree);
                apvts.replaceState(newTree);
                // 参数重建后重新缓存指针
                cacheParameterPointers();
            }
        }
    }
}

// ==============================================================================
// 编辑器
// ==============================================================================

juce::AudioProcessorEditor* SanMuQ6AudioProcessor::createEditor()
{
#if defined(SANMU_NO_EDITOR_TEST)
    return nullptr;
#else
    return new SanMuEqWebEditor(*this);
#endif
}

// ==============================================================================
// 参数指针缓存
// ==============================================================================

void SanMuQ6AudioProcessor::cacheParameterPointers()
{
    for (int i = 0; i < Params::NUM_BANDS; ++i)
    {
        auto& p = bandParams[static_cast<size_t>(i)];
        p.present = apvts.getRawParameterValue(Params::IDs::bandPresent(i + 1));
        p.enabled = apvts.getRawParameterValue(Params::IDs::bandEnabled(i + 1));
        p.type    = apvts.getRawParameterValue(Params::IDs::bandType(i + 1));
        p.freq    = apvts.getRawParameterValue(Params::IDs::bandFreq(i + 1));
        p.gain    = apvts.getRawParameterValue(Params::IDs::bandGain(i + 1));
        p.q       = apvts.getRawParameterValue(Params::IDs::bandQ(i + 1));
        p.solo    = apvts.getRawParameterValue(Params::IDs::bandSolo(i + 1));
        p.slope   = apvts.getRawParameterValue(Params::IDs::bandSlope(i + 1));
        p.placement = apvts.getRawParameterValue(Params::IDs::bandPlacement(i + 1));
        p.dynamic = apvts.getRawParameterValue(Params::IDs::bandDynamic(i + 1));
        p.dynUpward = apvts.getRawParameterValue(Params::IDs::bandDynUpward(i + 1));
        p.dynAuto = apvts.getRawParameterValue(Params::IDs::bandDynAuto(i + 1));
        p.dynRange = apvts.getRawParameterValue(Params::IDs::bandDynRange(i + 1));
        p.dynThreshold = apvts.getRawParameterValue(Params::IDs::bandDynThreshold(i + 1));
    }

    inputGainLParam  = apvts.getRawParameterValue(Params::IDs::INPUT_GAIN_L);
    inputGainRParam  = apvts.getRawParameterValue(Params::IDs::INPUT_GAIN_R);
    outputGainLParam = apvts.getRawParameterValue(Params::IDs::OUTPUT_GAIN_L);
    outputGainRParam = apvts.getRawParameterValue(Params::IDs::OUTPUT_GAIN_R);
    phaseInvertParam   = apvts.getRawParameterValue(Params::IDs::PHASE_INVERT);
    eqModeParam        = apvts.getRawParameterValue(Params::IDs::EQ_MODE);
    globalBypassParam  = apvts.getRawParameterValue(Params::IDs::GLOBAL_BYPASS);
    autoGainParam      = apvts.getRawParameterValue(Params::IDs::AUTO_GAIN);
    characterModeParam = apvts.getRawParameterValue(Params::IDs::CHARACTER_MODE);
}

// ==============================================================================
// A/B 对比
// ==============================================================================

void SanMuQ6AudioProcessor::clickA()
{
    // 第三十九轮：快照调用/取消 = 一条撤销事务
    undoManager.beginNewTransaction();

    if (activeSnapshot == ActiveSnapshot::A)
    {
        // 再次点击 A → 取消，回到默认参数
        if (defaultState.isValid())
        {
            apvts.replaceState(defaultState);
            cacheParameterPointers();
        }
        activeSnapshot = ActiveSnapshot::None;
        return;
    }

    // 将当前状态保存到之前的槽位
    if (activeSnapshot == ActiveSnapshot::B)
        snapshotB = apvts.copyState();

    // 首次点击 A → 保存当前为 A
    if (!snapshotA.isValid())
        snapshotA = apvts.copyState();

    // 切换到 A 状态
    apvts.replaceState(snapshotA);
    cacheParameterPointers();
    activeSnapshot = ActiveSnapshot::A;
}

void SanMuQ6AudioProcessor::clickB()
{
    // 第三十九轮：快照调用/取消 = 一条撤销事务
    undoManager.beginNewTransaction();

    if (activeSnapshot == ActiveSnapshot::B)
    {
        // 再次点击 B → 取消，回到默认参数
        if (defaultState.isValid())
        {
            apvts.replaceState(defaultState);
            cacheParameterPointers();
        }
        activeSnapshot = ActiveSnapshot::None;
        return;
    }

    // 将当前状态保存到之前的槽位
    if (activeSnapshot == ActiveSnapshot::A)
        snapshotA = apvts.copyState();

    // 首次点击 B → 使用默认参数作为 B
    if (!snapshotB.isValid())
        snapshotB = defaultState.createCopy();

    // 切换到 B 状态
    apvts.replaceState(snapshotB);
    cacheParameterPointers();
    activeSnapshot = ActiveSnapshot::B;
}

void SanMuQ6AudioProcessor::switchToSnapshot(ActiveSnapshot target)
{
    if (target == activeSnapshot && target != ActiveSnapshot::None)
        return;

    undoManager.beginNewTransaction();

    // 先保存当前状态到当前激活槽位
    if (activeSnapshot == ActiveSnapshot::A)
        snapshotA = apvts.copyState();
    else if (activeSnapshot == ActiveSnapshot::B)
        snapshotB = apvts.copyState();

    if (target == ActiveSnapshot::A)
    {
        if (!snapshotA.isValid())
            snapshotA = apvts.copyState();
        apvts.replaceState(snapshotA);
        cacheParameterPointers();
        activeSnapshot = ActiveSnapshot::A;
    }
    else if (target == ActiveSnapshot::B)
    {
        if (!snapshotB.isValid())
            snapshotB = defaultState.isValid() ? defaultState.createCopy() : apvts.copyState();
        apvts.replaceState(snapshotB);
        cacheParameterPointers();
        activeSnapshot = ActiveSnapshot::B;
    }
    else
    {
        if (defaultState.isValid())
        {
            apvts.replaceState(defaultState);
            cacheParameterPointers();
        }
        activeSnapshot = ActiveSnapshot::None;
    }
}

void SanMuQ6AudioProcessor::copyActiveToOtherSnapshot()
{
    undoManager.beginNewTransaction();
    if (activeSnapshot == ActiveSnapshot::B)
    {
        snapshotA = apvts.copyState();
    }
    else
    {
        snapshotB = apvts.copyState();
    }
}

// ==============================================================================
// JUCE 插件入口点 - DAW 通过此函数创建插件实例
// ==============================================================================

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SanMuQ6AudioProcessor();
}
