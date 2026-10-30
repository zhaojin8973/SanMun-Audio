// ==============================================================================
// DelayProcessor.h - 核心延迟系统协调器与音频处理总线
//
// 职责：
//   1. 统筹调度 4 大专属独立引擎（Modern, Tape, BBD, Diffused）
//   2. 宿主 BPM 速度跟随与自由毫秒转换，带抗杂音平滑器
//   3. 外部 Sidechain 总线与内部干声智能 Ducking
//   4. 全局等功率干湿混合、输出增益与安全旁通
// ==============================================================================

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "engines/ModernEngine.h"
#include "engines/TapeEngine.h"
#include "engines/BbdEngine.h"
#include "engines/DiffusedEngine.h"
#include "primitives/DuckingDetector.h"
#include "../params/Parameters.h"

namespace dsp
{

class DelayProcessor
{
public:
    DelayProcessor();
    ~DelayProcessor() = default;

    void prepare(double sampleRate, int maxBlockSize);
    void reset();

    // 核心立体声主处理流程（含外部侧链总线）
    void process(juce::AudioBuffer<float>& mainBuffer,
                 const juce::AudioBuffer<float>* sidechainBuffer,
                 const juce::AudioPlayHead* playHead,
                 const juce::AudioProcessorValueTreeState& apvts) noexcept;

    [[nodiscard]] float getCurrentDuckingGrDb() const noexcept
    {
        return duckingDetector.getReportedGrDb();
    }

    [[nodiscard]] float getInputPeakL() const noexcept { return inputPeakL.load(std::memory_order_relaxed); }
    [[nodiscard]] float getInputPeakR() const noexcept { return inputPeakR.load(std::memory_order_relaxed); }
    [[nodiscard]] float getOutputPeakL() const noexcept { return outputPeakL.load(std::memory_order_relaxed); }
    [[nodiscard]] float getOutputPeakR() const noexcept { return outputPeakR.load(std::memory_order_relaxed); }
    [[nodiscard]] float getWetPeakL() const noexcept { return wetPeakL.load(std::memory_order_relaxed); }
    [[nodiscard]] float getWetPeakR() const noexcept { return wetPeakR.load(std::memory_order_relaxed); }
    [[nodiscard]] double getLastValidBpm() const noexcept { return lastValidBpm.load(std::memory_order_relaxed); }

private:
    double sampleRate { 48000.0 };

    // 4 大物理独立引擎实例
    dsp_engines::ModernEngine   engineModern;
    dsp_engines::TapeEngine     engineTape;
    dsp_engines::BbdEngine      engineBbd;
    dsp_engines::DiffusedEngine engineDiffused;

    // 侧链避让检波器
    dsp_primitives::DuckingDetector duckingDetector;

    // 实时电平遥测（无锁原子更新）
    std::atomic<float> inputPeakL  { -100.0f };
    std::atomic<float> inputPeakR  { -100.0f };
    std::atomic<float> outputPeakL { -100.0f };
    std::atomic<float> outputPeakR { -100.0f };
    std::atomic<float> wetPeakL    { -100.0f };
    std::atomic<float> wetPeakR    { -100.0f };

    // 延迟时间平滑器（消除突变撕裂与爆音）
    float smoothedDelaySamplesL { 24000.0f };
    float smoothedDelaySamplesR { 24000.0f };
    float timeSmoothCoeff       { 0.999f };

    // 记忆最近一次有效宿主 BPM
    std::atomic<double> lastValidBpm { 120.0 };

    // 通用立体声正交低频调制器 (LFO Phase [0.0, 1.0))
    float lfoPhase { 0.0f };
};

} // namespace dsp
