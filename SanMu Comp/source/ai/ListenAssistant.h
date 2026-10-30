#pragma once
#include "../dsp/AcousticFeatureExtractor.h"
#include <limits>

namespace ai
{
// targetGrDb 为 0 时自动决定深度；fixedMode 为 -1 时比较全部引擎。
struct ListenOptions
{
    float targetGrDb = 0;
    int fixedMode = -1;
};

// 声源只是选择偏好。每个候选都调用定稿的生产引擎，独立测量压缩深度与电平。
struct ListenResult
{
    AcousticFeatures features;
    CompressorPresetRecommendation preset;
    std::vector<CompressorPresetRecommendation> candidates;
    juce::String message;
    bool ready = false;
};

class ListenAssistant
{
public:
    static juce::String modeName(int mode)
    { return mode == Params::ModeVOX ? "LEV" : mode == Params::ModeVCA ? "VCA" : mode == Params::ModeFET ? "FET" : "Opto"; }

    static ListenResult analyse(const ListenCapture& capture, bool smooth,
                                AcousticSourceType correction = AcousticSourceType::Unknown,
                                const std::atomic<bool>* cancelled = nullptr, ListenOptions options = {})
    {
        ListenResult result;
        if (!std::isfinite(options.targetGrDb) || options.targetGrDb < 0
            || (options.targetGrDb > 0 && (options.targetGrDb < .5f || options.targetGrDb > 24))
            || options.fixedMode < -1 || options.fixedMode > Params::ModeOpto)
        { result.message = juce::String::fromUTF8("目标压缩量需为 0.5～24 dB，或选择自动深度。"); return result; }
        const bool manualTarget = options.targetGrDb > 0;
        const bool targetOnly = manualTarget && options.fixedMode >= 0;
        result.features = AcousticFeatureExtractor::analyseCapture(capture, !targetOnly);
        auto& f = result.features;
        if ((!targetOnly && f.sourceType == AcousticSourceType::Unknown)
            || !std::isfinite(f.peakDb) || f.peakDb <= -90 || f.durationSeconds < 1 || f.activeSeconds < .12f)
        { result.message = juce::String::fromUTF8("有效音频不足，请播放代表性段落后重新监听。"); return result; }
        if (!targetOnly && correction != AcousticSourceType::Unknown)
        {
            f.sourceType = correction;
            f.inferredSource = correction == AcousticSourceType::LeadVocal ? "人声（手动指定）"
                : correction == AcousticSourceType::SnareDrum ? "打击乐（手动指定）"
                : correction == AcousticSourceType::MasterMixBus ? "混音或总线（手动指定）" : "乐器（手动指定）";
        }
        const bool drum = !targetOnly && (f.sourceType == AcousticSourceType::KickDrum || f.sourceType == AcousticSourceType::SnareDrum);
        const bool vocal = !targetOnly && f.sourceType == AcousticSourceType::LeadVocal;
        const bool layered = !targetOnly && f.sourceType == AcousticSourceType::BackingVocals;
        const bool bus = !targetOnly && f.sourceType == AcousticSourceType::MasterMixBus;
        // 当前分类器没有校准过的置信度百分比；以显式歧义类别决定通用回退。
        const bool vcaFallback = options.fixedMode < 0
            && (layered || f.sourceType == AcousticSourceType::AmbiguousInstrument);
        // 明确人声／打击乐仍保留各自的快速起点；待确认或无法归类的素材
        // 以 VCA 作为通用、可回退的中性候选，再让实际校准误差参与竞争。
        const std::array<float, 4> prior = drum ? std::array<float,4>{2.8f,.55f,0,2.2f}
            : vocal ? std::array<float,4>{0,2,1.3f,.65f}
            : layered ? std::array<float,4>{1.4f,0,.95f,.45f}
            : bus ? std::array<float,4>{2,0,2.5f,1.6f}
            : std::array<float,4>{2.2f,0,1.35f,.45f};
        const float target = manualTarget ? options.targetGrDb : bus ? 1.8f : vocal ? std::clamp(3 + f.dynamicRangeDb * .12f, 3.5f, 5.5f)
            : drum ? std::clamp(2.5f + f.dynamicRangeDb * .08f, 3.0f, 4.5f)
                   : std::clamp(1.7f + f.dynamicRangeDb * .10f, 2.0f, 3.5f);
        float bestScore = std::numeric_limits<float>::max();
        const auto stopped = [&] { return cancelled && cancelled->load(); };
        auto evaluate = [&](int mode, float depth)
        {
            CompressorPresetRecommendation r;
            r.mode = mode; r.voxSmooth = smooth; r.voxGate = -80;
            r.fetAttack = 3; r.fetRelease = 4.5f; r.fetRatioIdx = 0;
            // 低频为主体时不让高通把有用检波信号大幅滤掉。
            const float low = f.subEnergyPct + f.bassEnergyPct;
            r.fetScHp = low > 60 ? 20.0f : 60.0f;
            r.optoScHp = low > 60 ? 20.0f : 60.0f;
            r.vcaScHp = bus ? 70.0f : low > 60 ? 20.0f : 60.0f;
            r.vcaRatio = drum ? 4.0f : 2.0f;
            r.vcaAttack = f.crestFactorDb > 10 ? 30.0f : 10.0f;
            r.vcaRelease = std::clamp(float(30000 / std::max(f.bpm, 40.0)), 120.0f, 300.0f);
            r.requestedGrDb = depth;
            r.detectedSource = targetOnly ? juce::String::fromUTF8("手动目标") : juce::String::fromUTF8(f.inferredSource.c_str());
            r.title = modeName(mode) + juce::String::fromUTF8(" · 动态起点");
            AcousticFeatureExtractor::calibrateCapture(capture, r, cancelled);
            if (stopped()) return;
            const float depthError = std::abs(r.measuredGrDb - depth);
            // 不能用几乎没有压缩的方案假装命中目标，也不能以深压缩换取配平。
            r.usable = r.usable && depthError <= (manualTarget ? std::clamp(depth * .12f, .25f, .75f) : 1.2f) && r.maximumGrDb <= depth + 9;
            result.candidates.push_back(r);
            const float levelError = std::abs(r.calibratedRmsDb - r.calibrationInputRmsDb);
            const float score = prior[size_t(mode)] + 2 * levelError + .7f * depthError
                              + .15f * std::max(0.0f, r.maximumGrDb - depth - 3)
                              + (r.usable ? 0 : 100);
            if (score < bestScore) { bestScore = score; result.preset = r; }
        };
        const int firstMode = options.fixedMode >= 0 ? options.fixedMode : vcaFallback ? Params::ModeVCA : 0;
        const int endMode = options.fixedMode >= 0 || vcaFallback ? firstMode + 1 : 4;
        for (int mode = firstMode; mode < endMode && !stopped(); ++mode) evaluate(mode, target);
        if (stopped()) return result;
        if (!result.preset.usable && !manualTarget)
        {
            // 峰值余量不足时只尝试更轻的动态起点，不收紧时间常数或额外限幅。
            for (int mode = firstMode; mode < endMode && !stopped(); ++mode) evaluate(mode, target * .65f);
        }
        if (stopped()) return result;
        result.ready = result.preset.usable;
        const auto& r = result.preset;
        result.message = targetOnly ? modeName(r.mode) + juce::String::fromUTF8(" / 手动目标")
                                    : r.detectedSource + " / " + modeName(r.mode);
        if (vcaFallback) result.message += juce::String::fromUTF8("\n声源判定不明确，采用 VCA 通用起点。");
        if (manualTarget) result.message += juce::String::fromUTF8("\n目标主体压缩 ") + juce::String(target, 1) + " dB";
        if (options.fixedMode >= 0) result.message += juce::String::fromUTF8("（固定当前引擎）");
        result.message +=  juce::String::fromUTF8("\n实测主体压缩约 ") + juce::String(r.measuredGrDb, 1)
            + juce::String::fromUTF8(" dB，最大约 ") + juce::String(r.maximumGrDb, 1)
            + juce::String::fromUTF8(" dB\n输出与输入电平差 ")
            + juce::String(r.calibratedRmsDb - r.calibrationInputRmsDb, 1) + " dB";
        result.message += result.ready ? juce::String::fromUTF8("\n这是可微调的起点；可重新测算或撤销。")
                                     : juce::String::fromUTF8("\n当前片段未得到合理起点，尚未应用。请更换片段或调整目标压缩量。");
        if (!result.ready && manualTarget)
            result.message += juce::String::fromUTF8("\n当前引擎／素材未同时满足目标和电平要求。目标未降低，参数未应用。");
        result.preset.rationale = result.message;
        return result;
    }
};
}
