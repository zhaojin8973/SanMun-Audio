// ==============================================================================
// PresetManager.h - SanMu Comp 预设管理系统
// 工厂预设 + 用户预设保存/加载
// ==============================================================================

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../params/Parameters.h"

#include <vector>

// ==============================================================================
// 单个压缩器预设数据快照
// ==============================================================================
struct PresetData
{
    juce::String name;

    int   mode               = Params::ModeVCA;
    float threshold          = Params::Defaults::THRESHOLD;
    float ratio              = Params::Defaults::RATIO;
    float attack             = Params::Defaults::ATTACK;
    float release            = Params::Defaults::RELEASE;
    float knee               = Params::Defaults::KNEE;
    float makeup             = Params::Defaults::MAKEUP;
    float mix                = Params::Defaults::MIX;
    float outputTrim = 0.0f;
    float headroom           = Params::Defaults::HEADROOM;

    bool  sidechainEnabled   = false;
    float sidechainHp        = Params::Defaults::SIDECHAIN_HP;
    float stereoLink         = Params::Defaults::STEREO_LINK;
    bool  autoGain           = false;

    // FET
    float fetInput           = Params::Defaults::FET_INPUT;
    int   fetRatio           = 0;
    float fetAttack          = Params::Defaults::FET_ATTACK;
    float fetRelease         = Params::Defaults::FET_RELEASE;
    float fetOutput          = Params::Defaults::FET_OUTPUT;

    // Opto
    float optoPeakReduction  = Params::Defaults::OPTO_PEAK_REDUCTION;
    float optoGain           = Params::Defaults::OPTO_GAIN;
    bool  optoIsLimit        = Params::Defaults::OPTO_IS_LIMIT;

    // VOX
    float voxGate            = -80.0f;
    float voxComp            = 0.0f;
    float voxGain            = 0.0f;
    float voxMix             = 100.0f;
    bool  voxSmooth          = false;
};

// ==============================================================================
// PresetManager
// ==============================================================================
class PresetManager
{
public:
    PresetManager() = default;

    // ---- 工厂预设 ----
    static int getNumFactoryPresets();
    static const juce::StringArray& getFactoryPresetNames();
    static const PresetData& getFactoryPreset(int index);

    // ---- 按模式分类工厂预设 ----
    static std::vector<PresetData> getFactoryPresetsForMode(int mode);
    static juce::StringArray getFactoryPresetNamesForMode(int mode);
    static const PresetData& getDefaultPresetForMode(int mode);

    // ---- 用户预设（文件 I/O） ----
    static juce::File getUserPresetDirectory();
    static juce::StringArray getUserPresetNames();
    static bool saveUserPreset(const juce::String& name, juce::AudioProcessorValueTreeState& apvts);
    static bool loadUserPreset(const juce::String& name, juce::AudioProcessorValueTreeState& apvts);
    static bool deleteUserPreset(const juce::String& name);

    // ---- 应用预设 ----
    static void applyPreset(const PresetData& preset, juce::AudioProcessorValueTreeState& apvts);
    static PresetData snapshotFromApvts(juce::AudioProcessorValueTreeState& apvts);

private:
    static void ensureFactoryPresetsBuilt();
    static std::vector<PresetData>& getFactoryPresets();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetManager)
};
