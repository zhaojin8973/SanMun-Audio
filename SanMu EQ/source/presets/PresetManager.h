// ==============================================================================
// PresetManager.h - 预设管理系统
// 第三阶段：工厂预设 + 用户预设的保存/加载
// ==============================================================================

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../params/Parameters.h"

#include <array>

// ==============================================================================
// 单个预设的数据结构——所有参数值的快照
// ==============================================================================
struct PresetData
{
    juce::String name;

    // 每个频段的参数（24 段完整快照，R07）
    bool   present[Params::NUM_BANDS]   = {};
    bool   enabled[Params::NUM_BANDS]   = {};
    int    type[Params::NUM_BANDS]      = {};
    float  freq[Params::NUM_BANDS]      = {};
    float  gain[Params::NUM_BANDS]      = {};
    float  q[Params::NUM_BANDS]         = {};
    int    slope[Params::NUM_BANDS]     = {};
    int    placement[Params::NUM_BANDS] = {};
    bool   solo[Params::NUM_BANDS]      = {};

    // 全局参数
    float inputGainL     = 0.0f;
    float inputGainR     = 0.0f;
    float outputGainL    = 0.0f;
    float outputGainR    = 0.0f;
    bool  phaseInvert    = false;
    int   processingMode = 0;
    bool  globalBypass   = false;
    bool  autoGain       = false;
};

// ==============================================================================
// PresetManager——管理工厂预设和用户预设
// ==============================================================================
class PresetManager
{
public:
    PresetManager() = default;

    // ---- 工厂预设 ----
    /** 返回工厂预设数量 */
    static int getNumFactoryPresets();

    /** 获取工厂预设名称列表 */
    static const juce::StringArray& getFactoryPresetNames();

    /** 获取工厂预设数据 */
    static const PresetData& getFactoryPreset(int index);

    // ---- 用户预设（文件 I/O） ----
    /** 获取用户预设目录 */
    static juce::File getUserPresetDirectory();

    /** 获取用户预设列表 */
    static juce::StringArray getUserPresetNames();

    /** 保存当前参数状态为用户预设 */
    static bool saveUserPreset(const juce::String& name,
                               juce::AudioProcessorValueTreeState& apvts);

    /** 加载用户预设 */
    static bool loadUserPreset(const juce::String& name,
                               juce::AudioProcessorValueTreeState& apvts);

    /** 删除用户预设 */
    static bool deleteUserPreset(const juce::String& name);

    // ---- 应用预设 ----
    /** 将 PresetData 写入 APVTS */
    static void applyPreset(const PresetData& preset,
                            juce::AudioProcessorValueTreeState& apvts);

    // ---- 导出当前状态为 PresetData ----
    static PresetData snapshotFromApvts(juce::AudioProcessorValueTreeState& apvts);

private:
    /** 初始化工厂预设数据（懒加载） */
    static void ensureFactoryPresetsBuilt();
    static std::vector<PresetData>& getFactoryPresets();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetManager)
};
