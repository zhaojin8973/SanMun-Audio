// ==============================================================================
// Parameters.h - 插件参数常量定义
// 所有参数 ID、范围、默认值的单一数据源
// DSP 和 UI 模块都引用此头文件，避免参数 ID 拼写错误
// ==============================================================================

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace Params
{

// ---- 频段数量 ----
constexpr int LEGACY_NUM_BANDS = 6;
constexpr int MAX_BANDS        = 24;
constexpr int NUM_BANDS        = MAX_BANDS;

// ---- 滤波器类型 ----
enum FilterType
{
    Peak = 0,       // 钟型/峰值滤波器
    LowShelf,       // 低频搁架
    HighShelf,      // 高频搁架
    LowCut,         // 低切/高通
    HighCut         // 高切/低通
};

// ---- 立体声通道分布模式（Stereo Placement） ----
enum StereoPlacement
{
    PlacementStereo = 0,   // 立体声（L + R 同时处理）
    PlacementLeft,         // 仅左声道（L）
    PlacementRight,        // 仅右声道（R）
    PlacementMid,          // 中置声道（Mid = (L+R)/2）
    PlacementSide          // 侧边声场（Side = (L-R)/2）
};

// ---- 参数范围常量 ----
namespace Ranges
{
    constexpr float FREQ_MIN   = 20.0f;       // Hz
    constexpr float FREQ_MAX   = 20000.0f;    // Hz
    // 频段增益范围与曲线轴最大档一致（±24）：常规 ±12，超过 12 自动扩展
    constexpr float GAIN_MIN   = -24.0f;      // dB
    constexpr float GAIN_MAX   = 24.0f;       // dB
    constexpr float Q_MIN      = 0.1f;        // Q 值
    // Q 上限 10 → 40（Cut 采用 Butterworth 分配 + 共振注入，按肥波实测校准：
    // Q=40 时 12/24/36/48 dB/oct 共振峰分别约 +30/+25/+23/+19dB，
    // 均落在 ±30 显示档位内）
    constexpr float Q_MAX      = 40.0f;
    constexpr float GAIN_STEP  = 0.1f;        // dB 步进
    constexpr float Q_STEP     = 0.01f;       // Q 步进
}

// ---- 默认值 ----
namespace Defaults
{
    // 前六个频段的默认频率（Hz）：低 → 高（保持原有默认值不变）
    // 后续 7~24 频段预设对数阶梯分布
    constexpr float FREQUENCIES[MAX_BANDS] = {
        40.0f, 150.0f, 500.0f, 2000.0f, 5000.0f, 10000.0f,
        60.0f, 100.0f, 250.0f, 350.0f, 750.0f, 1200.0f,
        1600.0f, 2500.0f, 3200.0f, 4000.0f, 6000.0f, 7500.0f,
        8500.0f, 12000.0f, 14000.0f, 16000.0f, 18000.0f, 19000.0f
    };
    constexpr int TYPES[MAX_BANDS] = {
        LowShelf, Peak, Peak, Peak, Peak, HighShelf,
        Peak, Peak, Peak, Peak, Peak, Peak,
        Peak, Peak, Peak, Peak, Peak, Peak,
        Peak, Peak, Peak, Peak, Peak, Peak
    };
    constexpr float GAIN                   = 0.0f;     // dB
    constexpr float Q                      = 1.0f;
    constexpr bool  ENABLED                = true;
    constexpr float INPUT_GAIN_L           = 0.0f;     // dB
    constexpr float INPUT_GAIN_R           = 0.0f;     // dB
    constexpr float OUTPUT_GAIN_L          = 0.0f;     // dB
    constexpr float OUTPUT_GAIN_R          = 0.0f;     // dB
}

// ---- 参数 ID 字符串 ----
namespace IDs
{
    // 频段参数 - 运行时用 juce::String(N) 拼接数字
    inline juce::String bandEnabled(int band)   { return "band" + juce::String(band) + "_enabled"; }
    inline juce::String bandType(int band)      { return "band" + juce::String(band) + "_type"; }
    inline juce::String bandFreq(int band)      { return "band" + juce::String(band) + "_freq"; }
    inline juce::String bandGain(int band)      { return "band" + juce::String(band) + "_gain"; }
    inline juce::String bandQ(int band)         { return "band" + juce::String(band) + "_q"; }
    inline juce::String bandSolo(int band)       { return "band" + juce::String(band) + "_solo"; }
    inline juce::String bandSlope(int band)     { return "band" + juce::String(band) + "_slope"; }
    inline juce::String bandPlacement(int band) { return "band" + juce::String(band) + "_placement"; }
    // M1 新增：槽位存在性（present 与 enabled 独立；删除仅置 present=false，不破坏自动化）
    inline juce::String bandPresent(int band)   { return "band" + juce::String(band) + "_present"; }
    inline juce::String bandDynamic(int band)   { return "band" + juce::String(band) + "_dynamic"; }
    inline juce::String bandDynRange(int band)  { return "band" + juce::String(band) + "_dyn_range"; }
    inline juce::String bandDynThreshold(int band) { return "band" + juce::String(band) + "_dyn_threshold"; }
    inline juce::String bandDynUpward(int band) { return "band" + juce::String(band) + "_dyn_upward"; }
    inline juce::String bandDynAuto(int band) { return "band" + juce::String(band) + "_dyn_auto"; }

    // 全局参数（第九轮：输入/输出 L/R 独立增益替代 增益+平衡）
    constexpr auto INPUT_GAIN_L   = "input_gain_l";
    constexpr auto INPUT_GAIN_R   = "input_gain_r";
    constexpr auto OUTPUT_GAIN_L  = "output_gain_l";
    constexpr auto OUTPUT_GAIN_R  = "output_gain_r";
    constexpr auto PHASE_INVERT   = "phase_invert";
    constexpr auto EQ_MODE        = "eq_mode";
    constexpr auto THEME          = "theme";
    constexpr auto GAIN_RANGE     = "gain_range";
    constexpr auto GLOBAL_BYPASS  = "global_bypass";
    constexpr auto AUTO_GAIN      = "auto_gain";
    constexpr auto CHARACTER_MODE = "character_mode";
}

// ---- 滤波器类型名称 ----
inline const juce::StringArray& getFilterTypeNames()
{
    static const juce::StringArray names = {
        "Bell",
        "Low Shelf",
        "High Shelf",
        "Low Cut",
        "High Cut"
    };
    return names;
}

// ---- Cut 滤波器斜率名称 ----
inline const juce::StringArray& getSlopeNames()
{
    static const juce::StringArray names = {
        "12 dB/oct",
        "24 dB/oct",
        "36 dB/oct",
        "48 dB/oct"
    };
    return names;
}

// ---- 立体声分布模式名称 ----
inline const juce::StringArray& getPlacementNames()
{
    static const juce::StringArray names = {
        "Stereo",
        "Left",
        "Right",
        "Mid",
        "Side"
    };
    return names;
}

// ---- 兼容参数名称 ----
// 该参数仍保留 Zero Latency / Linear Phase 两个值，用于预设、自动化和旧工程兼容；
// 新界面把它作为独立的线性相位开关，音色由 character_mode 单独表达。
inline const juce::StringArray& getEqModeNames()
{
    static const juce::StringArray names = {
        "Zero Latency",
        "Linear Phase"
    };
    return names;
}

// ---- 构建 APVTS 参数布局 ----
// 契约：严格保持前 58 项参数 ID、顺序与取值范围不变，新增参数（7~24 段与 present 标志）追加在后
inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace Ranges;
    using namespace Defaults;

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // 频率范围使用对数偏斜：低频端有更高分辨率
    auto freqRange = juce::NormalisableRange<float>(
        FREQ_MIN, FREQ_MAX, 1.0f, 0.25f
    );

    // 增益是线性范围（dB 本身已是对数单位）
    auto gainRange = juce::NormalisableRange<float>(
        GAIN_MIN, GAIN_MAX, GAIN_STEP
    );

    // Q 值使用对数偏斜，低 Q 端更精细（0.1-1.0 是常用范围）
    auto qRange = juce::NormalisableRange<float>(
        Q_MIN, Q_MAX, Q_STEP, 0.5f
    );

    auto addBandParameters = [&](int i)
    {
        auto prefix = "Band " + juce::String(i);

        layout.add(std::make_unique<juce::AudioParameterBool>(
            IDs::bandEnabled(i), prefix + " Enabled", ENABLED
        ));
        layout.add(std::make_unique<juce::AudioParameterChoice>(
            IDs::bandType(i), prefix + " Type",
            getFilterTypeNames(), TYPES[i - 1]
        ));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            IDs::bandFreq(i), prefix + " Frequency",
            freqRange, FREQUENCIES[i - 1]
        ));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            IDs::bandGain(i), prefix + " Gain",
            gainRange, GAIN
        ));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            IDs::bandQ(i), prefix + " Q",
            qRange, Q
        ));
        layout.add(std::make_unique<juce::AudioParameterBool>(
            IDs::bandSolo(i), prefix + " Solo", false
        ));
        layout.add(std::make_unique<juce::AudioParameterChoice>(
            IDs::bandSlope(i), prefix + " Slope",
            getSlopeNames(), 0
        ));
        layout.add(std::make_unique<juce::AudioParameterChoice>(
            IDs::bandPlacement(i), prefix + " Placement",
            getPlacementNames(), 0
        ));
    };

    // ==========================================================================
    // [第 1 ~ 48 项] 经典前 6 段参数（保持原有布局 100% 不变）
    // ==========================================================================
    for (int i = 1; i <= LEGACY_NUM_BANDS; ++i)
    {
        addBandParameters(i);
    }

    // ==========================================================================
    // [第 49 ~ 58 项] 全局参数（保持原有布局 100% 不变）
    // ==========================================================================
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::INPUT_GAIN_L, "Input Gain L", gainRange, INPUT_GAIN_L
    ));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::INPUT_GAIN_R, "Input Gain R", gainRange, INPUT_GAIN_R
    ));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::OUTPUT_GAIN_L, "Output Gain L", gainRange, OUTPUT_GAIN_L
    ));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::OUTPUT_GAIN_R, "Output Gain R", gainRange, OUTPUT_GAIN_R
    ));
    layout.add(std::make_unique<juce::AudioParameterBool>(
        IDs::PHASE_INVERT, "Phase Invert", false
    ));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        IDs::EQ_MODE, "Linear Phase", getEqModeNames(), 0
    ));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        IDs::THEME, "UI Theme", juce::StringArray{ "Light", "Dark" }, 0
    ));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        IDs::GAIN_RANGE, "Gain Range", juce::StringArray{ "12 dB", "24 dB" }, 1
    ));
    layout.add(std::make_unique<juce::AudioParameterBool>(
        IDs::GLOBAL_BYPASS, "Global Bypass", false
    ));
    layout.add(std::make_unique<juce::AudioParameterBool>(
        IDs::AUTO_GAIN, "Auto Gain", false
    ));

    // ==========================================================================
    // [第 59 ~ 202 项] 新增 7 ~ 24 段参数（追加在后，绝不影响旧参数索引）
    // ==========================================================================
    for (int i = LEGACY_NUM_BANDS + 1; i <= MAX_BANDS; ++i)
    {
        addBandParameters(i);
    }

    // ==========================================================================
    // [第 203 ~ 226 项] 1 ~ 24 槽位存在性标志（present）
    // 前 6 段默认存在（true），7~24 段默认未创建（false）
    // ==========================================================================
    for (int i = 1; i <= MAX_BANDS; ++i)
    {
        const bool defaultPresent = (i <= LEGACY_NUM_BANDS);
        layout.add(std::make_unique<juce::AudioParameterBool>(
            IDs::bandPresent(i), "Band " + juce::String(i) + " Present", defaultPresent
        ));
    }
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        IDs::CHARACTER_MODE, "Character", juce::StringArray { "Clean", "Tube", "Discrete" }, 0
    ));
    for (int i = 1; i <= MAX_BANDS; ++i)
    {
        auto prefix = "Band " + juce::String(i);
        layout.add(std::make_unique<juce::AudioParameterBool>(IDs::bandDynamic(i), prefix + " Dynamic", false));
        layout.add(std::make_unique<juce::AudioParameterFloat>(IDs::bandDynRange(i), prefix + " Dynamic Range", juce::NormalisableRange<float>(0.f, 24.f, .1f), 6.f));
        layout.add(std::make_unique<juce::AudioParameterFloat>(IDs::bandDynThreshold(i), prefix + " Dynamic Threshold", juce::NormalisableRange<float>(-60.f, 0.f, .1f), -24.f));
    }

    // 追加参数，保留前 299 项的宿主自动化编号。
    for (int i = 1; i <= MAX_BANDS; ++i)
        layout.add(std::make_unique<juce::AudioParameterBool>(IDs::bandDynAuto(i), "Band " + juce::String(i) + " Auto Threshold", true));
    // 新方向放在最后，已有 323 项宿主参数的编号、范围保持不变。
    for (int i = 1; i <= MAX_BANDS; ++i)
        layout.add(std::make_unique<juce::AudioParameterBool>(IDs::bandDynUpward(i), "Band " + juce::String(i) + " Upward Compression", false));
    return layout;
}

} // namespace Params
