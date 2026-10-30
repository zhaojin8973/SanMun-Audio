// ==============================================================================
// ReverbProcessor.h - 顶层混响调度器 (4 引擎管理 + 50ms 等功率切模 + 遥测)
// ==============================================================================

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <memory>
#include <vector>

#include "engines/ReverbEngine.h"
#include "engines/PlateEngine.h"
#include "engines/RoomEngine.h"
#include "engines/HallEngine.h"
#include "engines/ChamberEngine.h"
#include "VisualMeter.h"

class ReverbProcessor
{
public:
    ReverbProcessor();
    ~ReverbProcessor() = default;

    void prepare(double sampleRate, int maxBlockSize);
    void reset();
    void process(juce::AudioBuffer<float>& buffer, const dsp_engines::ReverbParameters& params, bool bypass = false);
    VisualMeter visualMeter;

    // ---- 遥测数据接口 (无锁原子) ----
    float getInputPeakL() const noexcept  { return inPeakL.load(std::memory_order_relaxed); }
    float getInputPeakR() const noexcept  { return inPeakR.load(std::memory_order_relaxed); }
    float getOutputPeakL() const noexcept { return outPeakL.load(std::memory_order_relaxed); }
    float getOutputPeakR() const noexcept { return outPeakR.load(std::memory_order_relaxed); }
    float getReverbEnergy() const noexcept { return reverbEnergy.load(std::memory_order_relaxed); }

private:
    double sr = 48000.0;
    int maxBlock = 0;
    double bypassGain = 0, bypassStep = 0;

    // 同类切换模式也需要两份独立状态；所有实例只在构造和 prepare 阶段分配。
    std::unique_ptr<dsp_engines::ReverbEngine> engines[2][4];
    int activeBank = 0;

    int activeCategory = -1;
    int activeMode = -1;

    // 50ms 等功率交叉淡化状态
    bool isCrossfading = false;
    int previousCategory = 0;
    int previousBank = 0;
    dsp_engines::ReverbParameters activeParameters;
    dsp_engines::ReverbParameters previousParameters;
    int crossfadeSamples = 0;
    int crossfadeProgress = 0;
    std::vector<float> crossfadeOldGains, crossfadeNewGains;

    // 20ms 连续等功率混合：以旋转递推增益，避免在逐样本路径计算三角函数。
    bool mixInitialised = false;
    float mixTarget = 1.0f;
    int mixRampSamples = 1;
    int mixSamplesRemaining = 0;
    double mixAngle = 0.0;
    double mixAngleStep = 0.0;
    double mixDryGain = 0.0;
    double mixWetGain = 1.0;
    double mixStepCos = 1.0;
    double mixStepSin = 0.0;

    void setMixTarget(float mixPercent) noexcept;
    void advanceMix() noexcept;

    // 内部临时缓冲区
    std::vector<float> dryL, dryR;
    std::vector<float> curL, curR;
    std::vector<float> prevL, prevR;

    std::atomic<float> inPeakL { 0.0f };
    std::atomic<float> inPeakR { 0.0f };
    std::atomic<float> outPeakL { 0.0f };
    std::atomic<float> outPeakR { 0.0f };
    std::atomic<float> reverbEnergy { 0.0f };
};
