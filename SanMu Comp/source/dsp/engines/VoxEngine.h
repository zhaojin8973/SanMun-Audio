#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <vector>

namespace dsp_engines
{
// 独立人声平准器：默认零延迟，可开启平滑模式，以 12 毫秒缓冲平滑完整增益轨迹。
// 主压缩与峰值保护选择更严格的增益，不重复叠加；GAIN 是独立输出衰减。
// 属于行为设计，不声称复刻 RVox 内部算法或光电器件。
class VoxEngine
{
public:
    VoxEngine();
    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();
    void process(juce::AudioBuffer<float>& buffer, bool bypass = false);
    void setGate(float db) noexcept { gateDb.store(db, std::memory_order_relaxed); }
    void setComp(float db) noexcept { compDb.store(db, std::memory_order_relaxed); }
    void setGain(float db) noexcept { gainDb.store(db, std::memory_order_relaxed); }
    void setMix(float percent) noexcept { mixPercent.store(percent, std::memory_order_relaxed); }
    void setSmooth(bool enabled) noexcept { smoothEnabled.store(enabled, std::memory_order_relaxed); }
    float getCurrentGrDb() const noexcept { return currentGrDb.load(std::memory_order_relaxed); }
    float getInputEnergyDb() const noexcept { return inputEnergyDb.load(std::memory_order_relaxed); }
    float getCompGrDb() const noexcept { return currentCompGrDb.load(std::memory_order_relaxed); }
    float getGateGrDb() const noexcept { return currentGateGrDb.load(std::memory_order_relaxed); }
    float getLimiterGrDb() const noexcept { return currentLimiterGrDb.load(std::memory_order_relaxed); }
    static int getLatencySamples(double sr = 48000.0, bool smooth = false) noexcept
    { return smooth ? std::max(1, int(std::lround((sr > 0 ? sr : 48000.0) * .012))) : 0; }

private:
    double sampleRate = 48000;
    std::atomic<float> gateDb {-80}, compDb {0}, gainDb {0}, mixPercent {100};
    std::atomic<bool> smoothEnabled {false};
    // 各状态只属于 LEV；两套轨迹分别统计主体压缩和最终允许增益。
    struct SmoothEnvelope
    {
        struct Entry { uint64_t index = 0; double gain = 1; };
        std::vector<Entry> queue;
        std::vector<double> average;
        size_t head = 0, count = 0, position = 0;
        uint64_t index = 0;
        double sum = 0;
        void prepare(int delay);
        void reset();
        double push(double allowedGain);
    } smoothMain, smoothTotal;
    std::array<std::vector<double>, 2> wetDelay;
    std::array<std::vector<float>, 2> dryDelay;
    std::vector<double> gateDelay;
    int delaySamples = 0, delayPosition = 0;
    double smoothBlend = 0;
    double limiterEnvelope = 1;
    std::array<double, 2> inputPower {}, compPower {};
    double gateReduction = 0, fastReduction = 0, slowReduction = 0;
    double smoothedAmount = 0, smoothedOutput = 0, smoothedWet = 1;
    std::atomic<float> currentGrDb {0}, inputEnergyDb {-100}, currentCompGrDb {0},
                       currentGateGrDb {0}, currentLimiterGrDb {0};
};
}
