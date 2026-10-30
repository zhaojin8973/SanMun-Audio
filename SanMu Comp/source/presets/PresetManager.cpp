// ==============================================================================
// PresetManager.cpp - SanMu Comp 预设管理系统实现
// ==============================================================================

#include "PresetManager.h"

#include <vector>
#include <mutex>

static std::vector<PresetData> s_factoryPresets;
static juce::StringArray       s_factoryNames;
static std::once_flag          s_initFlag;

static void buildFactoryPresets()
{
    // ==================== VOX 模式预设（对标 RVox 人声贴脸平准神器）====================
    {
        PresetData p;
        p.name = "Default Settings";
        p.mode = Params::ModeVOX;
        p.voxGate = -80.0f;
        p.voxComp = 0.0f;
        p.voxGain = 0.0f;
        p.voxMix  = 100.0f;
        s_factoryPresets.push_back(p);
    }
    {
        PresetData p;
        p.name = "Modern Pop In-Your-Face Vocal";
        p.mode = Params::ModeVOX;
        p.voxGate = -48.0f;
        p.voxComp = -9.0f;
        p.voxGain = -3.0f;
        p.voxMix  = 100.0f;
        s_factoryPresets.push_back(p);
    }
    {
        PresetData p;
        p.name = "Aggressive Trap / Rap Vocal";
        p.mode = Params::ModeVOX;
        p.voxGate = -42.0f;
        p.voxComp = -15.0f;
        p.voxGain = -6.0f;
        p.voxMix  = 100.0f;
        s_factoryPresets.push_back(p);
    }
    {
        PresetData p;
        p.name = "Intimate Acoustic Ballad";
        p.mode = Params::ModeVOX;
        p.voxGate = -54.0f;
        p.voxComp = -5.0f;
        p.voxGain = -1.5f;
        p.voxMix  = 100.0f;
        s_factoryPresets.push_back(p);
    }
    {
        PresetData p;
        p.name = "Heavy Rock Screamer";
        p.mode = Params::ModeVOX;
        p.voxGate = -36.0f;
        p.voxComp = -18.0f;
        p.voxGain = -8.0f;
        p.voxMix  = 100.0f;
        s_factoryPresets.push_back(p);
    }
    {
        PresetData p;
        p.name = "Podcast / Voiceover Leveler";
        p.mode = Params::ModeVOX;
        p.voxGate = -45.0f;
        p.voxComp = -8.0f;
        p.voxGain = -2.0f;
        p.voxMix  = 100.0f;
        s_factoryPresets.push_back(p);
    }

    // ==================== VCA 模式预设 ====================
    {
        PresetData p;
        p.name = "Default Settings";
        p.mode = Params::ModeVCA;
        p.threshold = 0.0f;
        p.ratio = 2.0f;
        p.attack = 30.0f;
        p.release = 150.0f;
        p.makeup = 0.0f;
        p.sidechainEnabled = false;
        p.sidechainHp = 0.0f;
        p.mix = 100.0f;
        s_factoryPresets.push_back(p);
    }
    {
        PresetData p;
        p.name = "Master Bus Glue";
        p.mode = Params::ModeVCA;
        p.threshold = 0.0f;
        p.ratio = 2.0f;
        p.attack = 30.0f;
        p.release = 150.0f;
        p.makeup = 0.0f;
        p.sidechainEnabled = false;
        p.sidechainHp = 0.0f;
        p.mix = 100.0f;
        s_factoryPresets.push_back(p);
    }
    {
        PresetData p;
        p.name = "Punchy Drum Bus";
        p.mode = Params::ModeVCA;
        p.threshold = -18.0f;
        p.ratio = 4.0f;
        p.attack = 10.0f;
        p.release = 100.0f;
        p.makeup = 4.0f;
        p.sidechainEnabled = true;
        p.sidechainHp = 100.0f;
        s_factoryPresets.push_back(p);
    }
    {
        PresetData p;
        p.name = "Snare Crack & Body";
        p.mode = Params::ModeVCA;
        p.threshold = -16.0f;
        p.ratio = 4.0f;
        p.attack = 30.0f;
        p.release = 100.0f;
        p.makeup = 3.0f;
        p.sidechainEnabled = true;
        p.sidechainHp = 60.0f;
        s_factoryPresets.push_back(p);
    }
    {
        PresetData p;
        p.name = "Tight Funk Bass";
        p.mode = Params::ModeVCA;
        p.threshold = -18.0f;
        p.ratio = 4.0f;
        p.attack = 10.0f;
        p.release = 150.0f;
        p.makeup = 3.5f;
        p.sidechainEnabled = true;
        p.sidechainHp = 50.0f;
        s_factoryPresets.push_back(p);
    }
    {
        PresetData p;
        p.name = "Pop Vocal Consistency";
        p.mode = Params::ModeVCA;
        p.threshold = -20.0f;
        p.ratio = 3.0f;
        p.attack = 15.0f;
        p.release = 120.0f;
        p.makeup = 5.0f;
        p.sidechainEnabled = true;
        p.sidechainHp = 100.0f;
        s_factoryPresets.push_back(p);
    }

    // ==================== FET 模式预设 ====================
    {
        PresetData p;
        p.name = "Default Settings";
        p.mode = Params::ModeFET;
        p.fetInput = 18.0f;
        p.fetRatio = 0; // 4:1
        p.fetAttack = 3.0f;
        p.fetRelease = 4.0f;
        p.fetOutput = 30.0f;
        p.headroom = 16.0f;
        p.sidechainHp = 60.0f;
        p.sidechainEnabled = true;
        p.mix = 100.0f;
        s_factoryPresets.push_back(p);
    }
    {
        PresetData p;
        p.name = "Dr. Pepper Snare 4:1";
        p.mode = Params::ModeFET;
        p.fetInput = 30.0f;
        p.fetRatio = 0; // 4:1
        p.fetAttack = 3.0f; // 约 3.4ms 经典保留鼓皮瞬态
        p.fetRelease = 5.0f; // 约 770ms 顺滑释放
        p.fetOutput = 28.0f;
        p.headroom = 16.0f;
        p.sidechainHp = 90.0f;
        p.sidechainEnabled = true;
        s_factoryPresets.push_back(p);
    }
    {
        PresetData p;
        p.name = "British All-Buttons Smasher";
        p.mode = Params::ModeFET;
        p.fetInput = 36.0f;
        p.fetRatio = 4; // All-Buttons
        p.fetAttack = 6.0f; // 极速启动压扁瞬态
        p.fetRelease = 5.0f;
        p.fetOutput = 24.0f;
        p.headroom = 12.0f;
        p.mix = 100.0f;
        s_factoryPresets.push_back(p);
    }
    {
        PresetData p;
        p.name = "Aggressive Vocal 8:1";
        p.mode = Params::ModeFET;
        p.fetInput = 30.0f;
        p.fetRatio = 1; // 8:1
        p.fetAttack = 3.5f;
        p.fetRelease = 5.5f;
        p.fetOutput = 28.0f;
        p.headroom = 16.0f;
        p.sidechainHp = 120.0f;
        p.sidechainEnabled = true;
        s_factoryPresets.push_back(p);
    }
    {
        PresetData p;
        p.name = "Slammed Drum Room (20:1)";
        p.mode = Params::ModeFET;
        p.fetInput = 38.0f;
        p.fetRatio = 3; // 20:1
        p.fetAttack = 5.0f;
        p.fetRelease = 6.5f;
        p.fetOutput = 24.0f;
        p.headroom = 14.0f;
        s_factoryPresets.push_back(p);
    }
    {
        PresetData p;
        p.name = "Parallel Heavy Crush";
        p.mode = Params::ModeFET;
        p.fetInput = 40.0f;
        p.fetRatio = 2; // 12:1
        p.fetAttack = 4.0f;
        p.fetRelease = 6.0f;
        p.fetOutput = 28.0f;
        p.mix = 40.0f; // 并联
        s_factoryPresets.push_back(p);
    }
    {
        PresetData p;
        p.name = "Punchy Bass Attack (8:1)";
        p.mode = Params::ModeFET;
        p.fetInput = 28.0f;
        p.fetRatio = 1; // 8:1
        p.fetAttack = 2.0f; // 慢启动放过贝斯拨弦瞬态
        p.fetRelease = 4.5f;
        p.fetOutput = 28.0f;
        p.sidechainHp = 90.0f;
        p.sidechainEnabled = true;
        s_factoryPresets.push_back(p);
    }

    // ==================== Opto 模式预设 ====================
    {
        PresetData p;
        p.name = "Default Settings";
        p.mode = Params::ModeOpto;
        p.optoPeakReduction = 50.0f;
        p.optoGain = 40.0f;
        p.optoIsLimit = false; // Compress
        p.sidechainEnabled = true;
        p.sidechainHp = 60.0f;
        p.mix = 100.0f;
        s_factoryPresets.push_back(p);
    }
    {
        PresetData p;
        p.name = "Classic Smooth Vocal";
        p.mode = Params::ModeOpto;
        p.optoPeakReduction = 50.0f;
        p.optoGain = 42.0f;
        p.optoIsLimit = false; // Compress
        p.mix = 100.0f;
        p.sidechainEnabled = true;
        p.sidechainHp = 100.0f;
        s_factoryPresets.push_back(p);
    }
    {
        PresetData p;
        p.name = "Warm Creamy Bass";
        p.mode = Params::ModeOpto;
        p.optoPeakReduction = 55.0f;
        p.optoGain = 44.0f;
        p.optoIsLimit = false; // Compress
        p.sidechainEnabled = true;
        p.sidechainHp = 60.0f;
        s_factoryPresets.push_back(p);
    }
    {
        PresetData p;
        p.name = "Lush Acoustic Strings";
        p.mode = Params::ModeOpto;
        p.optoPeakReduction = 38.0f;
        p.optoGain = 38.0f;
        p.optoIsLimit = false; // Compress
        p.sidechainEnabled = true;
        p.sidechainHp = 120.0f;
        s_factoryPresets.push_back(p);
    }
    {
        PresetData p;
        p.name = "Electric Piano Leveler";
        p.mode = Params::ModeOpto;
        p.optoPeakReduction = 45.0f;
        p.optoGain = 40.0f;
        p.optoIsLimit = false; // Compress
        p.sidechainEnabled = true;
        p.sidechainHp = 80.0f;
        s_factoryPresets.push_back(p);
    }
    {
        PresetData p;
        p.name = "Silky Vocal Warmth";
        p.mode = Params::ModeOpto;
        p.optoPeakReduction = 52.0f;
        p.optoGain = 43.0f;
        p.optoIsLimit = false; // Compress
        p.sidechainEnabled = true;
        p.sidechainHp = 90.0f;
        s_factoryPresets.push_back(p);
    }
    {
        PresetData p;
        p.name = "Opto Master Peak Limiter";
        p.mode = Params::ModeOpto;
        p.optoPeakReduction = 60.0f;
        p.optoGain = 45.0f;
        p.optoIsLimit = true; // Limit
        p.sidechainEnabled = true;
        p.sidechainHp = 60.0f;
        s_factoryPresets.push_back(p);
    }

    s_factoryNames.clear();
    for (const auto& p : s_factoryPresets)
        s_factoryNames.add(p.name);
}

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

std::vector<PresetData> PresetManager::getFactoryPresetsForMode(int mode)
{
    ensureFactoryPresetsBuilt();
    std::vector<PresetData> result;
    for (const auto& p : s_factoryPresets)
    {
        if (p.mode == mode)
            result.push_back(p);
    }
    return result;
}

juce::StringArray PresetManager::getFactoryPresetNamesForMode(int mode)
{
    ensureFactoryPresetsBuilt();
    juce::StringArray result;
    for (const auto& p : s_factoryPresets)
    {
        if (p.mode == mode)
            result.add(p.name);
    }
    return result;
}

const PresetData& PresetManager::getDefaultPresetForMode(int mode)
{
    ensureFactoryPresetsBuilt();
    for (const auto& p : s_factoryPresets)
    {
        if (p.mode == mode)
            return p;
    }
    return s_factoryPresets[0];
}

juce::File PresetManager::getUserPresetDirectory()
{
    auto dir = juce::File::getSpecialLocation(juce::File::userMusicDirectory)
                   .getChildFile("SanMu Comp")
                   .getChildFile("Presets");

    if (!dir.exists())
        dir.createDirectory();

    return dir;
}

juce::StringArray PresetManager::getUserPresetNames()
{
    juce::StringArray names;
    auto dir = getUserPresetDirectory();

    for (const auto& file : dir.findChildFiles(juce::File::findFiles, false, "*.comppreset"))
        names.add(file.getFileNameWithoutExtension());

    return names;
}

bool PresetManager::saveUserPreset(const juce::String& name, juce::AudioProcessorValueTreeState& apvts)
{
    auto file = getUserPresetDirectory().getChildFile(name + ".comppreset");
    auto state = apvts.copyState();
    auto xml = state.createXml();
    if (xml == nullptr)
        return false;

    xml->setAttribute("presetName", name);
    return xml->writeTo(file);
}

bool PresetManager::loadUserPreset(const juce::String& name, juce::AudioProcessorValueTreeState& apvts)
{
    auto file = getUserPresetDirectory().getChildFile(name + ".comppreset");
    if (!file.existsAsFile())
        return false;

    auto xml = juce::parseXML(file);
    if (xml == nullptr)
        return false;

    auto valueTree = juce::ValueTree::fromXml(*xml);
    if (!valueTree.isValid() || !valueTree.hasType(apvts.state.getType()))
        return false;

    apvts.replaceState(Params::migrateLegacyState(valueTree));
    return true;
}

bool PresetManager::deleteUserPreset(const juce::String& name)
{
    auto file = getUserPresetDirectory().getChildFile(name + ".comppreset");
    return file.deleteFile();
}

static void setApvtsParam(juce::AudioProcessorValueTreeState& apvts, const juce::String& id, float value)
{
    auto* param = apvts.getParameter(id);
    if (param != nullptr)
        param->setValueNotifyingHost(param->convertTo0to1(value));
}

void PresetManager::applyPreset(const PresetData& preset, juce::AudioProcessorValueTreeState& apvts)
{
    setApvtsParam(apvts, Params::IDs::MODE,                static_cast<float>(preset.mode));
    if (preset.mode == Params::ModeFET)
    {
        setApvtsParam(apvts, Params::IDs::FET_INPUT, preset.fetInput);
        setApvtsParam(apvts, Params::IDs::FET_RATIO, static_cast<float>(preset.fetRatio));
        setApvtsParam(apvts, Params::IDs::FET_ATTACK, preset.fetAttack);
        setApvtsParam(apvts, Params::IDs::FET_RELEASE, preset.fetRelease);
        setApvtsParam(apvts, Params::IDs::FET_OUTPUT, preset.fetOutput);
        setApvtsParam(apvts, Params::IDs::FET_OUTPUT_TRIM, preset.outputTrim);
        setApvtsParam(apvts, Params::IDs::FET_MIX, preset.mix);
        setApvtsParam(apvts, Params::IDs::FET_SIDECHAIN_HP, preset.sidechainHp);
        setApvtsParam(apvts, Params::IDs::FET_STEREO_LINK, preset.stereoLink);
        setApvtsParam(apvts, Params::IDs::HEADROOM, preset.headroom);
    }
    else if (preset.mode == Params::ModeOpto)
    {
        setApvtsParam(apvts, Params::IDs::OPTO_PEAK_REDUCTION, preset.optoPeakReduction);
        setApvtsParam(apvts, Params::IDs::OPTO_GAIN, preset.optoGain);
        setApvtsParam(apvts, Params::IDs::OPTO_IS_LIMIT, preset.optoIsLimit ? 1.0f : 0.0f);
        setApvtsParam(apvts, Params::IDs::OPTO_OUTPUT_TRIM, preset.outputTrim);
        setApvtsParam(apvts, Params::IDs::OPTO_MIX, preset.mix);
        setApvtsParam(apvts, Params::IDs::OPTO_SIDECHAIN_HP, preset.sidechainHp);
        setApvtsParam(apvts, Params::IDs::OPTO_STEREO_LINK, preset.stereoLink);
    }
    if (preset.mode == Params::ModeVCA)
    {
        setApvtsParam(apvts, Params::IDs::VCA_THRESHOLD,    preset.threshold);
        setApvtsParam(apvts, Params::IDs::VCA_RATIO,        preset.ratio);
        setApvtsParam(apvts, Params::IDs::VCA_ATTACK,       preset.attack);
        setApvtsParam(apvts, Params::IDs::VCA_RELEASE,      preset.release);
        setApvtsParam(apvts, Params::IDs::VCA_MAKEUP,       preset.makeup);
        setApvtsParam(apvts, Params::IDs::VCA_SIDECHAIN_HP, preset.sidechainHp);
        setApvtsParam(apvts, Params::IDs::VCA_STEREO_LINK, preset.stereoLink);
        setApvtsParam(apvts, Params::IDs::VCA_OUTPUT_TRIM, preset.outputTrim);
        setApvtsParam(apvts, Params::IDs::VCA_MIX,          preset.mix);
    }
    else if (preset.mode == Params::ModeVOX)
    {
        setApvtsParam(apvts, Params::IDs::VOX_GATE,         preset.voxGate);
        setApvtsParam(apvts, Params::IDs::VOX_COMP,         preset.voxComp);
        setApvtsParam(apvts, Params::IDs::VOX_GAIN,         preset.voxGain);
        setApvtsParam(apvts, Params::IDs::VOX_MIX,          preset.voxMix);
        setApvtsParam(apvts, Params::IDs::VOX_SMOOTH,       preset.voxSmooth ? 1.0f : 0.0f);
    }
}

PresetData PresetManager::snapshotFromApvts(juce::AudioProcessorValueTreeState& apvts)
{
    PresetData data;
    data.name = "Custom";

    auto getVal = [&](const juce::String& id) -> float {
        auto* v = apvts.getRawParameterValue(id);
        return v ? v->load() : 0.0f;
    };

    data.mode             = static_cast<int>(std::lround(getVal(Params::IDs::MODE)));
    if (data.mode == Params::ModeVCA)
    {
        data.threshold    = getVal(Params::IDs::VCA_THRESHOLD);
        data.ratio        = getVal(Params::IDs::VCA_RATIO);
        data.attack       = getVal(Params::IDs::VCA_ATTACK);
        data.release      = getVal(Params::IDs::VCA_RELEASE);
        data.makeup       = getVal(Params::IDs::VCA_MAKEUP);
        data.sidechainHp  = getVal(Params::IDs::VCA_SIDECHAIN_HP);
        data.mix          = getVal(Params::IDs::VCA_MIX);
    }
    else if (data.mode == Params::ModeVOX)
    {
        data.voxGate      = getVal(Params::IDs::VOX_GATE);
        data.voxComp      = getVal(Params::IDs::VOX_COMP);
        data.voxGain      = getVal(Params::IDs::VOX_GAIN);
        data.voxMix       = getVal(Params::IDs::VOX_MIX);
        data.voxSmooth    = getVal(Params::IDs::VOX_SMOOTH) >= .5f;
    }
    else
    {
        data.threshold    = getVal(Params::IDs::THRESHOLD);
        data.ratio        = getVal(Params::IDs::RATIO);
        data.attack       = getVal(Params::IDs::ATTACK);
        data.release      = getVal(Params::IDs::RELEASE);
        data.makeup       = getVal(Params::IDs::MAKEUP);
        data.sidechainHp  = getVal(data.mode == Params::ModeFET ? Params::IDs::FET_SIDECHAIN_HP : Params::IDs::OPTO_SIDECHAIN_HP);
        data.mix          = (data.mode == Params::ModeFET) ? getVal(Params::IDs::FET_MIX)
                         : (data.mode == Params::ModeOpto) ? getVal(Params::IDs::OPTO_MIX)
                         : getVal(Params::IDs::MIX);
    }
    data.knee             = getVal(Params::IDs::KNEE);
    data.headroom         = getVal(Params::IDs::HEADROOM);
    data.sidechainEnabled = getVal(Params::IDs::SIDECHAIN_ENABLED) >= 0.5f;
    data.stereoLink       = getVal(data.mode == Params::ModeFET ? Params::IDs::FET_STEREO_LINK : data.mode == Params::ModeOpto ? Params::IDs::OPTO_STEREO_LINK : Params::IDs::VCA_STEREO_LINK);
    data.autoGain         = false;
    data.outputTrim = data.mode == Params::ModeVOX ? 0.0f : getVal(data.mode == Params::ModeFET ? Params::IDs::FET_OUTPUT_TRIM : data.mode == Params::ModeOpto ? Params::IDs::OPTO_OUTPUT_TRIM : Params::IDs::VCA_OUTPUT_TRIM);

    data.fetInput         = getVal(Params::IDs::FET_INPUT);
    data.fetRatio         = static_cast<int>(std::lround(getVal(Params::IDs::FET_RATIO)));
    data.fetAttack        = getVal(Params::IDs::FET_ATTACK);
    data.fetRelease       = getVal(Params::IDs::FET_RELEASE);
    data.fetOutput        = getVal(Params::IDs::FET_OUTPUT);

    data.optoPeakReduction = getVal(Params::IDs::OPTO_PEAK_REDUCTION);
    data.optoGain          = getVal(Params::IDs::OPTO_GAIN);
    data.optoIsLimit       = getVal(Params::IDs::OPTO_IS_LIMIT) >= 0.5f;

    return data;
}
