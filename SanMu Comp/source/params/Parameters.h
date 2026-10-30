// ==============================================================================
// Parameters.h - 压缩器插件参数常量定义
// 所有参数 ID、范围、默认值的单一数据源
// DSP 和 UI 模块都引用此头文件，避免参数 ID 拼写错误
//
// 约定（向下兼容铁律）：
//   参数 ID 一经发布永不改名、永不改含义；新增功能只新增 ID。
// ==============================================================================

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace Params
{

// ---- 压缩模式 ----
enum CompMode
{
    ModeVOX   = 0,  // VOX（现代人声贴脸平准压限神器，对标 RVox）
    ModeVCA   = 1,  // VCA（模拟母线 glue，对标 SSL 4000 G）
    ModeFET   = 2,  // FET（经典微秒级击打打桩机，对标 1176）
    ModeOpto  = 3   // Opto（经典光电双阶段丝滑管机，对标 LA-2A）
};

// 保持历史枚举别名兼容
constexpr CompMode ModeClean = ModeVOX;

constexpr double listenDurationSeconds = 5.0;

// ---- 参数范围常量 ----
namespace Ranges
{
    constexpr float THRESHOLD_MIN = -60.0f;   // dB
    constexpr float THRESHOLD_MAX = 0.0f;     // dB
    constexpr float RATIO_MIN      = 1.0f;    // 1:1
    constexpr float RATIO_MAX      = 20.0f;   // 20:1（UI 可叠加 ∞ 限幅档）
    constexpr float ATTACK_MIN     = 0.01f;   // ms
    constexpr float ATTACK_MAX     = 300.0f;  // ms
    constexpr float RELEASE_MIN    = 1.0f;    // ms
    constexpr float RELEASE_MAX    = 2000.0f; // ms
    constexpr float KNEE_MIN       = 0.0f;    // dB（硬拐）
    constexpr float KNEE_MAX       = 24.0f;   // dB（软拐）
    constexpr float MAKEUP_MIN     = -24.0f;  // dB
    constexpr float MAKEUP_MAX     = 24.0f;   // dB
    constexpr float MIX_MIN        = 0.0f;    // %
    constexpr float MIX_MAX        = 100.0f;  // %
    constexpr float HEADROOM_MIN   = 4.0f;   // dB（FET 输入饱和余量：越小越早进入饱和染色，UAD 同源语义）
    constexpr float HEADROOM_MAX   = 28.0f;  // dB
    constexpr float OUTPUT_TRIM_MIN = -18.0f; // dB（最终总输出 Trim，纯净电平缩放）
    constexpr float OUTPUT_TRIM_MAX = 18.0f;  // dB
    constexpr float SIDECHAIN_HP_MIN = 0.0f;  // Hz（0Hz = 关闭）
    constexpr float SIDECHAIN_HP_MAX = 1000.0f; // Hz
    constexpr float STEREO_LINK_MIN = 0.0f;    // %（0=左右独立）
    constexpr float STEREO_LINK_MAX = 100.0f;  // %（100=完全联动）

    // FET 特有（1176 经典面板刻度：0~48 刻度）
    constexpr float FET_INPUT_MIN   = 0.0f;    // 0~48 刻度（默认 24.0，12:00 正中方向）
    constexpr float FET_INPUT_MAX   = 48.0f;
    constexpr float FET_ATTACK_MIN  = 1.0f;    // 1~7 档位（1=最慢 800μs，7=最快 20μs）
    constexpr float FET_ATTACK_MAX  = 7.0f;
    constexpr float FET_RELEASE_MIN = 1.0f;    // 1~7 档位（1=最慢 1100ms，7=最快 50ms）
    constexpr float FET_RELEASE_MAX = 7.0f;
    constexpr float FET_OUTPUT_MIN  = 0.0f;    // 0~48 刻度（默认 24.0，12:00 正中方向）
    constexpr float FET_OUTPUT_MAX  = 48.0f;

    // Opto 特有（LA-2A 经典 0~100 硬件刻度）
    constexpr float OPTO_PEAK_REDUCTION_MIN = 0.0f;   // 0~100 硬件刻度
    constexpr float OPTO_PEAK_REDUCTION_MAX = 100.0f;
    constexpr float OPTO_GAIN_MIN           = 0.0f;   // 0~100 硬件刻度（40.0 为标准 Unity 0dB 工作点）
    constexpr float OPTO_GAIN_MAX           = 100.0f;
}

// ---- 默认值 ----
namespace Defaults
{
    constexpr float THRESHOLD     = -18.0f;   // dB
    constexpr float RATIO         = 4.0f;     // 4:1
    constexpr float ATTACK        = 10.0f;    // ms
    constexpr float RELEASE       = 150.0f;   // ms
    constexpr float KNEE          = 6.0f;     // dB
    constexpr float MAKEUP        = 0.0f;     // dB
    constexpr float MIX           = 100.0f;   // %
    constexpr float HEADROOM      = 16.0f;    // dB（输入饱和余量，小信号下无感，只改过驱动时染色）
    constexpr float OUTPUT_TRIM   = 0.0f;     // dB（默认 0dB 无增减）
    constexpr float SIDECHAIN_HP  = 60.0f;    // Hz（默认 60Hz，滤除超低频次声波与喷音，经典 R37 预设）
    constexpr float STEREO_LINK   = 100.0f;   // %

    // FET 特有（1176 经典面板位置：Input 默认坐落于 18.0 [10:30 方向] / Output 默认坐落于 30.0 [1:30 方向]）
    constexpr float FET_INPUT     = 18.0f;    // 1176 经典位置（10:30 方向，驱动基准，标准输入 -18dBFS 产生 10~14dB 黄金压缩）
    constexpr float FET_ATTACK    = 3.0f;     // 约 370μs（10:30 方向）
    constexpr float FET_RELEASE   = 4.0f;     // 约 230ms（12:00 方向，黄金默认释放点）
    constexpr float FET_OUTPUT    = 30.0f;    // 1176 经典位置（1:30 方向，补偿增益，输出电平与输入完美平衡）

    // Opto 特有（LA-2A 经典 0~100 硬件标定位置：经典黄金工作点 Gain 40.0 [Unity] / Peak Reduction 50.0 [3~5dB 经典平滑压缩]）
    constexpr float OPTO_PEAK_REDUCTION = 50.0f;
    constexpr float OPTO_GAIN           = 40.0f;
    constexpr bool  OPTO_IS_LIMIT       = false;   // false = Compress (3:1~4:1), true = Limit (10:1~20:1)
}

// ---- T4B 物理常量单一数据源 ----
namespace OptoPhysics
{
    constexpr float kRdark          = 1.0e6f;   // CdS 暗态电阻 (1MΩ)
    constexpr float kRfixedCompress = 70.7e3f;  // Compress 模式分压定臂 (70.7kΩ, 约 3:1~4:1 软拐人声平滑)
    constexpr float kRfixedLimit    = 68.0e3f;  // Limit 模式分压定臂 (68kΩ, 约 10:1+ 峰值限制)
    constexpr float kResistGamma    = 5.5f;     // CdS 光敏指数斜率
    constexpr float kFastAttackSec  = 0.010f;   // T4 荧光快响应启动 10ms
    constexpr float kSlowAttackSec  = 0.200f;   // T4 磷光深阱慢响应启动 200ms
    constexpr float kFastRelSec     = 0.080f;   // 快分量释放 80ms
    constexpr float kBaseSlowRelSec = 1.5f;     // 慢分量基础释放 1.5s
    constexpr float kElThresholdDb  = -24.0f;   // EL 发光面板起辉阈值
}

// ---- 参数 ID 字符串（永不改名）----
namespace IDs
{
    constexpr auto OPTO_STEREO_LINK = "opto_stereo_link";
    constexpr auto OPTO_OUTPUT_TRIM = "opto_output_trim";
    constexpr auto VCA_STEREO_LINK = "vca_stereo_link";
    constexpr auto VCA_OUTPUT_TRIM = "vca_output_trim";
    constexpr auto FET_STEREO_LINK = "fet_stereo_link";
    constexpr auto FET_OUTPUT_TRIM = "fet_output_trim";
    constexpr auto OPTO_SIDECHAIN_HP = "opto_sidechain_hp";
    constexpr auto FET_SIDECHAIN_HP = "fet_sidechain_hp";
    // 模式与核心压缩
    constexpr auto MODE           = "mode";
    constexpr auto THRESHOLD      = "threshold";
    constexpr auto RATIO          = "ratio";
    constexpr auto ATTACK         = "attack";
    constexpr auto RELEASE        = "release";
    constexpr auto KNEE           = "knee";
    constexpr auto MAKEUP         = "makeup";
    constexpr auto MIX            = "mix";
    constexpr auto FET_MIX        = "fet_mix";
    constexpr auto OPTO_MIX       = "opto_mix";
    constexpr auto OUTPUT_TRIM    = "output_trim";

    // FET 特有
    constexpr auto HEADROOM       = "headroom";
    constexpr auto IN_OUT_LINK    = "in_out_link";

    // 通用
    constexpr auto SIDECHAIN_ENABLED = "sidechain_enabled";
    constexpr auto SIDECHAIN_HP   = "sidechain_hp";
    constexpr auto STEREO_LINK    = "stereo_link";
    constexpr auto AUTO_GAIN      = "auto_gain";
    constexpr auto OVERSAMPLE     = "oversample";
    constexpr auto THEME          = "theme";
    constexpr auto GLOBAL_BYPASS  = "global_bypass";

    // FET 特有（1176，独立参数）
    constexpr auto FET_INPUT      = "fet_input";
    constexpr auto FET_RATIO      = "fet_ratio";
    constexpr auto FET_ATTACK     = "fet_attack";
    constexpr auto FET_RELEASE    = "fet_release";
    constexpr auto FET_OUTPUT     = "fet_output";

    // Opto 特有（LA-2A，独立参数）
    constexpr auto OPTO_PEAK_REDUCTION = "opto_peak_reduction";
    constexpr auto OPTO_GAIN           = "opto_gain";
    constexpr auto OPTO_IS_LIMIT       = "opto_is_limit";

    // VCA 独立参数（SSL 4000 G 总线，物理独立隔离）
    constexpr auto VCA_THRESHOLD       = "vca_threshold";
    constexpr auto VCA_RATIO           = "vca_ratio";
    constexpr auto VCA_ATTACK          = "vca_attack";
    constexpr auto VCA_RELEASE         = "vca_release";
    constexpr auto VCA_MAKEUP          = "vca_makeup";
    constexpr auto VCA_SIDECHAIN_HP    = "vca_sidechain_hp";
    constexpr auto VCA_MIX             = "vca_mix";

    // VOX 专属独立参数（对标 RVox：Gate / Comp / Gain / Mix）
    constexpr auto VOX_GATE            = "vox_gate";
    constexpr auto VOX_COMP            = "vox_comp";
    constexpr auto VOX_GAIN            = "vox_gain";
    constexpr auto VOX_MIX             = "vox_mix";
    constexpr auto VOX_SMOOTH          = "vox_smooth";
}

// ---- 模式名称 ----
inline const juce::StringArray& getModeNames()
{
    static const juce::StringArray names = {
        "VOX",
        "VCA",
        "FET",
        "Opto"
    };
    return names;
}

// ---- FET Ratio 固定档位名称（1176 按钮组，单击循环切换）----
inline const juce::StringArray& getFetRatioNames()
{
    static const juce::StringArray names = {
        "4:1",
        "8:1",
        "12:1",
        "20:1",
        "All"
    };
    return names;
}

// ---- 构建 APVTS 参数布局 ----
inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace Ranges;
    using namespace Defaults;

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // 阈值：线性（dB 本身已是对数单位）
    auto thresholdRange = juce::NormalisableRange<float>(
        THRESHOLD_MIN, THRESHOLD_MAX, 0.1f);

    // 比率：对数偏斜，低比率端更精细（1:1~4:1 是常用区）
    auto ratioRange = juce::NormalisableRange<float>(
        RATIO_MIN, RATIO_MAX, 0.01f, 0.4f);

    // 启动/释放：对数偏斜，短时间端更精细
    auto attackRange = juce::NormalisableRange<float>(
        ATTACK_MIN, ATTACK_MAX, 0.01f, 0.35f);
    auto releaseRange = juce::NormalisableRange<float>(
        RELEASE_MIN, RELEASE_MAX, 0.1f, 0.35f);

    // 拐点：线性
    auto kneeRange = juce::NormalisableRange<float>(KNEE_MIN, KNEE_MAX, 0.1f);

    // 增益补偿 / 余量：线性
    auto makeupRange = juce::NormalisableRange<float>(MAKEUP_MIN, MAKEUP_MAX, 0.1f);
    auto headroomRange = juce::NormalisableRange<float>(
        HEADROOM_MIN, HEADROOM_MAX, 1.0f);

    // 干湿比 / 立体声链接：线性百分比
    auto mixRange = juce::NormalisableRange<float>(MIX_MIN, MIX_MAX, 0.1f);
    auto linkRange = juce::NormalisableRange<float>(STEREO_LINK_MIN, STEREO_LINK_MAX, 0.1f);

    // 侧链高通：对数偏斜，低频端精细
    auto sidechainHpRange = juce::NormalisableRange<float>(
        SIDECHAIN_HP_MIN, SIDECHAIN_HP_MAX, 1.0f, 0.35f);

    // ---- 模式 ----
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        IDs::MODE, "Compressor Mode", getModeNames(), ModeVCA));

    // ---- 核心压缩参数 ----
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::THRESHOLD, "Threshold", thresholdRange, THRESHOLD));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::RATIO, "Ratio", ratioRange, RATIO));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::ATTACK, "Attack", attackRange, ATTACK));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::RELEASE, "Release", releaseRange, RELEASE));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::KNEE, "Knee", kneeRange, KNEE));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::MAKEUP, "Makeup", makeupRange, MAKEUP));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::MIX, "Mix", mixRange, MIX));

    auto outputTrimRange = juce::NormalisableRange<float>(
        OUTPUT_TRIM_MIN, OUTPUT_TRIM_MAX, 0.1f);
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::OUTPUT_TRIM, "Output Trim", outputTrimRange, OUTPUT_TRIM));

    // ---- FET 特有 ----
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::HEADROOM, "Headroom", headroomRange, HEADROOM));

    // ---- FET 独立参数（1176：Input/Output 0~40 + 5 联排扣 Ratio + 1~7 档位时间）----
    auto fetInputRange = juce::NormalisableRange<float>(
        FET_INPUT_MIN, FET_INPUT_MAX, 0.1f);
    auto fetAttackRange = juce::NormalisableRange<float>(
        FET_ATTACK_MIN, FET_ATTACK_MAX, 0.1f);
    auto fetReleaseRange = juce::NormalisableRange<float>(
        FET_RELEASE_MIN, FET_RELEASE_MAX, 0.1f);
    auto fetOutputRange = juce::NormalisableRange<float>(
        FET_OUTPUT_MIN, FET_OUTPUT_MAX, 0.1f);

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::FET_INPUT, "FET Input", fetInputRange, FET_INPUT));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        IDs::FET_RATIO, "FET Ratio", getFetRatioNames(), 0));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::FET_ATTACK, "FET Attack", fetAttackRange, FET_ATTACK));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::FET_RELEASE, "FET Release", fetReleaseRange, FET_RELEASE));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::FET_OUTPUT, "FET Output", fetOutputRange, FET_OUTPUT));

    // ---- Opto 独立参数（LA-2A：Peak Reduction + Gain 双旋钮）----
    auto optoPeakRange = juce::NormalisableRange<float>(
        OPTO_PEAK_REDUCTION_MIN, OPTO_PEAK_REDUCTION_MAX, 0.1f);
    auto optoGainRange = juce::NormalisableRange<float>(
        OPTO_GAIN_MIN, OPTO_GAIN_MAX, 0.1f);

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::OPTO_PEAK_REDUCTION, "Opto Peak Reduction", optoPeakRange,
        OPTO_PEAK_REDUCTION));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::OPTO_GAIN, "Opto Gain", optoGainRange, OPTO_GAIN));
    layout.add(std::make_unique<juce::AudioParameterBool>(
        IDs::OPTO_IS_LIMIT, "Opto Mode (Compress/Limit)", Defaults::OPTO_IS_LIMIT));

    // ---- VCA 独立参数（SSL 4000 G 总线：独立阈值/3档比率/Attack/Release/SC/Makeup/Mix）----
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::VCA_THRESHOLD, "VCA Threshold", thresholdRange, 0.0f)); // 首次挂入默认 0.0dB 未压缩
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::VCA_RATIO, "VCA Ratio", ratioRange, 2.0f)); // 默认 2:1 经典温和总线粘合
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::VCA_ATTACK, "VCA Attack", attackRange, 30.0f)); // 默认 30ms 保留打击瞬态
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::VCA_RELEASE, "VCA Release", releaseRange, 150.0f)); // 默认 150ms 经典释放
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::VCA_MAKEUP, "VCA Makeup", makeupRange, 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::VCA_SIDECHAIN_HP, "VCA Sidechain HP", sidechainHpRange, 0.0f)); // 默认 OFF
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::VCA_MIX, "VCA Mix", mixRange, 100.0f));

    // ---- VOX 独立参数（对标 RVox：Gate -80~0dB / Comp -36~0dB / Gain -36~0dB / Mix 0~100%）----
    auto voxGateRange = juce::NormalisableRange<float>(-80.0f, 0.0f, 0.1f);
    auto voxCompRange = juce::NormalisableRange<float>(-36.0f, 0.0f, 0.1f);
    auto voxGainRange = juce::NormalisableRange<float>(-36.0f, 0.0f, 0.1f);

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::VOX_GATE, "VOX Gate", voxGateRange, -80.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::VOX_COMP, "VOX Comp", voxCompRange, 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::VOX_GAIN, "VOX Gain", voxGainRange, 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::VOX_MIX,  "VOX Mix",  mixRange,     100.0f));

    // ---- 通用 ----
    layout.add(std::make_unique<juce::AudioParameterBool>(
        IDs::SIDECHAIN_ENABLED, "Sidechain Enabled", false));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::SIDECHAIN_HP, "Sidechain HP", sidechainHpRange, SIDECHAIN_HP));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::STEREO_LINK, "Stereo Link", linkRange, STEREO_LINK));
    layout.add(std::make_unique<juce::AudioParameterBool>(
        IDs::IN_OUT_LINK, "In/Out Link", false));
    layout.add(std::make_unique<juce::AudioParameterBool>(
        IDs::AUTO_GAIN, "Auto Gain", false));
    layout.add(std::make_unique<juce::AudioParameterBool>(
        IDs::OVERSAMPLE, "Oversample", true));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        IDs::THEME, "UI Theme", juce::StringArray{ "Light", "Dark" }, 0));
    layout.add(std::make_unique<juce::AudioParameterBool>(
        IDs::GLOBAL_BYPASS, "Global Bypass", false));

    layout.add(std::make_unique<juce::AudioParameterFloat>(IDs::FET_MIX, "FET MIX", mixRange, MIX));
    layout.add(std::make_unique<juce::AudioParameterFloat>(IDs::OPTO_MIX, "OPTO MIX", mixRange, MIX));
    layout.add(std::make_unique<juce::AudioParameterFloat>(IDs::FET_SIDECHAIN_HP, "FET SIDECHAIN HP", sidechainHpRange, SIDECHAIN_HP));
    layout.add(std::make_unique<juce::AudioParameterFloat>(IDs::OPTO_SIDECHAIN_HP, "OPTO SIDECHAIN HP", sidechainHpRange, SIDECHAIN_HP));
    layout.add(std::make_unique<juce::AudioParameterFloat>(IDs::FET_OUTPUT_TRIM, "FET OUTPUT TRIM", outputTrimRange, OUTPUT_TRIM));
    layout.add(std::make_unique<juce::AudioParameterFloat>(IDs::FET_STEREO_LINK, "FET STEREO LINK", linkRange, STEREO_LINK));
    layout.add(std::make_unique<juce::AudioParameterFloat>(IDs::VCA_OUTPUT_TRIM, "VCA OUTPUT TRIM", outputTrimRange, OUTPUT_TRIM));
    layout.add(std::make_unique<juce::AudioParameterFloat>(IDs::VCA_STEREO_LINK, "VCA STEREO LINK", linkRange, STEREO_LINK));
    layout.add(std::make_unique<juce::AudioParameterFloat>(IDs::OPTO_OUTPUT_TRIM, "OPTO OUTPUT TRIM", outputTrimRange, OUTPUT_TRIM));
    layout.add(std::make_unique<juce::AudioParameterFloat>(IDs::OPTO_STEREO_LINK, "OPTO STEREO LINK", linkRange, STEREO_LINK));
    // 新参数追加到末尾，保留已有宿主参数次序；旧工程默认使用零延迟。
    layout.add(std::make_unique<juce::AudioParameterBool>(IDs::VOX_SMOOTH, "LEV Smooth", false));
    return layout;
}


inline juce::ValueTree migrateLegacyState(juce::ValueTree state)
{
    if (!state.getChildWithProperty("id", IDs::VOX_SMOOTH).isValid())
    {
        juce::ValueTree child("PARAM");
        child.setProperty("id", IDs::VOX_SMOOTH, nullptr);
        child.setProperty("value", 0.0f, nullptr);
        state.appendChild(child, nullptr);
    }
    // 旧参数只用于一次性导入，运行时不再驱动多个模式。
    if (!state.getChildWithProperty("id", "fet_mix").isValid())
    {
        auto old = state.getChildWithProperty("id", "mix");
        juce::ValueTree child("PARAM");
        child.setProperty("id", "fet_mix", nullptr);
        child.setProperty("value", old.isValid() ? old.getProperty("value") : juce::var(100.0f), nullptr);
        state.appendChild(child, nullptr);
    }
    if (!state.getChildWithProperty("id", "opto_mix").isValid())
    {
        auto old = state.getChildWithProperty("id", "mix");
        juce::ValueTree child("PARAM");
        child.setProperty("id", "opto_mix", nullptr);
        child.setProperty("value", old.isValid() ? old.getProperty("value") : juce::var(100.0f), nullptr);
        state.appendChild(child, nullptr);
    }
    if (!state.getChildWithProperty("id", "fet_sidechain_hp").isValid())
    {
        auto old = state.getChildWithProperty("id", "sidechain_hp");
        juce::ValueTree child("PARAM");
        child.setProperty("id", "fet_sidechain_hp", nullptr);
        child.setProperty("value", old.isValid() ? old.getProperty("value") : juce::var(60.0f), nullptr);
        state.appendChild(child, nullptr);
    }
    if (!state.getChildWithProperty("id", "opto_sidechain_hp").isValid())
    {
        auto old = state.getChildWithProperty("id", "sidechain_hp");
        juce::ValueTree child("PARAM");
        child.setProperty("id", "opto_sidechain_hp", nullptr);
        child.setProperty("value", old.isValid() ? old.getProperty("value") : juce::var(60.0f), nullptr);
        state.appendChild(child, nullptr);
    }
    if (!state.getChildWithProperty("id", "fet_output_trim").isValid())
    {
        auto old = state.getChildWithProperty("id", "output_trim");
        juce::ValueTree child("PARAM");
        child.setProperty("id", "fet_output_trim", nullptr);
        child.setProperty("value", old.isValid() ? old.getProperty("value") : juce::var(0.0f), nullptr);
        state.appendChild(child, nullptr);
    }
    if (!state.getChildWithProperty("id", "fet_stereo_link").isValid())
    {
        auto old = state.getChildWithProperty("id", "stereo_link");
        juce::ValueTree child("PARAM");
        child.setProperty("id", "fet_stereo_link", nullptr);
        child.setProperty("value", old.isValid() ? old.getProperty("value") : juce::var(100.0f), nullptr);
        state.appendChild(child, nullptr);
    }
    if (!state.getChildWithProperty("id", "vca_output_trim").isValid())
    {
        auto old = state.getChildWithProperty("id", "output_trim");
        juce::ValueTree child("PARAM");
        child.setProperty("id", "vca_output_trim", nullptr);
        child.setProperty("value", old.isValid() ? old.getProperty("value") : juce::var(0.0f), nullptr);
        state.appendChild(child, nullptr);
    }
    if (!state.getChildWithProperty("id", "vca_stereo_link").isValid())
    {
        auto old = state.getChildWithProperty("id", "stereo_link");
        juce::ValueTree child("PARAM");
        child.setProperty("id", "vca_stereo_link", nullptr);
        child.setProperty("value", old.isValid() ? old.getProperty("value") : juce::var(100.0f), nullptr);
        state.appendChild(child, nullptr);
    }
    if (!state.getChildWithProperty("id", "opto_output_trim").isValid())
    {
        auto old = state.getChildWithProperty("id", "output_trim");
        juce::ValueTree child("PARAM");
        child.setProperty("id", "opto_output_trim", nullptr);
        child.setProperty("value", old.isValid() ? old.getProperty("value") : juce::var(0.0f), nullptr);
        state.appendChild(child, nullptr);
    }
    if (!state.getChildWithProperty("id", "opto_stereo_link").isValid())
    {
        auto old = state.getChildWithProperty("id", "stereo_link");
        juce::ValueTree child("PARAM");
        child.setProperty("id", "opto_stereo_link", nullptr);
        child.setProperty("value", old.isValid() ? old.getProperty("value") : juce::var(100.0f), nullptr);
        state.appendChild(child, nullptr);
    }
    return state;
}

} // namespace Params
