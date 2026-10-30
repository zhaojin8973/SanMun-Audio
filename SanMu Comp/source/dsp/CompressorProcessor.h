// ==============================================================================
// CompressorProcessor.h - 模块化压缩器 DSP 核心调度器（无锁线程模型）
//
// 四大独立物理建模引擎（完全模块化隔离）：
//   1. CleanEngine: 纯数字透明前馈压缩器（连续软拐点 Knee、零失真）
//   2. VcaEngine:   模拟总线 Glue 前馈压缩器（真有效值对数检波、OverEasy 软拐、微量 THAT 2180 染色）
//   3. FetEngine:   1176 经典反馈压缩器（单向二极管峰值保持检波、20μs~800μs 极速 Attack、双阶段释放、UTC 变压器）
//   4. OptoEngine:  LA-2A 经典光学反馈压缩器（T4B 双阶段释放、CdS 历史记忆效应、EL 起压阈值、环外纯补偿）
//
// 线程约定：
//   - 音频线程：process() 读写内部状态，禁止锁与堆内存分配。
//   - UI 线程：只通过 std::atomic 读取 GR 与电平值。
// ==============================================================================

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>
#include <memory>
#include <limits>

#include "../params/Parameters.h"
#include "engines/CleanEngine.h"
#include "engines/VcaEngine.h"
#include "engines/FetEngine.h"
#include "engines/OptoEngine.h"
#include "engines/VoxEngine.h"
#include "AcousticFeatureExtractor.h"

class CompressorProcessor
{
public:
    CompressorProcessor();
    ~CompressorProcessor() = default;

    // ---- 生命周期 ----
    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();

    // ---- 音频处理 ----
    void process(juce::AudioBuffer<float>& buffer, double currentBpm = 120.0, int timeSigNum = 4, int timeSigDen = 4);

    // ---- 参数设置（音频线程调用，原子更新）----
    void setMode(int m);
    void setThreshold(float db);
    void setRatio(float r);
    void setAttack(float ms);
    void setRelease(float ms);
    void setKnee(float db);
    void setMakeup(float db);
    void setMix(float percent);
    void setOutputTrim(float db);
    void setHeadroom(float db);
    void setSidechainEnabled(bool e);
    void setSidechainHp(float hz);
    void setStereoLink(float percent);
    void setAutoGain(bool e);
    void setGlobalBypass(bool b);

    // ---- FET（1176）特有参数 ----
    void setFetDrive(float drive);
    void setFetRatioIndex(int idx);

    // ---- Opto（LA-2A）特有参数 ----
    void setOptoPeakReduction(float pr);
    void setOptoIsLimit(bool limit);

    // ---- VOX 特有参数 ----
    void setVoxGate(float db);
    void setVoxComp(float db);
    void setVoxGain(float db);
    void setVoxMix(float percent);
    void setVoxSmooth(bool enabled);

    // ---- UI 读取（原子，无锁）----
    /** 当前增益衰减量（dB，正数 = 压缩量），供 GR 表与动态包络波形绘制 */
    float getCurrentGainReductionDb() const   { return currentGrDb.load(std::memory_order_relaxed); }
    /** 输出峰值电平（dBFS） */
    float getOutputPeakL() const              { return outputPeakL.load(std::memory_order_relaxed); }
    float getOutputPeakR() const              { return outputPeakR.load(std::memory_order_relaxed); }
    /** 输出 RMS 电平（dBFS） */
    float getOutputRmsL() const               { return outputRmsL.load(std::memory_order_relaxed); }
    float getOutputRmsR() const               { return outputRmsR.load(std::memory_order_relaxed); }
    /** 输入峰值电平（dBFS） */
    float getInputPeakL() const               { return inputPeakL.load(std::memory_order_relaxed); }
    float getInputPeakR() const               { return inputPeakR.load(std::memory_order_relaxed); }

    // 单一界面读取者；负无穷表示尚无新块，必须与真实静音电平区分。
    std::array<float, 3> consumeMeterPeaks() noexcept
    {
        constexpr float empty = -std::numeric_limits<float>::infinity();
        return {meterInput.exchange(empty), meterOutput.exchange(empty), meterGr.exchange(empty)};
    }

    float getMeterIdleTimeoutMs() const noexcept
    {
        // 给两次实际音频块及一次界面刷新留出余量，兼容宿主的大块处理。
        return std::max(250.0f, 2.0f * meterBlockDurationMs.load(std::memory_order_relaxed) + 1000.0f / 60.0f);
    }

    // 提供对各子引擎的直接访问（用于精准测试与高级控制）
    dsp_engines::CleanEngine& getCleanEngine() noexcept { return cleanEngine; }
    dsp_engines::VcaEngine&   getVcaEngine()   noexcept { return vcaEngine; }
    dsp_engines::FetEngine&   getFetEngine()   noexcept { return fetEngine; }
    dsp_engines::OptoEngine&  getOptoEngine()  noexcept { return optoEngine; }
    dsp_engines::VoxEngine&   getVoxEngine()   noexcept { return voxEngine; }
    ai::AcousticFeatureExtractor& getAcousticExtractor() noexcept { return acousticExtractor; }

private:
    // ---- 4 大物理建模引擎实例（完全隔离）----
    dsp_engines::CleanEngine cleanEngine;
    dsp_engines::VcaEngine   vcaEngine;
    dsp_engines::FetEngine   fetEngine;
    dsp_engines::OptoEngine  optoEngine;
    dsp_engines::VoxEngine   voxEngine;

    ai::AcousticFeatureExtractor acousticExtractor;

    int processedMode = -1;

    // ---- 全局控制参数（音频线程读写）----
    std::atomic<int>  mode         { Params::ModeFET };
    std::atomic<bool> autoGain     { false };
    std::atomic<bool> globalBypass { false };

    std::atomic<float> meterInput{-100.0f}, meterOutput{-100.0f}, meterGr{0.0f};
    std::atomic<float> meterBlockDurationMs{0.0f};
    double meterSampleRate = 48000.0;

    // ---- UI 读写电平状态 ----
    std::atomic<float> currentGrDb  { 0.0f };
    std::atomic<float> outputPeakL  { -100.0f };
    std::atomic<float> outputPeakR  { -100.0f };
    std::atomic<float> outputRmsL   { -100.0f };
    std::atomic<float> outputRmsR   { -100.0f };
    std::atomic<float> inputPeakL   { -100.0f };
    std::atomic<float> inputPeakR   { -100.0f };
};
