// ==============================================================================
// PresetManager.h - SanMu Delay 预设管理系统
// 工厂预设 + 用户预设保存/加载
// ==============================================================================

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../params/Parameters.h"
#include <vector>

struct DelayPresetData
{
    juce::String name;
    juce::String category;
    int   engine           = Params::Defaults::ENGINE;
    int   syncMode         = Params::Defaults::SYNC_MODE;
    float manualBpm        = Params::Defaults::MANUAL_BPM;
    int   division         = Params::Defaults::DIVISION;
    int   divisionR        = Params::Defaults::DIVISION_R;
    float timeL            = Params::Defaults::TIME_MS;
    float timeR            = Params::Defaults::TIME_MS;
    bool  timeLink         = Params::Defaults::TIME_LINK;
    float feedback         = Params::Defaults::FEEDBACK;
    int   topology         = Params::Defaults::TOPOLOGY;

    float dampLowcut       = Params::Defaults::DAMP_LOWCUT;
    float dampHighcut      = Params::Defaults::DAMP_HIGHCUT;

    float mix              = Params::Defaults::MIX;
    float outputGain       = Params::Defaults::OUTPUT_GAIN;
    bool  freeze           = false;

    bool  duckEnable       = Params::Defaults::DUCK_ENABLE;
    int   duckSource       = Params::Defaults::DUCK_SOURCE;
    float duckThreshold    = Params::Defaults::DUCK_THRESHOLD;
    float duckAmount       = Params::Defaults::DUCK_AMOUNT;
    float duckRelease      = Params::Defaults::DUCK_RELEASE;

    float modRate          = Params::Defaults::MOD_RATE;
    float modDepth         = Params::Defaults::MOD_DEPTH;

    float tapeDrive        = Params::Defaults::TAPE_DRIVE;
    float tapeWow          = Params::Defaults::TAPE_WOW;
    float bbdWarmth        = Params::Defaults::BBD_WARMTH;
    float diffusionAmount  = Params::Defaults::DIFFUSION_AMOUNT;
};

class PresetManager
{
public:
    PresetManager() = default;

    // ---- 工厂预设 ----
    static int getNumFactoryPresets();
    static const juce::StringArray& getFactoryPresetNames();
    static const DelayPresetData& getFactoryPreset(int index);

    // ---- 用户预设（文件 I/O） ----
    static juce::File getUserPresetDirectory();
    static juce::StringArray getUserPresetNames();
    static bool saveUserPreset(const juce::String& name, juce::AudioProcessorValueTreeState& apvts);
    static bool loadUserPreset(const juce::String& name, juce::AudioProcessorValueTreeState& apvts);
    static bool deleteUserPreset(const juce::String& name);

    // ---- 应用预设 ----
    static void applyPreset(const DelayPresetData& preset, juce::AudioProcessorValueTreeState& apvts);

private:
    static void ensureFactoryPresetsBuilt();
    static std::vector<DelayPresetData>& getFactoryPresets();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetManager)
};
