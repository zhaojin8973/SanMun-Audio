// ==============================================================================
// Parameters.cpp - SanMu Reverb 15 旋钮全物理参数工业级闭环实现
// ==============================================================================

#include "Parameters.h"
#include <cmath>
#include <regex>

namespace
{
// 宿主文本输入和旋钮共用物理单位解析；无效文本保留当前参数。
class PhysicalAudioParameter final : public juce::AudioParameterFloat
{
public:
    PhysicalAudioParameter(const juce::String& id, const juce::String& name,
                           juce::NormalisableRange<float> valueRange, float defaultValue,
                           const juce::String& unitIn, int decimals)
        : juce::AudioParameterFloat(juce::ParameterID { id, 1 }, name, valueRange, defaultValue,
              juce::AudioParameterFloatAttributes().withLabel(" " + unitIn)
                  .withStringFromValueFunction([unitIn, decimals](float value, int) {
                      return juce::String(value, decimals) + " " + unitIn;
                  })), unit(unitIn.toLowerCase()) {}

    float getValueForText(const juce::String& text) const override
    {
        // 只接受完整的十进制数字及可选单位，避免把错误文本拼接成另一个数值。
        static const std::regex pattern(R"(^([+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+))\s*(.*)$)");
        std::smatch match;
        const auto input = text.trim().toStdString();
        const auto unchanged = convertTo0to1(get());
        if (!std::regex_match(input, match, pattern)) return unchanged;
        const auto suffix = juce::String::fromUTF8(match[2].str().c_str()).trim().toLowerCase();
        double scale = 1.0;
        if (suffix.isNotEmpty() && suffix != unit)
        {
            const auto is = [&suffix](const char* value) { return suffix == juce::String::fromUTF8(value); };
            if (unit == "hz" && (is("khz") || is("千赫兹") || is("千赫"))) scale = 1000.0;
            else if (unit == "hz" && is("赫兹")) {}
            else if (unit == "s" && (is("ms") || is("毫秒"))) scale = .001;
            else if (unit == "s" && is("秒")) {}
            else if (unit == "ms" && (is("s") || is("秒"))) scale = 1000.0;
            else if (unit == "ms" && is("毫秒")) {}
            else if (unit == "db" && is("分贝")) {}
            else if (unit == "x" && (is("倍") || is("×"))) {}
            else return unchanged;
        }
        const double value = juce::String(match[1].str()).getDoubleValue() * scale;
        if (!std::isfinite(value)) return unchanged;
        const auto& limits = getNormalisableRange();
        return convertTo0to1(static_cast<float>(juce::jlimit(
            static_cast<double>(limits.start), static_cast<double>(limits.end), value)));
    }

private:
    const juce::String unit;
};
}

namespace Params
{

juce::StringArray getCategoryNames()
{
    return { "PLATE", "ROOM", "HALL", "CHAMBER" };
}

juce::StringArray getModeNamesForCategory(int category)
{
    switch (category)
    {
        case 0: // Plate
            return { "Modern Silk", "Classic Steel" };
        case 1: // Room
            return { "Studio Room", "Live Room" };
        case 2: // Hall
            return { "Concert Hall", "Cathedral" };
        case 3: // Chamber
            return { "Velvet Chamber", "Stone Chamber" };
        default:
            return { "Mode 1", "Mode 2", "Mode 3" };
    }
}

int modeToMenuIndex(int category,int mode)
{
    mode=juce::jlimit(0,2,mode);
    if(category==0)return mode==1?1:0;
    if(category==1)return mode==0?0:1;
    if(category==2)return mode==2?1:0;
    if(category==3)return mode==1?1:0;
    return mode;
}
int menuIndexToMode(int category,int index)
{
    const int count=getModeNamesForCategory(category).size();
    index=juce::jlimit(0,count-1,index);
    if (category == 2 && index == 1) return 2;
    if (category == 3 && index == 1) return 1;
    return index;
}

juce::StringArray getAllKnobParamIds()
{
    return {
        IDs::DECAY, IDs::PREDELAY, IDs::MIX,
        IDs::HIGH_FREQ, IDs::HIGH_SHELF, IDs::BASS_FREQ, IDs::BASS_MULT,
        IDs::SIZE, IDs::ATTACK,
        IDs::EARLY_DIFF, IDs::LATE_DIFF,
        IDs::MOD_RATE, IDs::MOD_DEPTH,
        IDs::LOW_CUT, IDs::HIGH_CUT
    };
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // 1. 类别选择 (Plate, Room, Hall, Chamber)
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { IDs::CATEGORY, 1 },
        "Category",
        getCategoryNames(),
        0));

    // 2. 模式选择 (Mode 1, Mode 2, Mode 3)
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { IDs::MODE, 1 },
        "Mode",
        juce::StringArray { "Mode 1", "Mode 2", "Mode 3" },
        0));

    auto makeFloatParam = [](const juce::String& id, const juce::String& name,
                             float minVal, float maxVal, float defaultVal,
                             float step, float skew,
                             const juce::String& unit, int numDecimals)
    {
        return std::make_unique<PhysicalAudioParameter>(
            id, name,
            juce::NormalisableRange<float>(minVal, maxVal, step, skew),
            defaultVal, unit, numDecimals
        );
    };

    // 3. TIME / MASTER (3)
    params.push_back(makeFloatParam(IDs::DECAY,    "Decay",    0.2f, 20.0f, 3.99f, 0.01f, 0.45f, "s",  2));
    params.push_back(makeFloatParam(IDs::PREDELAY, "PreDelay", 0.0f, 250.0f, 20.0f, 0.1f,  0.5f,  "ms", 1));
    params.push_back(makeFloatParam(IDs::MIX,      "Mix",      0.0f, 100.0f, 100.0f, 0.1f, 1.0f,  "%",  1));

    // 4. DAMPING (4)
    params.push_back(makeFloatParam(IDs::HIGH_FREQ,  "High Freq",  1000.0f, 20000.0f, 6000.0f, 1.0f,  0.35f, "Hz", 0));
    params.push_back(makeFloatParam(IDs::HIGH_SHELF, "High Shelf", -24.0f,  0.0f,     -24.0f,  0.1f,  1.0f,  "dB", 1));
    params.push_back(makeFloatParam(IDs::BASS_FREQ,  "Bass Freq",  50.0f,   1000.0f,  300.0f,  1.0f,  0.4f,  "Hz", 0));
    params.push_back(makeFloatParam(IDs::BASS_MULT,  "Bass Mult",  0.25f,   4.0f,     1.50f,   0.01f, 0.5f,  "X",  2));

    // 5. SHAPE (2)
    params.push_back(makeFloatParam(IDs::SIZE,   "Size",   0.0f, 100.0f, 100.0f, 0.1f, 1.0f, "%", 1));
    params.push_back(makeFloatParam(IDs::ATTACK, "Attack", 0.0f, 100.0f, 50.0f,  0.1f, 1.0f, "%", 1));

    // 6. DIFF (2)
    params.push_back(makeFloatParam(IDs::EARLY_DIFF, "Early Diff", 0.0f, 100.0f, 100.0f, 0.1f, 1.0f, "%", 1));
    params.push_back(makeFloatParam(IDs::LATE_DIFF,  "Late Diff",  0.0f, 100.0f, 100.0f, 0.1f, 1.0f, "%", 1));

    // 7. MOD (2)
    params.push_back(makeFloatParam(IDs::MOD_RATE,  "Mod Rate",  0.10f, 10.0f,  2.53f, 0.01f, 0.35f, "Hz", 2));
    params.push_back(makeFloatParam(IDs::MOD_DEPTH, "Mod Depth", 0.0f,  100.0f, 38.0f, 0.1f,  1.0f,  "%",  1));

    // 8. EQ (2)
    params.push_back(makeFloatParam(IDs::LOW_CUT,  "Low Cut",  10.0f,   500.0f,   10.0f,   1.0f, 0.35f, "Hz", 0));
    params.push_back(makeFloatParam(IDs::HIGH_CUT, "High Cut", 1000.0f, 20000.0f, 8000.0f, 1.0f, 0.35f, "Hz", 0));

    params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { IDs::BYPASS, 1 }, "Bypass", false));
    return { params.begin(), params.end() };
}

} // namespace Params
