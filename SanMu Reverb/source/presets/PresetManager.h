// ==============================================================================
// PresetManager.h - SanMu Reverb 专业预设管理器
// ==============================================================================

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <map>
#include <vector>

struct ReverbPreset
{
    juce::String name;
    juce::String categoryName;
    juce::String purpose;
    std::map<juce::String, float> values;
};

class PresetManager
{
public:
    static int getNumFactoryPresets();
    static const ReverbPreset& getFactoryPreset(int index);
    static juce::StringArray getFactoryPresetNames();

    static void applyPreset(const ReverbPreset& preset, juce::AudioProcessorValueTreeState& apvts);
    static ReverbPreset snapshotCurrent(const juce::String& name,
                                        const juce::String& category,
                                        juce::AudioProcessorValueTreeState& apvts);

    static juce::File getUserPresetDirectory();
    static juce::File saveUserPreset(const juce::String& name, juce::AudioProcessorValueTreeState& apvts,
                                    const juce::File& directory = getUserPresetDirectory());
    static bool loadUserPreset(const juce::File& file, juce::AudioProcessorValueTreeState& apvts);
    static juce::Array<juce::File> getUserPresetFiles(const juce::File& directory = getUserPresetDirectory());

private:
    static const std::vector<ReverbPreset>& getFactoryPresetsInternal();
};
