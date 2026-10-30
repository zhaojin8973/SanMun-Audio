#include "PresetManager.h"
#include "HallPresets.h"
#include "PlatePresets.h"
#include "../params/Parameters.h"
#include "../params/StateCompatibility.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace
{
ReverbPreset makePreset(const juce::String& name, const juce::String& purpose,
                        const juce::String& category, int categoryIndex, int mode,
                        float decay, float predelay, float mix,
                        float highFrequency, float highShelf, float bassFrequency, float bassMultiplier,
                        float size, float attack, float early, float late,
                        float modRate, float modDepth, float lowCut, float highCut)
{
    ReverbPreset preset;
    preset.name = name;
    preset.categoryName = category;
    preset.purpose = purpose;
    preset.values[Params::IDs::CATEGORY] = static_cast<float>(categoryIndex);
    preset.values[Params::IDs::MODE] = static_cast<float>(mode);
    preset.values[Params::IDs::DECAY] = decay;
    preset.values[Params::IDs::PREDELAY] = predelay;
    preset.values[Params::IDs::MIX] = mix;
    preset.values[Params::IDs::HIGH_FREQ] = highFrequency;
    preset.values[Params::IDs::HIGH_SHELF] = highShelf;
    preset.values[Params::IDs::BASS_FREQ] = bassFrequency;
    preset.values[Params::IDs::BASS_MULT] = bassMultiplier;
    preset.values[Params::IDs::SIZE] = size;
    preset.values[Params::IDs::ATTACK] = attack;
    preset.values[Params::IDs::EARLY_DIFF] = early;
    preset.values[Params::IDs::LATE_DIFF] = late;
    preset.values[Params::IDs::MOD_RATE] = modRate;
    preset.values[Params::IDs::MOD_DEPTH] = modDepth;
    preset.values[Params::IDs::LOW_CUT] = lowCut;
    preset.values[Params::IDs::HIGH_CUT] = highCut;
    return preset;
}

void appendDefinition(std::vector<ReverbPreset>& result, const char* name, const char* purpose,
                      const char* category, int categoryIndex, int mode,
                      const std::array<float, 15>& values)
{
    result.push_back(makePreset(juce::String::fromUTF8(name), juce::String::fromUTF8(purpose), category,
        categoryIndex, mode, values[0], values[1], values[2], values[3], values[4], values[5],
        values[6], values[7], values[8], values[9], values[10], values[11], values[12], values[13], values[14]));
}
}

const std::vector<ReverbPreset>& PresetManager::getFactoryPresetsInternal()
{
    // 只展示已按当前八种模式设计的正式预设。旧架构预设保存在历史候选包中，
    // 不再混入产品菜单。插件没有将这里的序号暴露为宿主Program。
    static const std::vector<ReverbPreset> presets = []
    {
        std::vector<ReverbPreset> result;
        result.reserve(23);

        // PLATE / Modern Silk
        for (const auto& definition : plate_presets::definitions)
            appendDefinition(result, definition.name, definition.purpose, "Plate", 0, 0, definition.values);
        // PLATE / Classic Steel
        result.push_back(makePreset(juce::String::fromUTF8("经典钢板 · 基准"),
            juce::String::fromUTF8("保留颗粒与轮廓，中低频尾音温和收束。"), "Plate", 0, 1,
            2.39f, 15, 100, 6000, -18, 300, 1, 100, 30, 100, 100, .8f, 20, 20, 9000));

        // ROOM / Studio Room
        result.push_back(makePreset(juce::String::fromUTF8("录音室 · 自然基准"),
            juce::String::fromUTF8("紧凑自然的公共空间，保留音头与近处反射。"), "Room", 1, 0,
            .65f, 3, 100, 6500, -14, 250, .9f, 85, 20, 100, 100, .45f, 12, 35, 10500));
        result.push_back(makePreset(juce::String::fromUTF8("录音室 · 贴近人声"),
            juce::String::fromUTF8("短而贴近的人声空间，减少低频与齿音堆积。"), "Room", 1, 0,
            .45f, 8, 100, 6000, -16, 250, .85f, 65, 10, 100, 100, .35f, 8, 70, 9500));
        // ROOM / Live Room
        result.push_back(makePreset(juce::String::fromUTF8("现场房间 · 开阔基准"),
            juce::String::fromUTF8("更开阔的反射间距和鲜活尾音。"), "Room", 1, 1,
            1.3f, 5, 100, 6500, -12, 250, 1, 100, 35, 90, 100, .55f, 15, 35, 11000));
        result.push_back(makePreset(juce::String::fromUTF8("现场房间 · 鼓组空间"),
            juce::String::fromUTF8("保留鼓组瞬态与宽度，控制房间金属感。"), "Room", 1, 1,
            .85f, 2, 100, 7000, -12, 250, .9f, 80, 10, 75, 100, .5f, 10, 55, 11000));

        // HALL / Concert Hall 与 Cathedral
        for (const auto& definition : hall_presets::definitions)
            appendDefinition(result, definition.name, definition.purpose, "Hall", 2, definition.mode, definition.values);

        // CHAMBER / Velvet Chamber
        result.push_back(makePreset(juce::String::fromUTF8("柔暖回声室 · 自然基准"),
            juce::String::fromUTF8("柔暖细密的回声室基准，适合先判断整体空间。"), "Chamber", 3, 0,
            2.1f, 15, 100, 6000, -16, 280, 1.1f, 100, 35, 100, 100, .6f, 20, 35, 9500));
        result.push_back(makePreset(juce::String::fromUTF8("柔暖回声室 · 人声包裹"),
            juce::String::fromUTF8("让人声后方形成柔和包裹，保留字头。"), "Chamber", 3, 0,
            1.85f, 28, 100, 5800, -18, 260, 1, 90, 42, 100, 100, .45f, 15, 80, 8800));
        result.push_back(makePreset(juce::String::fromUTF8("柔暖回声室 · 弦乐余韵"),
            juce::String::fromUTF8("较慢展开的连续尾声，融合弦乐声部。"), "Chamber", 3, 0,
            2.8f, 18, 100, 6200, -16, 280, 1.15f, 100, 65, 100, 100, .4f, 22, 50, 9500));
        // CHAMBER / Stone Chamber
        result.push_back(makePreset(juce::String::fromUTF8("石质回声室 · 开阔基准"),
            juce::String::fromUTF8("清晰墙面轮廓与更大的反射跨度。"), "Chamber", 3, 1,
            2.3f, 12, 100, 7200, -12, 280, 1.05f, 100, 30, 90, 100, .55f, 15, 45, 11000));
        result.push_back(makePreset(juce::String::fromUTF8("石质回声室 · 军鼓轮廓"),
            juce::String::fromUTF8("短尾、鲜明起音和受控的鼓身。"), "Chamber", 3, 1,
            1.05f, 8, 100, 7500, -12, 240, .85f, 80, 15, 80, 95, .7f, 12, 100, 11000));
        result.push_back(makePreset(juce::String::fromUTF8("石质回声室 · 钢琴空间"),
            juce::String::fromUTF8("在敲击轮廓后展开清晰、稳定的空间。"), "Chamber", 3, 1,
            1.75f, 18, 100, 6500, -14, 260, 1, 95, 45, 100, 100, .4f, 12, 65, 10000));
        return result;
    }();
    return presets;
}

int PresetManager::getNumFactoryPresets()
{
    return static_cast<int>(getFactoryPresetsInternal().size());
}

const ReverbPreset& PresetManager::getFactoryPreset(int index)
{
    const auto& presets = getFactoryPresetsInternal();
    return presets[static_cast<std::size_t>(juce::jlimit(0, static_cast<int>(presets.size()) - 1, index))];
}

juce::StringArray PresetManager::getFactoryPresetNames()
{
    juce::StringArray names;
    for (const auto& preset : getFactoryPresetsInternal()) names.add(preset.name);
    return names;
}

void PresetManager::applyPreset(const ReverbPreset& preset, juce::AudioProcessorValueTreeState& apvts)
{
    for (const auto& [id, value] : preset.values)
        if (auto* parameter = apvts.getParameter(id))
            parameter->setValueNotifyingHost(parameter->getNormalisableRange().convertTo0to1(value));
}

ReverbPreset PresetManager::snapshotCurrent(const juce::String& name, const juce::String& category,
                                            juce::AudioProcessorValueTreeState& apvts)
{
    ReverbPreset preset;
    preset.name = name;
    preset.categoryName = category;
    for (const auto& id : Params::getAllKnobParamIds())
        if (auto* raw = apvts.getRawParameterValue(id)) preset.values[id] = raw->load();
    if (auto* raw = apvts.getRawParameterValue(Params::IDs::CATEGORY)) preset.values[Params::IDs::CATEGORY] = raw->load();
    if (auto* raw = apvts.getRawParameterValue(Params::IDs::MODE)) preset.values[Params::IDs::MODE] = raw->load();
    return preset;
}

juce::File PresetManager::getUserPresetDirectory()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("SanMu Audio").getChildFile("SanMu Reverb").getChildFile("Presets");
}

juce::File PresetManager::saveUserPreset(const juce::String& name, juce::AudioProcessorValueTreeState& apvts,
                                         const juce::File& directory)
{
    const auto safeName = juce::File::createLegalFileName(name.trim()).substring(0, 80);
    if (safeName.isEmpty() || !directory.createDirectory()) return {};
    const auto file = directory.getNonexistentChildFile(safeName, ".sanmureverb", true);
    auto state = apvts.copyState();
    state_compatibility::markCurrent(state);
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    return xml != nullptr && xml->writeTo(file) ? file : juce::File();
}

bool PresetManager::loadUserPreset(const juce::File& file, juce::AudioProcessorValueTreeState& apvts)
{
    if (!file.existsAsFile()) return false;
    const auto xml = juce::XmlDocument::parse(file);
    if (xml == nullptr) return false;
    auto state = juce::ValueTree::fromXml(*xml);
    if (!state_compatibility::isValid(state, apvts)) return false;
    state_compatibility::migrate(state);
    apvts.replaceState(state);
    return true;
}

juce::Array<juce::File> PresetManager::getUserPresetFiles(const juce::File& directory)
{
    auto files = directory.findChildFiles(juce::File::findFiles, false, "*.sanmureverb");
    std::sort(files.begin(), files.end(), [](const auto& a, const auto& b)
    { return a.getFileName().compareNatural(b.getFileName()) < 0; });
    return files;
}
