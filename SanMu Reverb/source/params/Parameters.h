// ==============================================================================
// Parameters.h - SanMu Reverb 15 旋钮全物理参数工业级闭环定义
// ==============================================================================

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace Params
{

namespace IDs
{
    inline const juce::String BYPASS      = "bypass";
    // 类别与专属模式选择
    inline const juce::String CATEGORY    = "category";   // 0: Plate, 1: Room, 2: Hall, 3: Chamber
    inline const juce::String MODE        = "mode";       // 0, 1, 2

    // TIME / MASTER (3)
    inline const juce::String MIX         = "mix";
    inline const juce::String PREDELAY    = "predelay";
    inline const juce::String DECAY       = "decay";

    // DAMPING (4)
    inline const juce::String HIGH_FREQ   = "high_freq";
    inline const juce::String HIGH_SHELF  = "high_shelf";
    inline const juce::String BASS_FREQ   = "bass_freq";
    inline const juce::String BASS_MULT   = "bass_mult";

    // SHAPE (2)
    inline const juce::String SIZE        = "size";
    inline const juce::String ATTACK      = "attack";

    // DIFF (2)
    inline const juce::String EARLY_DIFF  = "early_diff";
    inline const juce::String LATE_DIFF   = "late_diff";

    // MOD (2)
    inline const juce::String MOD_RATE    = "mod_rate";
    inline const juce::String MOD_DEPTH   = "mod_depth";

    // EQ (2)
    inline const juce::String LOW_CUT     = "low_cut";
    inline const juce::String HIGH_CUT    = "high_cut";
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

// 获取 4 大类别的名称列表
juce::StringArray getCategoryNames();

// 获取当前类别下 3 大专属模式的名称
juce::StringArray getModeNamesForCategory(int category);
// 菜单位置与宿主旧编号分开，避免删减菜单破坏Cathedral的编号2。
int modeToMenuIndex(int category,int mode);
int menuIndexToMode(int category,int index);

// 获取 15 个旋钮参数 ID 列表（用于统一布局）
juce::StringArray getAllKnobParamIds();

} // namespace Params
