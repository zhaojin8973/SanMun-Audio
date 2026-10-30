// ==============================================================================
// Parameters.h - SanMu Delay 插件参数常量单一数据源
// 约定：参数 ID 一经发布永不改名、永不改含义
// ==============================================================================

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace Params
{

// ---- 引擎类型 ----
enum DelayEngineType
{
    EngineModern   = 0,  // Modern（超清高保真数字延迟，平滑无怪声，默认）
    EngineTape     = 1,  // Tape（经典 1970 磁带录音机，环内软饱和、预加重与 Wow 抖动）
    EngineBbd      = 2,  // BBD（1980 模拟桶链设备，时钟滤波滚降、暖色压扩与颗粒）
    EngineDiffused = 3   // Diffused（水洗空间，全通能量守恒扩散网络，回声化混响）
};

// ---- 拓扑模式 ----
enum TopologyType
{
    TopologyStereo   = 0,  // 双通道独立/联动
    TopologyPingPong = 1,  // 乒乓回弹
    TopologyCross    = 2   // 交叉立体声融合
};

// ---- 同步模式 ----
enum SyncMode
{
    SyncHost = 0,   // Sync (锁定宿主 DAW)
    SyncBpm  = 1,   // BPM (手动设定 BPM)
    SyncFree = 2    // Free (纯毫秒自由模式)
};

// ---- 侧链来源 ----
enum DuckSource
{
    DuckSourceInternal = 0,  // 内部干声自避让
    DuckSourceExternal = 1   // 外部宿主侧链总线 (Sidechain Bus)
};

// ---- 节拍切分定义 (时长降序排列，涵盖 1/1、1/2 及常规/附点/三连音) ----
inline juce::StringArray getDivisionNames()
{
    return {
        "1/1",
        "1/2D",
        "1/2",
        "1/2T",
        "1/4D",
        "1/4",
        "1/4T",
        "1/8D",
        "1/8",
        "1/8T",
        "1/16D",
        "1/16",
        "1/16T",
        "1/32"
    };
}

// 获取时值系数（单位：拍数 beats）
inline double getDivisionBeats(int index) noexcept
{
    switch (index)
    {
        case 0:  return 4.0;          // 1/1 全音符
        case 1:  return 3.0;          // 1/2 Dotted 附点二分音符
        case 2:  return 2.0;          // 1/2 二分音符
        case 3:  return 4.0 / 3.0;    // 1/2 Triplet 三连音二分音符
        case 4:  return 1.5;          // 1/4 Dotted 附点四分音符
        case 5:  return 1.0;          // 1/4 四分音符
        case 6:  return 2.0 / 3.0;    // 1/4 Triplet 三连音四分音符
        case 7:  return 0.75;         // 1/8 Dotted 附点八分音符
        case 8:  return 0.5;          // 1/8 八分音符
        case 9:  return 1.0 / 3.0;    // 1/8 Triplet 三连音八分音符
        case 10: return 0.375;        // 1/16 Dotted 附点十六分音符
        case 11: return 0.25;         // 1/16 十六分音符
        case 12: return 0.5 / 3.0;    // 1/16 Triplet 三连音十六分音符
        case 13: return 0.125;        // 1/32 三十二分音符
        default: return 1.0;          // 1/4
    }
}

// ---- 参数 ID 定义 ----
namespace IDs
{
    constexpr auto engine           = "engine";
    constexpr auto sync_mode        = "sync_mode";
    constexpr auto manual_bpm       = "manual_bpm";
    constexpr auto division         = "division";    // Note L (向下兼容)
    constexpr auto division_r       = "division_r";  // Note R
    constexpr auto time_l           = "time_l";
    constexpr auto time_r           = "time_r";
    constexpr auto time_link        = "time_link";
    constexpr auto feedback         = "feedback";
    constexpr auto topology         = "topology";

    constexpr auto damp_lowcut      = "damp_lowcut";
    constexpr auto damp_highcut     = "damp_highcut";

    constexpr auto mix              = "mix";
    constexpr auto output_gain      = "output_gain";
    constexpr auto freeze           = "freeze";
    constexpr auto global_bypass    = "global_bypass";
    constexpr auto theme            = "theme";

    // 动态 Ducking
    constexpr auto duck_enable      = "duck_enable";
    constexpr auto duck_source      = "duck_source";
    constexpr auto duck_threshold   = "duck_threshold";
    constexpr auto duck_amount      = "duck_amount";
    constexpr auto duck_release     = "duck_release";

    // 通用调制 (Modulation)
    constexpr auto mod_rate         = "mod_rate";
    constexpr auto mod_depth        = "mod_depth";

    // 引擎专属修饰参数
    constexpr auto tape_drive       = "tape_drive";
    constexpr auto tape_wow         = "tape_wow";
    constexpr auto bbd_warmth       = "bbd_warmth";
    constexpr auto diffusion_amount = "diffusion_amount";
}

// ---- 参数范围常量 ----
namespace Ranges
{
    constexpr float TIME_MS_MIN          = 1.0f;
    constexpr float TIME_MS_MAX          = 2000.0f;
    constexpr float FEEDBACK_MIN         = 0.0f;
    constexpr float FEEDBACK_MAX         = 100.0f;
    constexpr float DAMP_LOWCUT_MIN      = 20.0f;
    constexpr float DAMP_LOWCUT_MAX      = 1000.0f;
    constexpr float DAMP_HIGHCUT_MIN     = 1000.0f;
    constexpr float DAMP_HIGHCUT_MAX     = 20000.0f;
    constexpr float MIX_MIN              = 0.0f;
    constexpr float MIX_MAX              = 100.0f;
    constexpr float OUTPUT_GAIN_MIN      = -24.0f;
    constexpr float OUTPUT_GAIN_MAX      = 24.0f;

    constexpr float DUCK_THRESH_MIN      = -60.0f;
    constexpr float DUCK_THRESH_MAX      = 0.0f;
    constexpr float DUCK_AMOUNT_MIN      = 0.0f;
    constexpr float DUCK_AMOUNT_MAX      = 100.0f;
    constexpr float DUCK_RELEASE_MIN     = 50.0f;
    constexpr float DUCK_RELEASE_MAX     = 2000.0f;

    constexpr float ENGINE_MOD_MIN       = 0.0f;
    constexpr float ENGINE_MOD_MAX       = 100.0f;

    constexpr float MOD_RATE_MIN         = 0.1f;
    constexpr float MOD_RATE_MAX         = 10.0f;
    constexpr float MOD_DEPTH_MIN        = 0.0f;
    constexpr float MOD_DEPTH_MAX        = 100.0f;

    constexpr float BPM_MIN              = 20.0f;
    constexpr float BPM_MAX              = 300.0f;
}

// ---- 默认值 ----
namespace Defaults
{
    constexpr int   ENGINE           = EngineModern;
    // 默认使用手动 BPM；宿主同步仍可在时间编辑器中主动选择。
    constexpr int   SYNC_MODE        = SyncBpm;
    constexpr float MANUAL_BPM       = 120.0f;
    constexpr int   DIVISION         = 5; // 1/4 (Note L)
    constexpr int   DIVISION_R       = 5; // 1/4 (Note R)
    constexpr float TIME_MS          = 500.0f;
    constexpr bool  TIME_LINK        = true;
    constexpr float FEEDBACK         = 35.0f; // %
    constexpr int   TOPOLOGY         = TopologyStereo;

    constexpr float DAMP_LOWCUT      = 20.0f;    // Hz
    constexpr float DAMP_HIGHCUT     = 14000.0f; // Hz

    constexpr float MOD_RATE         = 1.0f;     // Hz
    constexpr float MOD_DEPTH        = 0.0f;     // % (默认 0% 保持纯净)

    constexpr float MIX              = 30.0f;    // %
    constexpr float OUTPUT_GAIN      = 0.0f;     // dB
    constexpr bool  FREEZE           = false;
    constexpr bool  GLOBAL_BYPASS    = false;

    constexpr bool  DUCK_ENABLE      = false;
    constexpr int   DUCK_SOURCE      = DuckSourceInternal;
    constexpr float DUCK_THRESHOLD   = -24.0f;   // dB
    constexpr float DUCK_AMOUNT      = 60.0f;    // % (约 14.4 dB 最大衰减)
    constexpr float DUCK_RELEASE     = 300.0f;   // ms

    constexpr float TAPE_DRIVE       = 30.0f;    // %
    constexpr float TAPE_WOW         = 20.0f;    // %
    constexpr float BBD_WARMTH       = 50.0f;    // %
    constexpr float DIFFUSION_AMOUNT = 60.0f;    // %
}

// 引擎名称列表
inline juce::StringArray getEngineNames()
{
    return { "Modern", "Tape", "BBD", "Diffused" };
}

// 拓扑名称列表
inline juce::StringArray getTopologyNames()
{
    return { "Independent", "Ping-Pong", "Cross" };
}

// 同步模式名称
inline juce::StringArray getSyncModeNames()
{
    return { "Sync", "BPM", "Free" };
}

// 侧链源名称
inline juce::StringArray getDuckSourceNames()
{
    return { "Internal", "External" };
}

// ---- 构建 APVTS 参数布局 ----
inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace Ranges;
    using namespace Defaults;

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // 引擎选择
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        IDs::engine, "Engine", getEngineNames(), ENGINE));

    // 同步模式 (Sync, BPM, Free)
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        IDs::sync_mode, "Sync Mode", getSyncModeNames(), SYNC_MODE));

    // 手动 BPM (20.0 ~ 300.0 BPM，步进 0.1)
    auto bpmRange = juce::NormalisableRange<float>(BPM_MIN, BPM_MAX, 0.1f);
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::manual_bpm, "BPM", bpmRange, MANUAL_BPM));

    // 切分音符 (Note L / Note R)
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        IDs::division, "Note L", getDivisionNames(), DIVISION));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        IDs::division_r, "Note R", getDivisionNames(), DIVISION_R));

    // 自由延迟时间 (对数偏斜，短时间更精细)
    auto timeRange = juce::NormalisableRange<float>(TIME_MS_MIN, TIME_MS_MAX, 0.1f, 0.35f);
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::time_l, "Time L", timeRange, TIME_MS));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::time_r, "Time R", timeRange, TIME_MS));

    layout.add(std::make_unique<juce::AudioParameterBool>(
        IDs::time_link, "Link L/R", TIME_LINK));

    // 反馈
    auto feedbackRange = juce::NormalisableRange<float>(FEEDBACK_MIN, FEEDBACK_MAX, 0.1f);
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::feedback, "Feedback", feedbackRange, FEEDBACK));

    // 拓扑
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        IDs::topology, "Topology", getTopologyNames(), TOPOLOGY));

    // 阻尼低切 / 高切 (对数偏斜)
    auto lowCutRange = juce::NormalisableRange<float>(DAMP_LOWCUT_MIN, DAMP_LOWCUT_MAX, 1.0f, 0.4f);
    auto highCutRange = juce::NormalisableRange<float>(DAMP_HIGHCUT_MIN, DAMP_HIGHCUT_MAX, 1.0f, 0.4f);
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::damp_lowcut, "Low Cut", lowCutRange, DAMP_LOWCUT));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::damp_highcut, "High Cut", highCutRange, DAMP_HIGHCUT));

    // 干湿比 / 输出增益
    auto mixRange = juce::NormalisableRange<float>(MIX_MIN, MIX_MAX, 0.1f);
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::mix, "Mix", mixRange, MIX));

    auto outGainRange = juce::NormalisableRange<float>(OUTPUT_GAIN_MIN, OUTPUT_GAIN_MAX, 0.1f);
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::output_gain, "Output Gain", outGainRange, OUTPUT_GAIN));

    layout.add(std::make_unique<juce::AudioParameterBool>(
        IDs::freeze, "Freeze", FREEZE));
    layout.add(std::make_unique<juce::AudioParameterBool>(
        IDs::global_bypass, "Global Bypass", GLOBAL_BYPASS));

    // Ducking
    layout.add(std::make_unique<juce::AudioParameterBool>(
        IDs::duck_enable, "Duck Enable", DUCK_ENABLE));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        IDs::duck_source, "Duck Source", getDuckSourceNames(), DUCK_SOURCE));

    auto threshRange = juce::NormalisableRange<float>(DUCK_THRESH_MIN, DUCK_THRESH_MAX, 0.1f);
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::duck_threshold, "Duck Threshold", threshRange, DUCK_THRESHOLD));

    auto duckAmountRange = juce::NormalisableRange<float>(DUCK_AMOUNT_MIN, DUCK_AMOUNT_MAX, 0.1f);
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::duck_amount, "Duck Amount", duckAmountRange, DUCK_AMOUNT));

    auto duckReleaseRange = juce::NormalisableRange<float>(DUCK_RELEASE_MIN, DUCK_RELEASE_MAX, 1.0f, 0.35f);
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::duck_release, "Duck Release", duckReleaseRange, DUCK_RELEASE));

    // 通用调制 (LFO Rate & Depth)
    auto modRateRange = juce::NormalisableRange<float>(MOD_RATE_MIN, MOD_RATE_MAX, 0.01f, 0.4f);
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::mod_rate, "Mod Rate", modRateRange, MOD_RATE));

    auto modDepthRange = juce::NormalisableRange<float>(MOD_DEPTH_MIN, MOD_DEPTH_MAX, 0.1f);
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::mod_depth, "Mod Depth", modDepthRange, MOD_DEPTH));

    // 引擎专属微调
    auto modRange = juce::NormalisableRange<float>(ENGINE_MOD_MIN, ENGINE_MOD_MAX, 0.1f);
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::tape_drive, "Tape Drive", modRange, TAPE_DRIVE));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::tape_wow, "Tape Wow", modRange, TAPE_WOW));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::bbd_warmth, "BBD Warmth", modRange, BBD_WARMTH));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::diffusion_amount, "Diffusion", modRange, DIFFUSION_AMOUNT));

    // UI 主题 (0 = Light, 1 = Dark)
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        IDs::theme, "Theme", juce::StringArray{ "Light", "Dark" }, 0));

    return layout;
}

} // namespace Params
