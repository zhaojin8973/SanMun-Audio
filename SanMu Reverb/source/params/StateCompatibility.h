#pragma once
#include "Parameters.h"
#include <cmath>
#include <cstdlib>

namespace state_compatibility
{
inline constexpr const char* plateStyleVersion="plateStyleVersion";

// 宿主工程与用户预设共用完整性检查，校验通过后才允许替换当前状态。
inline bool isValid(const juce::ValueTree& state, const juce::AudioProcessorValueTreeState& apvts)
{
    if (!state.isValid() || !state.hasType(apvts.state.getType())) return false;
    auto ids = Params::getAllKnobParamIds();
    ids.add(Params::IDs::CATEGORY); ids.add(Params::IDs::MODE);
    if (state.getChildWithProperty("id", Params::IDs::BYPASS).isValid()) ids.add(Params::IDs::BYPASS);
    if (state.getNumChildren() != ids.size()) return false;
    for (const auto& id : ids)
    {
        const auto child = state.getChildWithProperty("id", id);
        const auto* parameter = apvts.getParameter(id);
        if (!child.hasType("PARAM") || !child.hasProperty("value") || parameter == nullptr) return false;
        const auto text = child.getProperty("value").toString();
        char* end = nullptr;
        const auto utf8 = text.toRawUTF8();
        const double parsed = std::strtod(utf8, &end);
        const auto range = parameter->getNormalisableRange();
        if (end == utf8 || *end != '\0' || !std::isfinite(parsed) || parsed < range.start || parsed > range.end) return false;
        if ((id == Params::IDs::CATEGORY || id == Params::IDs::MODE || id == Params::IDs::BYPASS) && parsed != std::floor(parsed)) return false;
    }
    return true;
}

inline void markCurrent(juce::ValueTree state)
{
    state.setProperty(plateStyleVersion,1,nullptr);
}

// 0907A的三个Plate模式都发出P3声音。旧工程/用户预设恢复时统一指向Silk，
// 只有带新版标记的mode1才表示Steel；其他类型的模式编号保持不变。
inline void migrate(juce::ValueTree state)
{
    if (!state.getChildWithProperty("id", Params::IDs::BYPASS).isValid())
    {
        juce::ValueTree bypass("PARAM"); bypass.setProperty("id", Params::IDs::BYPASS, nullptr);
        bypass.setProperty("value", 0, nullptr); state.addChild(bypass, -1, nullptr);
    }
    if(!state.hasProperty(plateStyleVersion))
    {
        const auto category=state.getChildWithProperty("id",Params::IDs::CATEGORY);
        auto mode=state.getChildWithProperty("id",Params::IDs::MODE);
        if(category.isValid()&&mode.isValid()&&category.getProperty("value").toString().getDoubleValue()==0)
        {
            const double value=mode.getProperty("value").toString().getDoubleValue();
            if(value==1||value==2)mode.setProperty("value",0,nullptr);
        }
    }
    markCurrent(state);
}
}
