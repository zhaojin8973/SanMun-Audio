#pragma once
#include "Parameters.h"

namespace Params
{
inline constexpr const char* LINEAR_VERSION = "linearPhaseVersion";
inline constexpr const char* LINEAR_LEGACY_MASK = "linearPhaseLegacyMask";
inline constexpr const char* LINEAR_QUALITY = "linearPhaseQuality";
inline constexpr int ALL_LINEAR_BANDS = (1 << MAX_BANDS) - 1;

// 根属性不增加自动化参数；旧工程的动态开关仍在其它三种模式下正常使用。
inline void markCurrentLinearState(juce::ValueTree& tree, juce::UndoManager* undo = nullptr)
{
    tree.setProperty(LINEAR_VERSION, 1, undo);
    tree.setProperty(LINEAR_LEGACY_MASK, 0, undo);
    tree.setProperty(LINEAR_QUALITY, 0, undo);
}

// APVTS 参数存于 PARAM 子节点；旧工程缺少新增字段时显式恢复默认值。
inline void supplyUpgradeDefaults(juce::ValueTree& tree)
{
    if (!tree.hasProperty(LINEAR_QUALITY)) tree.setProperty(LINEAR_QUALITY, 0, nullptr);
    if (!tree.hasProperty(LINEAR_VERSION))
    {
        tree.setProperty(LINEAR_VERSION, 1, nullptr);
        tree.setProperty(LINEAR_LEGACY_MASK, ALL_LINEAR_BANDS, nullptr);
    }
    const auto supply = [&](const juce::String& id, float value)
    {
        if (tree.getChildWithProperty("id", id).isValid()) return;
        juce::ValueTree child("PARAM"); child.setProperty("id", id, nullptr);
        child.setProperty("value", value, nullptr); tree.appendChild(child, nullptr);
    };
    supply(IDs::CHARACTER_MODE, 0);
    for (int n = 1; n <= MAX_BANDS; ++n)
    {
        supply(IDs::bandDynamic(n), 0); supply(IDs::bandDynRange(n), 6);
        supply(IDs::bandDynThreshold(n), -24);
        supply(IDs::bandDynAuto(n), 0);
        supply(IDs::bandDynUpward(n), 0);
    }
}
}
