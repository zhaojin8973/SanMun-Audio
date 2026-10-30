// ==============================================================================
// PresetManager.cpp - 预设管理系统实现
// ==============================================================================

#include "PresetManager.h"
#include "../params/UpgradeState.h"

#include <vector>
#include <array>
#include <mutex>

// ==============================================================================
// 工厂预设数据（懒加载 + 线程安全初始化）
// ==============================================================================

static std::vector<PresetData> s_factoryPresets;
static juce::StringArray       s_factoryNames;
static std::once_flag          s_initFlag;

static PresetData createDefaultPreset()
{
    PresetData p;
    p.inputGainL     = 0.0f;
    p.inputGainR     = 0.0f;
    p.outputGainL    = 0.0f;
    p.outputGainR    = 0.0f;
    p.phaseInvert    = false;
    p.processingMode = 0;
    p.globalBypass   = false;
    p.autoGain       = false;

    for (int i = 0; i < Params::NUM_BANDS; ++i)
    {
        p.present[i]   = (i < Params::LEGACY_NUM_BANDS);
        p.enabled[i]   = (i < Params::LEGACY_NUM_BANDS) ? Params::Defaults::ENABLED : false;
        p.type[i]      = Params::Defaults::TYPES[i];
        p.freq[i]      = Params::Defaults::FREQUENCIES[i];
        p.gain[i]      = Params::Defaults::GAIN;
        p.q[i]         = Params::Defaults::Q;
        p.slope[i]     = 0; // 12 dB/oct
        p.placement[i] = Params::PlacementStereo;
        p.solo[i]      = false;
    }
    return p;
}

static void buildFactoryPresets()
{
    // ---- 预设 0：默认（平坦） ----
    {
        PresetData p = createDefaultPreset();
        p.name = "Default";
        s_factoryPresets.push_back(p);
    }

    // ---- 预设 1：人声增强（Vocal Enhancer） ----
    {
        PresetData p = createDefaultPreset();
        p.name = "Vocal Enhancer";

        // Band 1: Low Cut @ 80Hz, 去除低频胸腔共鸣噪声
        p.enabled[0] = true;  p.type[0] = Params::LowCut;   p.freq[0] = 80.0f;   p.gain[0] = 0.0f;  p.q[0] = 0.71f;  p.solo[0] = false;
        // Band 2: Peak @ 200Hz, -2dB, 减少浑浊感
        p.enabled[1] = true;  p.type[1] = Params::Peak;     p.freq[1] = 200.0f;  p.gain[1] = -2.0f; p.q[1] = 1.0f;   p.solo[1] = false;
        // Band 3: Peak @ 2.5kHz, +4dB, 增加清晰度和临场感
        p.enabled[2] = true;  p.type[2] = Params::Peak;     p.freq[2] = 2500.0f; p.gain[2] = 4.0f;  p.q[2] = 1.0f;   p.solo[2] = false;
        // Band 4: Peak @ 5kHz, +2dB, 增加亮度
        p.enabled[3] = true;  p.type[3] = Params::Peak;     p.freq[3] = 5000.0f; p.gain[3] = 2.0f;  p.q[3] = 1.5f;   p.solo[3] = false;
        // Band 5: High Shelf @ 8kHz, +3dB, 空气感
        p.enabled[4] = true;  p.type[4] = Params::HighShelf; p.freq[4] = 8000.0f; p.gain[4] = 3.0f;  p.q[4] = 0.71f;  p.solo[4] = false;
        // Band 6: High Cut @ 16kHz, 轻微柔化
        p.enabled[5] = true;  p.type[5] = Params::HighCut;  p.freq[5] = 16000.0f; p.gain[5] = 0.0f;  p.q[5] = 0.71f;  p.solo[5] = false;

        s_factoryPresets.push_back(p);
    }

    // ---- 预设 2：低音收紧（Bass Tight） ----
    {
        PresetData p = createDefaultPreset();
        p.name = "Bass Tight";

        // Band 1: Low Cut @ 30Hz, 切除次声频
        p.enabled[0] = true;  p.type[0] = Params::LowCut;   p.freq[0] = 30.0f;   p.gain[0] = 0.0f;  p.q[0] = 0.71f;  p.solo[0] = false;
        // Band 2: Peak @ 60Hz, +3dB, 增强低频冲击力
        p.enabled[1] = true;  p.type[1] = Params::Peak;     p.freq[1] = 60.0f;   p.gain[1] = 3.0f;  p.q[1] = 0.5f;   p.solo[1] = false;
        // Band 3: Peak @ 120Hz, -2dB, 减少共鸣嗡嗡声
        p.enabled[2] = true;  p.type[2] = Params::Peak;     p.freq[2] = 120.0f;  p.gain[2] = -2.0f; p.q[2] = 1.5f;   p.solo[2] = false;
        // Band 4: Peak @ 500Hz, -1dB, 减少箱体感
        p.enabled[3] = true;  p.type[3] = Params::Peak;     p.freq[3] = 500.0f;  p.gain[3] = -1.0f; p.q[3] = 1.0f;   p.solo[3] = false;
        // Band 5: Peak @ 1.5kHz, +2dB, 增加手指拨弦清晰度
        p.enabled[4] = true;  p.type[4] = Params::Peak;     p.freq[4] = 1500.0f; p.gain[4] = 2.0f;  p.q[4] = 1.5f;   p.solo[4] = false;
        // Band 6: High Shelf @ 5kHz, -2dB, 减少手指噪音
        p.enabled[5] = true;  p.type[5] = Params::HighShelf; p.freq[5] = 5000.0f; p.gain[5] = -2.0f; p.q[5] = 0.71f;  p.solo[5] = false;

        s_factoryPresets.push_back(p);
    }

    // ---- 预设 3：空气感（Air & Shine） ----
    {
        PresetData p = createDefaultPreset();
        p.name = "Air & Shine";
        p.outputGainL = p.outputGainR = -1.0f;  // 稍降输出以防高频过载

        // Band 1: Low Cut @ 40Hz, 清理次低频
        p.enabled[0] = true;  p.type[0] = Params::LowCut;   p.freq[0] = 40.0f;   p.gain[0] = 0.0f;  p.q[0] = 0.71f;  p.solo[0] = false;
        // Band 2: Low Shelf @ 100Hz, +1dB, 保留一点温暖感
        p.enabled[1] = true;  p.type[1] = Params::LowShelf;  p.freq[1] = 100.0f;  p.gain[1] = 1.0f;  p.q[1] = 0.71f;  p.solo[1] = false;
        // Band 3: Peak @ 3kHz, +2dB, 临场感
        p.enabled[2] = true;  p.type[2] = Params::Peak;     p.freq[2] = 3000.0f; p.gain[2] = 2.0f;  p.q[2] = 1.2f;   p.solo[2] = false;
        // Band 4: Peak @ 8kHz, +4dB, 提升亮度
        p.enabled[3] = true;  p.type[3] = Params::Peak;     p.freq[3] = 8000.0f; p.gain[3] = 4.0f;  p.q[3] = 0.8f;   p.solo[3] = false;
        // Band 5: High Shelf @ 12kHz, +5dB, 空气感
        p.enabled[4] = true;  p.type[4] = Params::HighShelf; p.freq[4] = 12000.0f; p.gain[4] = 5.0f;  p.q[4] = 0.71f;  p.solo[4] = false;
        // Band 6: Peak @ 15kHz, +1dB, 极高频润色
        p.enabled[5] = true;  p.type[5] = Params::Peak;     p.freq[5] = 15000.0f; p.gain[5] = 1.0f;  p.q[5] = 2.0f;   p.solo[5] = false;

        s_factoryPresets.push_back(p);
    }

    // ---- 预设 4：去浑浊（De-Mud） ----
    {
        PresetData p = createDefaultPreset();
        p.name = "De-Mud";

        // Band 1: Low Cut @ 50Hz
        p.enabled[0] = true;  p.type[0] = Params::LowCut;   p.freq[0] = 50.0f;   p.gain[0] = 0.0f;  p.q[0] = 0.71f;  p.solo[0] = false;
        // Band 2: Low Shelf @ 150Hz, +2dB, 增加低频温暖感
        p.enabled[1] = true;  p.type[1] = Params::LowShelf;  p.freq[1] = 150.0f;  p.gain[1] = 2.0f;  p.q[1] = 0.71f;  p.solo[1] = false;
        // Band 3: Peak @ 250Hz, -4dB, 宽Q去除浑浊核心
        p.enabled[2] = true;  p.type[2] = Params::Peak;     p.freq[2] = 250.0f;  p.gain[2] = -4.0f; p.q[2] = 0.7f;   p.solo[2] = false;
        // Band 4: Peak @ 500Hz, -1.5dB, 减少鼻腔感
        p.enabled[3] = true;  p.type[3] = Params::Peak;     p.freq[3] = 500.0f;  p.gain[3] = -1.5f; p.q[3] = 1.5f;   p.solo[3] = false;
        // Band 5: Peak @ 4kHz, +3dB, 提升清晰度
        p.enabled[4] = true;  p.type[4] = Params::Peak;     p.freq[4] = 4000.0f; p.gain[4] = 3.0f;  p.q[4] = 1.2f;   p.solo[4] = false;
        // Band 6: High Shelf @ 10kHz, +2dB, 开放式高频
        p.enabled[5] = true;  p.type[5] = Params::HighShelf; p.freq[5] = 10000.0f; p.gain[5] = 2.0f;  p.q[5] = 0.71f;  p.solo[5] = false;

        s_factoryPresets.push_back(p);
    }

    // ---- 预设 5：架子鼓总线（Drum Bus） ----
    {
        PresetData p = createDefaultPreset();
        p.name = "Drum Bus";
        p.outputGainL = p.outputGainR = -2.0f;  // 架子鼓通常响度大，留余量

        // Band 1: Low Cut @ 25Hz
        p.enabled[0] = true;  p.type[0] = Params::LowCut;   p.freq[0] = 25.0f;   p.gain[0] = 0.0f;  p.q[0] = 0.71f;  p.solo[0] = false;
        // Band 2: Peak @ 60Hz, +4dB, 底鼓冲击力
        p.enabled[1] = true;  p.type[1] = Params::Peak;     p.freq[1] = 60.0f;   p.gain[1] = 4.0f;  p.q[1] = 0.5f;   p.solo[1] = false;
        // Band 3: Peak @ 200Hz, -3dB, 减少鼓面共鸣
        p.enabled[2] = true;  p.type[2] = Params::Peak;     p.freq[2] = 200.0f;  p.gain[2] = -3.0f; p.q[2] = 1.0f;   p.solo[2] = false;
        // Band 4: Peak @ 800Hz, -2dB, 减少箱体共鸣
        p.enabled[3] = true;  p.type[3] = Params::Peak;     p.freq[3] = 800.0f;  p.gain[3] = -2.0f; p.q[3] = 1.5f;   p.solo[3] = false;
        // Band 5: Peak @ 5kHz, +2dB, 军鼓清晰度
        p.enabled[4] = true;  p.type[4] = Params::Peak;     p.freq[4] = 5000.0f; p.gain[4] = 2.0f;  p.q[4] = 1.2f;   p.solo[4] = false;
        // Band 6: High Shelf @ 8kHz, +3dB, 镲片空气感
        p.enabled[5] = true;  p.type[5] = Params::HighShelf; p.freq[5] = 8000.0f; p.gain[5] = 3.0f;  p.q[5] = 0.71f;  p.solo[5] = false;

        s_factoryPresets.push_back(p);
    }

    // ---- 预设 6：母带润色（Master Polish） ----
    {
        PresetData p = createDefaultPreset();
        p.name = "Master Polish";

        // Band 1: Low Cut @ 20Hz
        p.enabled[0] = true;  p.type[0] = Params::LowCut;   p.freq[0] = 20.0f;   p.gain[0] = 0.0f;  p.q[0] = 0.71f;  p.solo[0] = false;
        // Band 2: Low Shelf @ 80Hz, +1dB, 轻微温暖
        p.enabled[1] = true;  p.type[1] = Params::LowShelf;  p.freq[1] = 80.0f;   p.gain[1] = 1.0f;  p.q[1] = 0.5f;   p.solo[1] = false;
        // Band 3: Peak @ 400Hz, +0.5dB, 轻微增加饱满度
        p.enabled[2] = true;  p.type[2] = Params::Peak;     p.freq[2] = 400.0f;  p.gain[2] = 0.5f;  p.q[2] = 0.5f;   p.solo[2] = false;
        // Band 4: Peak @ 2kHz, +1dB, 临场感
        p.enabled[3] = true;  p.type[3] = Params::Peak;     p.freq[3] = 2000.0f; p.gain[3] = 1.0f;  p.q[3] = 0.7f;   p.solo[3] = false;
        // Band 5: Peak @ 6kHz, +1.5dB, 亮度
        p.enabled[4] = true;  p.type[4] = Params::Peak;     p.freq[4] = 6000.0f; p.gain[4] = 1.5f;  p.q[4] = 0.7f;   p.solo[4] = false;
        // Band 6: High Shelf @ 14kHz, +1dB, 空气感
        p.enabled[5] = true;  p.type[5] = Params::HighShelf; p.freq[5] = 14000.0f; p.gain[5] = 1.0f;  p.q[5] = 0.71f;  p.solo[5] = false;

        s_factoryPresets.push_back(p);
    }

    // ---- 预设 7：电话效果（Telephone / Lo-Fi） ----
    {
        PresetData p = createDefaultPreset();
        p.name = "Telephone";
        p.outputGainL = p.outputGainR = 2.0f;

        // Band 1: High Cut @ 4kHz, 切除高频模拟电话带宽
        p.enabled[0] = true;  p.type[0] = Params::HighCut;  p.freq[0] = 4000.0f; p.gain[0] = 0.0f;  p.q[0] = 0.71f;  p.solo[0] = false;
        // Band 2: Low Cut @ 400Hz
        p.enabled[1] = true;  p.type[1] = Params::LowCut;   p.freq[1] = 400.0f;  p.gain[1] = 0.0f;  p.q[1] = 0.71f;  p.solo[0] = false;
        // Band 3: Peak @ 1kHz, +4dB, 中频推前
        p.enabled[2] = true;  p.type[2] = Params::Peak;     p.freq[2] = 1000.0f; p.gain[2] = 4.0f;  p.q[2] = 0.7f;   p.solo[2] = false;
        // Band 4: Peak @ 2kHz, +3dB
        p.enabled[3] = true;  p.type[3] = Params::Peak;     p.freq[3] = 2000.0f; p.gain[3] = 3.0f;  p.q[3] = 0.8f;   p.solo[3] = false;
        // Band 5-6 关闭
        p.enabled[4] = false; p.type[4] = Params::Peak;     p.freq[4] = 6000.0f; p.gain[4] = 0.0f;  p.q[4] = 1.0f;   p.solo[4] = false;
        p.enabled[5] = false; p.type[5] = Params::HighShelf; p.freq[5] = 10000.0f; p.gain[5] = 0.0f;  p.q[5] = 0.71f;  p.solo[5] = false;

        s_factoryPresets.push_back(p);
    }

    // 构建名称列表
    s_factoryNames.clear();
    for (const auto& preset : s_factoryPresets)
        s_factoryNames.add(preset.name);
}

// ==============================================================================
// 工厂预设查询
// ==============================================================================

void PresetManager::ensureFactoryPresetsBuilt()
{
    std::call_once(s_initFlag, buildFactoryPresets);
}

std::vector<PresetData>& PresetManager::getFactoryPresets()
{
    ensureFactoryPresetsBuilt();
    return s_factoryPresets;
}

int PresetManager::getNumFactoryPresets()
{
    ensureFactoryPresetsBuilt();
    return static_cast<int>(s_factoryPresets.size());
}

const juce::StringArray& PresetManager::getFactoryPresetNames()
{
    ensureFactoryPresetsBuilt();
    return s_factoryNames;
}

const PresetData& PresetManager::getFactoryPreset(int index)
{
    ensureFactoryPresetsBuilt();
    jassert(index >= 0 && index < static_cast<int>(s_factoryPresets.size()));
    return s_factoryPresets[static_cast<size_t>(index)];
}

// ==============================================================================
// 用户预设目录
// ==============================================================================

juce::File PresetManager::getUserPresetDirectory()
{
    auto dir = juce::File::getSpecialLocation(
                   juce::File::userMusicDirectory)
                   .getChildFile("SanMu EQ6")
                   .getChildFile("Presets");

    if (!dir.exists())
        dir.createDirectory();

    return dir;
}

// ==============================================================================
// 用户预设名称列表
// ==============================================================================

juce::StringArray PresetManager::getUserPresetNames()
{
    juce::StringArray names;
    auto dir = getUserPresetDirectory();

    for (const auto& file : dir.findChildFiles(
             juce::File::findFiles, false, "*.eqpreset"))
        names.add(file.getFileNameWithoutExtension());

    return names;
}

// ==============================================================================
// 保存/加载用户预设
// ==============================================================================

bool PresetManager::saveUserPreset(const juce::String& name,
                                   juce::AudioProcessorValueTreeState& apvts)
{
    auto file = getUserPresetDirectory().getChildFile(name + ".eqpreset");

    // 从 APVTS 获取完整的参数树 XML
    auto state = apvts.copyState();
    auto xml = state.createXml();
    if (xml == nullptr)
        return false;

    // 嵌入预设名称与版本号
    xml->setAttribute("presetName", name);
    xml->setAttribute("schemaVersion", "1.1");

    return xml->writeTo(file);
}

bool PresetManager::loadUserPreset(const juce::String& name,
                                   juce::AudioProcessorValueTreeState& apvts)
{
    auto file = getUserPresetDirectory().getChildFile(name + ".eqpreset");
    if (!file.existsAsFile())
        return false;

    auto xml = juce::parseXML(file);
    if (xml == nullptr)
        return false;

    auto valueTree = juce::ValueTree::fromXml(*xml);
    if (!valueTree.isValid())
        return false;

    // 检查是否为旧版 6 段预设，执行自动迁移（R08）
    juce::String schema = xml->getStringAttribute("schemaVersion", "");
    if (schema.isEmpty())
    {
        for (int i = 1; i <= Params::LEGACY_NUM_BANDS; ++i)
        {
            auto pid = Params::IDs::bandPresent(i);
            if (!valueTree.hasProperty(pid))
                valueTree.setProperty(pid, 1.0f, nullptr);
        }
        for (int i = Params::LEGACY_NUM_BANDS + 1; i <= Params::MAX_BANDS; ++i)
        {
            valueTree.setProperty(Params::IDs::bandPresent(i), 0.0f, nullptr);
            valueTree.setProperty(Params::IDs::bandEnabled(i), 0.0f, nullptr);
        }
    }

    Params::supplyUpgradeDefaults(valueTree);
    apvts.replaceState(valueTree);
    return true;
}

bool PresetManager::deleteUserPreset(const juce::String& name)
{
    auto file = getUserPresetDirectory().getChildFile(name + ".eqpreset");
    return file.deleteFile();
}

// ==============================================================================
// 从 APVTS 快照当前参数状态（完整状态，R07）
// ==============================================================================

PresetData PresetManager::snapshotFromApvts(
    juce::AudioProcessorValueTreeState& apvts)
{
    PresetData data;
    data.name = "User Preset";

    for (int i = 0; i < Params::NUM_BANDS; ++i)
    {
        int bandNum = i + 1;

        if (auto* p = apvts.getRawParameterValue(Params::IDs::bandPresent(bandNum)))
            data.present[i] = p->load() > 0.5f;
        else
            data.present[i] = (i < Params::LEGACY_NUM_BANDS);

        if (auto* p = apvts.getRawParameterValue(Params::IDs::bandEnabled(bandNum)))
            data.enabled[i] = p->load() > 0.5f;
        if (auto* p = apvts.getRawParameterValue(Params::IDs::bandType(bandNum)))
            data.type[i] = static_cast<int>(p->load());
        if (auto* p = apvts.getRawParameterValue(Params::IDs::bandFreq(bandNum)))
            data.freq[i] = p->load();
        if (auto* p = apvts.getRawParameterValue(Params::IDs::bandGain(bandNum)))
            data.gain[i] = p->load();
        if (auto* p = apvts.getRawParameterValue(Params::IDs::bandQ(bandNum)))
            data.q[i] = p->load();
        if (auto* p = apvts.getRawParameterValue(Params::IDs::bandSlope(bandNum)))
            data.slope[i] = static_cast<int>(p->load());
        if (auto* p = apvts.getRawParameterValue(Params::IDs::bandPlacement(bandNum)))
            data.placement[i] = static_cast<int>(p->load());
        if (auto* p = apvts.getRawParameterValue(Params::IDs::bandSolo(bandNum)))
            data.solo[i] = p->load() > 0.5f;
    }

    if (auto* p = apvts.getRawParameterValue(Params::IDs::INPUT_GAIN_L))  data.inputGainL  = p->load();
    if (auto* p = apvts.getRawParameterValue(Params::IDs::INPUT_GAIN_R))  data.inputGainR  = p->load();
    if (auto* p = apvts.getRawParameterValue(Params::IDs::OUTPUT_GAIN_L)) data.outputGainL = p->load();
    if (auto* p = apvts.getRawParameterValue(Params::IDs::OUTPUT_GAIN_R)) data.outputGainR = p->load();
    if (auto* p = apvts.getRawParameterValue(Params::IDs::PHASE_INVERT))  data.phaseInvert = p->load() > 0.5f;
    if (auto* p = apvts.getRawParameterValue(Params::IDs::EQ_MODE))       data.processingMode = static_cast<int>(p->load());
    if (auto* p = apvts.getRawParameterValue(Params::IDs::GLOBAL_BYPASS)) data.globalBypass = p->load() > 0.5f;
    if (auto* p = apvts.getRawParameterValue(Params::IDs::AUTO_GAIN))     data.autoGain     = p->load() > 0.5f;

    return data;
}

// ==============================================================================
// 将 PresetData 应用到 APVTS（完整声音状态恢复，R07）
// ==============================================================================

static void setApvtsParam(juce::AudioProcessorValueTreeState& apvts,
                         const juce::String& id, float value)
{
    auto* param = apvts.getParameter(id);
    if (param != nullptr)
        param->setValueNotifyingHost(param->convertTo0to1(value));
}

void PresetManager::applyPreset(const PresetData& preset,
                                juce::AudioProcessorValueTreeState& apvts)
{
    Params::markCurrentLinearState(apvts.state, apvts.undoManager);
    for (int i = 0; i < Params::NUM_BANDS; ++i)
    {
        int bandNum = i + 1;

        setApvtsParam(apvts, Params::IDs::bandPresent(bandNum),   preset.present[i] ? 1.0f : 0.0f);
        setApvtsParam(apvts, Params::IDs::bandEnabled(bandNum),   preset.enabled[i] ? 1.0f : 0.0f);
        setApvtsParam(apvts, Params::IDs::bandType(bandNum),      static_cast<float>(preset.type[i]));
        setApvtsParam(apvts, Params::IDs::bandFreq(bandNum),      preset.freq[i]);
        setApvtsParam(apvts, Params::IDs::bandGain(bandNum),      preset.gain[i]);
        setApvtsParam(apvts, Params::IDs::bandQ(bandNum),         preset.q[i]);
        setApvtsParam(apvts, Params::IDs::bandSlope(bandNum),     static_cast<float>(preset.slope[i]));
        setApvtsParam(apvts, Params::IDs::bandPlacement(bandNum), static_cast<float>(preset.placement[i]));
        setApvtsParam(apvts, Params::IDs::bandSolo(bandNum),      preset.solo[i] ? 1.0f : 0.0f);
        setApvtsParam(apvts, Params::IDs::bandDynamic(bandNum), 0);
        setApvtsParam(apvts, Params::IDs::bandDynAuto(bandNum), 1);
        setApvtsParam(apvts, Params::IDs::bandDynUpward(bandNum), 0);
        setApvtsParam(apvts, Params::IDs::bandDynRange(bandNum), 6);
        setApvtsParam(apvts, Params::IDs::bandDynThreshold(bandNum), -24);
    }

    setApvtsParam(apvts, Params::IDs::INPUT_GAIN_L,  preset.inputGainL);
    setApvtsParam(apvts, Params::IDs::INPUT_GAIN_R,  preset.inputGainR);
    setApvtsParam(apvts, Params::IDs::OUTPUT_GAIN_L, preset.outputGainL);
    setApvtsParam(apvts, Params::IDs::OUTPUT_GAIN_R, preset.outputGainR);
    setApvtsParam(apvts, Params::IDs::PHASE_INVERT,  preset.phaseInvert ? 1.0f : 0.0f);
    setApvtsParam(apvts, Params::IDs::EQ_MODE,       static_cast<float>(preset.processingMode));
    setApvtsParam(apvts, Params::IDs::GLOBAL_BYPASS, preset.globalBypass ? 1.0f : 0.0f);
    setApvtsParam(apvts, Params::IDs::AUTO_GAIN,     preset.autoGain ? 1.0f : 0.0f);
    setApvtsParam(apvts, Params::IDs::CHARACTER_MODE, 0);
}
