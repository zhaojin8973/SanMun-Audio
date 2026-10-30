#pragma once
#include "TptSvf.h"
#include <atomic>

// 检测器与动态滤波器分离；立体声共享包络，控制更新间隔与宿主块大小无关。
class DynamicBand
{
public:
    void prepare(double rate)
    {
        sampleRate = rate;
        lastType = -1; lastFrequency = lastQ = -1;
        for (auto& f : detector) f.setSampleRate(rate);
        for (auto& f : filter) f.setSampleRate(rate);
        attack = std::exp(-1.0f / static_cast<float>(rate * .01));
        release = std::exp(-1.0f / static_cast<float>(rate * .15));
        tracking = std::exp(-1.0f / static_cast<float>(rate * 1.5));
        thresholdSmoothing = std::exp(-1.0f / static_cast<float>(rate * .04));
        controlInterval = std::max(1, static_cast<int>(rate / 3000.0));
        reset();
    }
    void reset()
    {
        for (auto& f : detector) f.reset();
        for (auto& f : filter) { f.reset(); f.setGainDb(0); }
        envelope = reduction = 0; countdown = 0; meter.store(0);
        learnedDb = -120; effectiveDb = thresholdDb; warmup = 0;
        detectorMeter.store(-120); thresholdMeter.store(thresholdDb); readyMeter.store(false);
    }
    void configure(int type, float frequency, float q, bool enabled, float range, float threshold, bool automatic = false, bool upward = false)
    {
        if (automatic != autoThreshold || (enabled && !active)) { learnedDb = -120; warmup = 0; }
        autoThreshold = automatic;
        upwardCompression = upward;
        active = enabled && type < 3;
        if (type >= 3) { reset(); return; }
        maxReduction = juce::jlimit(0.f, 24.f, range);
        thresholdDb = juce::jlimit(-60.f, 0.f, threshold);
        if (type == lastType && std::abs(frequency - lastFrequency) < .001f && std::abs(q - lastQ) < .00001f) return;
        // 连续扫频、调 Q 不重新触发预热，避免动态处理在拖动时反复停启。
        if (lastType != type) { learnedDb = -120; warmup = 0; }
        lastType = type; lastFrequency = frequency; lastQ = q;
        const auto eqType = type == 1 ? TptSvf::Type::LowShelf : type == 2 ? TptSvf::Type::HighShelf : TptSvf::Type::Bell;
        const auto detectType = type == 1 ? TptSvf::Type::LowPass : type == 2 ? TptSvf::Type::HighPass : TptSvf::Type::BandPass;
        detectorScale = type == 0 ? 1.f / q : 1.f;
        for (auto& f : detector) { f.setType(detectType); f.setCutoff(frequency); f.setQ(type == 0 ? q : .70710678f); }
        for (auto& f : filter) { f.setType(eqType); f.setCutoff(frequency); f.setQ(type == 0 ? q : q * .70710678f); f.setGainDb(-reduction); }
    }
    // 只运行检测器和包络，不改变输入音频；Linear Phase 用它生成动态 FIR 目标。
    float processControl(float left, float right, int channels = 2)
    {
        if (!active && std::abs(reduction) < .0001f) { meter.store(0, std::memory_order_relaxed); detectorMeter.store(-120, std::memory_order_relaxed); readyMeter.store(false, std::memory_order_relaxed); return 0.0f; }
        float level = std::abs(detector[0].processSample(left) * detectorScale);
        if (channels == 2) level = std::max(level, std::abs(detector[1].processSample(right) * detectorScale));
        const float coefficient = level > envelope ? attack : release;
        envelope = level + coefficient * (envelope - level);
        const float levelDb = juce::Decibels::gainToDecibels(envelope, -120.f);
        // 先用 100 ms 建立带内参考，再以 1.5 s 跟随平均电平；静音不把阈值拖向噪声底。
        const int warmupSamples = static_cast<int>(sampleRate * .1);
        if (autoThreshold && levelDb > -85)
        {
            if (warmup < warmupSamples) { learnedDb = std::max(learnedDb, levelDb); ++warmup; }
            else learnedDb = levelDb + tracking * (learnedDb - levelDb);
        }
        const bool ready = !autoThreshold || warmup >= warmupSamples;
        const float desiredDb = autoThreshold ? juce::jlimit(-84.f, 6.f, learnedDb + (upwardCompression ? 2.f : 1.f)) : thresholdDb;
        effectiveDb = autoThreshold && !ready ? desiredDb : desiredDb + thresholdSmoothing * (effectiveDb - desiredDb);
        const float threshold = autoThreshold ? effectiveDb : thresholdDb;
        const float over = upwardCompression ? threshold - levelDb : levelDb - threshold;
        // MANUAL 向下保留原来的 4:1；AUTO 用 1.5:1，并将默认 6 dB 范围收敛到 2.1 dB。
        const float slope = autoThreshold ? 1.f / 3.f : .75f;
        const float compressed = over <= -3 ? 0.f : over < 3 ? slope * (over + 3) * (over + 3) / 12.f : slope * over;
        const float limit = autoThreshold ? std::min(3.f, maxReduction * .35f) : maxReduction;
        // 向上压缩提升低于阈值的有效信号；-80 至 -60 dB 渐入，禁止提升静音和底噪。
        const float gate = upwardCompression ? juce::jlimit(0.f, 1.f, (levelDb + 80.f) / 20.f) : 1.f;
        const float magnitude = active && ready && (!autoThreshold || levelDb > -85) ? std::min(limit, compressed) * gate : 0.f;
        const float target = upwardCompression ? -magnitude : magnitude;
        reduction = target + (std::abs(target) > std::abs(reduction) ? attack : release) * (reduction - target);
        if (--countdown <= 0)
        {
            countdown = controlInterval;
            for (auto& f : filter) f.setGainDb(-reduction);
            meter.store(reduction, std::memory_order_relaxed);
            detectorMeter.store(levelDb, std::memory_order_relaxed);
            thresholdMeter.store(autoThreshold ? effectiveDb : thresholdDb, std::memory_order_relaxed);
            readyMeter.store(ready, std::memory_order_relaxed);
        }
        return reduction;
    }
    void process(float& left, float& right, int channels = 2)
    {
        processControl(left, right, channels);
        if (!active && std::abs(reduction) < .0001f)
            return;
        left = filter[0].processSample(left);
        if (channels == 2) right = filter[1].processSample(right);
    }
    float getReduction() const { return meter.load(std::memory_order_relaxed); }
    float getDynamicGainDb() const { return -reduction; }
    float getDetectorDb() const { return detectorMeter.load(std::memory_order_relaxed); }
    float getThresholdDb() const { return thresholdMeter.load(std::memory_order_relaxed); }
    bool isThresholdReady() const { return readyMeter.load(std::memory_order_relaxed); }
    bool isAutomatic() const { return autoThreshold; }
private:
    std::array<TptSvf, 2> detector, filter;
    std::atomic<float> meter{0};
    std::atomic<float> detectorMeter{-120}, thresholdMeter{-24};
    std::atomic<bool> readyMeter{false};
    double sampleRate = 96000;
    float attack = 0, release = 0, envelope = 0, reduction = 0;
    float tracking = 0, thresholdSmoothing = 0, learnedDb = -120, effectiveDb = -24;
    int warmup = 0;
    bool autoThreshold = false, upwardCompression = false;
    float maxReduction = 6, thresholdDb = -24, detectorScale = 1;
    float lastFrequency = -1, lastQ = -1;
    int lastType = -1, countdown = 0, controlInterval = 32;
    bool active = false;
};
