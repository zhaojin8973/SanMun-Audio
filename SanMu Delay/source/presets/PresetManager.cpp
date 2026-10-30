// ==============================================================================
// PresetManager.cpp - SanMu Delay 预设管理系统实现
// ==============================================================================

#include "PresetManager.h"
#include <mutex>

static std::vector<DelayPresetData> s_factoryPresets;
static juce::StringArray            s_factoryNames;
static std::once_flag               s_initFlag;

static void buildFactoryPresets()
{
    // 1. Default Settings (Modern 1/4 Note)
    {
        DelayPresetData p;
        p.name = "Default Settings";
        p.category = "Modern";
        p.engine = Params::EngineModern;
        p.syncMode = Params::SyncBpm;
        p.division = 5; // 1/4
        p.divisionR = 5; // 1/4
        p.timeLink = true;
        p.feedback = 35.0f;
        p.mix = 30.0f;
        s_factoryPresets.push_back(p);
    }
    // 2. Dotted Eighth Groove (U2 / The Edge 标志性 1/8D)
    {
        DelayPresetData p;
        p.name = "Dotted Eighth Groove";
        p.category = "Modern";
        p.engine = Params::EngineModern;
        p.syncMode = Params::SyncHost;
        p.division = 7; // 1/8D
        p.divisionR = 7;
        p.timeLink = true;
        p.feedback = 42.0f;
        p.dampHighcut = 12000.0f;
        p.mix = 35.0f;
        s_factoryPresets.push_back(p);
    }
    // 3. Vintage Tape Echo (经典磁带暖色饱和与微抖动)
    {
        DelayPresetData p;
        p.name = "Vintage Tape Echo";
        p.category = "Tape";
        p.engine = Params::EngineTape;
        p.syncMode = Params::SyncHost;
        p.division = 5; // 1/4
        p.divisionR = 5;
        p.feedback = 45.0f;
        p.tapeDrive = 40.0f;
        p.tapeWow = 30.0f;
        p.dampHighcut = 8000.0f;
        p.mix = 35.0f;
        s_factoryPresets.push_back(p);
    }
    // 4. Analog BBD Slapback (1980 复古模拟桶链短延迟)
    {
        DelayPresetData p;
        p.name = "Analog BBD Slapback";
        p.category = "BBD";
        p.engine = Params::EngineBbd;
        p.syncMode = Params::SyncFree;
        p.timeL = 110.0f;
        p.timeR = 125.0f;
        p.timeLink = false;
        p.feedback = 18.0f;
        p.bbdWarmth = 65.0f;
        p.dampHighcut = 6500.0f;
        p.mix = 40.0f;
        s_factoryPresets.push_back(p);
    }
    // 5. Ping-Pong Space Bounce (立体声左右弹跳宽声场)
    {
        DelayPresetData p;
        p.name = "Ping-Pong Space Bounce";
        p.category = "Modern";
        p.engine = Params::EngineModern;
        p.topology = Params::TopologyPingPong;
        p.syncMode = Params::SyncHost;
        p.division = 7; // 1/8D
        p.divisionR = 7;
        p.feedback = 50.0f;
        p.modRate = 0.6f;
        p.modDepth = 20.0f;
        p.mix = 40.0f;
        s_factoryPresets.push_back(p);
    }
    // 6. Diffused Ambient Cloud (水洗空灵混响化扩散回声)
    {
        DelayPresetData p;
        p.name = "Diffused Ambient Cloud";
        p.category = "Diffused";
        p.engine = Params::EngineDiffused;
        p.syncMode = Params::SyncHost;
        p.division = 4; // 1/4D
        p.divisionR = 2; // 1/2
        p.timeLink = false;
        p.feedback = 68.0f;
        p.diffusionAmount = 85.0f;
        p.dampLowcut = 150.0f;
        p.dampHighcut = 9000.0f;
        p.mix = 45.0f;
        s_factoryPresets.push_back(p);
    }
    // 7. Vocal Ducking Delay (人声避让延迟，唱时退后，停时涌现)
    {
        DelayPresetData p;
        p.name = "Vocal Ducking Delay";
        p.category = "Modern";
        p.engine = Params::EngineModern;
        p.syncMode = Params::SyncHost;
        p.division = 5; // 1/4
        p.divisionR = 5;
        p.feedback = 48.0f;
        p.duckEnable = true;
        p.duckSource = Params::DuckSourceInternal;
        p.duckThreshold = -24.0f;
        p.duckAmount = 75.0f;
        p.duckRelease = 350.0f;
        p.mix = 40.0f;
        s_factoryPresets.push_back(p);
    }
    // 8. Cross Stereo Shimmer (立体声交叉互馈梦幻声场)
    {
        DelayPresetData p;
        p.name = "Cross Stereo Shimmer";
        p.category = "Diffused";
        p.engine = Params::EngineDiffused;
        p.topology = Params::TopologyCross;
        p.syncMode = Params::SyncHost;
        p.division = 8;  // 1/8
        p.divisionR = 7; // 1/8D
        p.timeLink = false;
        p.feedback = 55.0f;
        p.diffusionAmount = 50.0f;
        p.modRate = 1.2f;
        p.modDepth = 35.0f;
        p.mix = 38.0f;
        s_factoryPresets.push_back(p);
    }
    // 9. Tape Slapback (短促磁带回声)
    {
        DelayPresetData p;
        p.name = "Tape Slapback";
        p.category = "Tape";
        p.engine = Params::EngineTape;
        p.syncMode = Params::SyncFree;
        p.timeL = 105.0f; p.timeR = 118.0f; p.timeLink = false;
        p.feedback = 12.0f; p.tapeDrive = 24.0f; p.tapeWow = 8.0f;
        p.dampHighcut = 9000.0f; p.mix = 28.0f;
        s_factoryPresets.push_back(p);
    }
    // 10. BBD Chorus Echo (带轻微调制的模拟回声)
    {
        DelayPresetData p;
        p.name = "BBD Chorus Echo";
        p.category = "BBD";
        p.engine = Params::EngineBbd;
        p.syncMode = Params::SyncHost;
        p.division = 8; p.divisionR = 8; p.timeLink = true;
        p.feedback = 32.0f; p.bbdWarmth = 72.0f;
        p.modRate = 0.8f; p.modDepth = 24.0f;
        p.dampHighcut = 7200.0f; p.mix = 34.0f;
        s_factoryPresets.push_back(p);
    }
    // 11. Diffused Plate Wash (扩散尾音氛围)
    {
        DelayPresetData p;
        p.name = "Diffused Plate Wash";
        p.category = "Diffused";
        p.engine = Params::EngineDiffused;
        p.syncMode = Params::SyncHost;
        p.division = 5; p.divisionR = 7; p.timeLink = false;
        p.feedback = 58.0f; p.diffusionAmount = 74.0f;
        p.dampLowcut = 120.0f; p.dampHighcut = 10500.0f;
        p.modRate = 0.5f; p.modDepth = 16.0f; p.mix = 42.0f;
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

std::vector<DelayPresetData>& PresetManager::getFactoryPresets()
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

const DelayPresetData& PresetManager::getFactoryPreset(int index)
{
    ensureFactoryPresetsBuilt();
    jassert(index >= 0 && index < static_cast<int>(s_factoryPresets.size()));
    return s_factoryPresets[static_cast<size_t>(index)];
}

juce::File PresetManager::getUserPresetDirectory()
{
    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                   .getChildFile("SanMu Audio/SanMu Delay/Presets");
    if (!dir.exists())
        dir.createDirectory();
    return dir;
}

juce::StringArray PresetManager::getUserPresetNames()
{
    juce::StringArray names;
    auto dir = getUserPresetDirectory();
    if (dir.isDirectory())
    {
        auto files = dir.findChildFiles(juce::File::findFiles, false, "*.delaypreset");
        for (const auto& f : files)
            names.add(f.getFileNameWithoutExtension());
    }
    return names;
}

bool PresetManager::saveUserPreset(const juce::String& name, juce::AudioProcessorValueTreeState& apvts)
{
    auto file = getUserPresetDirectory().getChildFile(name + ".delaypreset");
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    if (xml == nullptr)
        return false;
    return xml->writeTo(file);
}

bool PresetManager::loadUserPreset(const juce::String& name, juce::AudioProcessorValueTreeState& apvts)
{
    auto file = getUserPresetDirectory().getChildFile(name + ".delaypreset");
    if (!file.existsAsFile())
        return false;

    auto xml = juce::parseXML(file);
    if (xml == nullptr)
        return false;

    auto valueTree = juce::ValueTree::fromXml(*xml);
    if (!valueTree.isValid() || !valueTree.hasType(apvts.state.getType()))
        return false;

    apvts.replaceState(valueTree);
    return true;
}

bool PresetManager::deleteUserPreset(const juce::String& name)
{
    auto file = getUserPresetDirectory().getChildFile(name + ".delaypreset");
    return file.deleteFile();
}

static void setApvtsParam(juce::AudioProcessorValueTreeState& apvts, const juce::String& id, float value)
{
    if (auto* param = apvts.getParameter(id))
        param->setValueNotifyingHost(param->convertTo0to1(value));
}

void PresetManager::applyPreset(const DelayPresetData& p, juce::AudioProcessorValueTreeState& apvts)
{
    setApvtsParam(apvts, Params::IDs::engine,           static_cast<float>(p.engine));
    setApvtsParam(apvts, Params::IDs::sync_mode,        static_cast<float>(p.syncMode));
    setApvtsParam(apvts, Params::IDs::manual_bpm,       p.manualBpm);
    setApvtsParam(apvts, Params::IDs::division,         static_cast<float>(p.division));
    setApvtsParam(apvts, Params::IDs::division_r,       static_cast<float>(p.divisionR));
    setApvtsParam(apvts, Params::IDs::time_l,           p.timeL);
    setApvtsParam(apvts, Params::IDs::time_r,           p.timeR);
    setApvtsParam(apvts, Params::IDs::time_link,        p.timeLink ? 1.0f : 0.0f);
    setApvtsParam(apvts, Params::IDs::feedback,         p.feedback);
    setApvtsParam(apvts, Params::IDs::topology,         static_cast<float>(p.topology));

    setApvtsParam(apvts, Params::IDs::damp_lowcut,      p.dampLowcut);
    setApvtsParam(apvts, Params::IDs::damp_highcut,     p.dampHighcut);

    setApvtsParam(apvts, Params::IDs::mix,              p.mix);
    setApvtsParam(apvts, Params::IDs::output_gain,      p.outputGain);
    setApvtsParam(apvts, Params::IDs::freeze,           p.freeze ? 1.0f : 0.0f);

    setApvtsParam(apvts, Params::IDs::duck_enable,      p.duckEnable ? 1.0f : 0.0f);
    setApvtsParam(apvts, Params::IDs::duck_source,      static_cast<float>(p.duckSource));
    setApvtsParam(apvts, Params::IDs::duck_threshold,   p.duckThreshold);
    setApvtsParam(apvts, Params::IDs::duck_amount,      p.duckAmount);
    setApvtsParam(apvts, Params::IDs::duck_release,     p.duckRelease);

    setApvtsParam(apvts, Params::IDs::mod_rate,         p.modRate);
    setApvtsParam(apvts, Params::IDs::mod_depth,        p.modDepth);

    setApvtsParam(apvts, Params::IDs::tape_drive,       p.tapeDrive);
    setApvtsParam(apvts, Params::IDs::tape_wow,         p.tapeWow);
    setApvtsParam(apvts, Params::IDs::bbd_warmth,       p.bbdWarmth);
    setApvtsParam(apvts, Params::IDs::diffusion_amount, p.diffusionAmount);
}
