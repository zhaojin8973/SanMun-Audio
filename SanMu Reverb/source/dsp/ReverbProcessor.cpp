// ==============================================================================
// ReverbProcessor.cpp - 顶层混响调度器实现
// ==============================================================================

#include "ReverbProcessor.h"
#include <cmath>
#include <algorithm>

namespace
{
float finiteInRange(float value, float low, float high, float fallback) noexcept
{
    return std::isfinite(value) ? juce::jlimit(low, high, value) : fallback;
}

dsp_engines::ReverbParameters sanitiseParameters(dsp_engines::ReverbParameters p) noexcept
{
    const dsp_engines::ReverbParameters defaults;
    p.category = juce::jlimit(0, 3, p.category);
    p.mode = juce::jlimit(0, 2, p.mode);
    p.decay = finiteInRange(p.decay, 0.2f, 20.0f, defaults.decay);
    p.predelay = finiteInRange(p.predelay, 0.0f, 250.0f, defaults.predelay);
    p.mix = finiteInRange(p.mix, 0.0f, 100.0f, defaults.mix);
    p.highFreq = finiteInRange(p.highFreq, 1000.0f, 20000.0f, defaults.highFreq);
    p.highShelf = finiteInRange(p.highShelf, -24.0f, 0.0f, defaults.highShelf);
    p.bassFreq = finiteInRange(p.bassFreq, 50.0f, 1000.0f, defaults.bassFreq);
    p.bassMult = finiteInRange(p.bassMult, 0.25f, 4.0f, defaults.bassMult);
    p.size = finiteInRange(p.size, 0.0f, 100.0f, defaults.size);
    p.attack = finiteInRange(p.attack, 0.0f, 100.0f, defaults.attack);
    p.earlyDiff = finiteInRange(p.earlyDiff, 0.0f, 100.0f, defaults.earlyDiff);
    p.lateDiff = finiteInRange(p.lateDiff, 0.0f, 100.0f, defaults.lateDiff);
    p.modRate = finiteInRange(p.modRate, 0.1f, 10.0f, defaults.modRate);
    p.modDepth = finiteInRange(p.modDepth, 0.0f, 100.0f, defaults.modDepth);
    p.lowCut = finiteInRange(p.lowCut, 10.0f, 500.0f, defaults.lowCut);
    p.highCut = finiteInRange(p.highCut, 1000.0f, 20000.0f, defaults.highCut);
    return p;
}
}

ReverbProcessor::ReverbProcessor()
{
    for (auto& bank : engines)
    {
        bank[0] = std::make_unique<dsp_engines::PlateEngine>();
        bank[1] = std::make_unique<dsp_engines::RoomEngine>();
        bank[2] = std::make_unique<dsp_engines::HallEngine>();
        bank[3] = std::make_unique<dsp_engines::ChamberEngine>();
    }
}

void ReverbProcessor::prepare(double sampleRate, int maxBlockSize)
{
    sr = std::isfinite(sampleRate) && sampleRate > 0.0 ? sampleRate : 48000.0;
    maxBlock = std::max(1, maxBlockSize);
    bypassStep = 1.0 / std::max(1.0, sr * .02);
    visualMeter.prepare(sr);

    for (auto* scratch : { &dryL, &dryR, &curL, &curR, &prevL, &prevR })
        scratch->assign(static_cast<size_t>(maxBlock), 0.0f);

    crossfadeSamples = std::max(2, static_cast<int>(sr * 0.05));
    crossfadeOldGains.resize(static_cast<size_t>(crossfadeSamples));
    crossfadeNewGains.resize(static_cast<size_t>(crossfadeSamples));
    for (int i = 0; i < crossfadeSamples; ++i)
    {
        const double angle = juce::MathConstants<double>::halfPi * i / (crossfadeSamples - 1);
        crossfadeOldGains[static_cast<size_t>(i)] = static_cast<float>(std::cos(angle));
        crossfadeNewGains[static_cast<size_t>(i)] = static_cast<float>(std::sin(angle));
    }
    crossfadeOldGains.back() = 0.0f;
    crossfadeNewGains.front() = 0.0f;
    mixRampSamples = std::max(1, static_cast<int>(sr * 0.02));

    for (auto& bank : engines)
        for (auto& engine : bank)
            engine->prepare(sr, maxBlock);

    reset();
}

void ReverbProcessor::reset()
{
    for (auto& bank : engines)
        for (auto& engine : bank)
            engine->reset();

    isCrossfading = false;
    crossfadeProgress = 0;
    activeBank = 0;
    activeCategory = -1;
    activeMode = -1;
    mixInitialised = false;
    mixSamplesRemaining = 0;
    bypassGain = 0;
    visualMeter.reset();

    inPeakL.store(0.0f, std::memory_order_relaxed);
    inPeakR.store(0.0f, std::memory_order_relaxed);
    outPeakL.store(0.0f, std::memory_order_relaxed);
    outPeakR.store(0.0f, std::memory_order_relaxed);
    reverbEnergy.store(0.0f, std::memory_order_relaxed);
}

void ReverbProcessor::setMixTarget(float mixPercent) noexcept
{
    const float target = mixPercent / 100.0f;
    if (!mixInitialised)
    {
        mixInitialised = true;
        mixTarget = target;
        mixAngle = target * juce::MathConstants<double>::halfPi;
        mixDryGain = target >= 1.0f ? 0.0 : std::cos(mixAngle);
        mixWetGain = target <= 0.0f ? 0.0 : std::sin(mixAngle);
        return;
    }

    // 相同目标不重启斜坡，保证自动化结果不依赖宿主块长。
    if (std::abs(target - mixTarget) < 1.0e-7f)
        return;

    mixTarget = target;
    mixSamplesRemaining = mixRampSamples;
    mixAngleStep = (target * juce::MathConstants<double>::halfPi - mixAngle) / mixRampSamples;
    mixStepCos = std::cos(mixAngleStep);
    mixStepSin = std::sin(mixAngleStep);
}

void ReverbProcessor::advanceMix() noexcept
{
    if (mixSamplesRemaining == 0)
        return;

    mixAngle += mixAngleStep;
    const double dry = mixDryGain * mixStepCos - mixWetGain * mixStepSin;
    mixWetGain = mixWetGain * mixStepCos + mixDryGain * mixStepSin;
    mixDryGain = dry;

    if (--mixSamplesRemaining == 0)
    {
        mixAngle = mixTarget * juce::MathConstants<double>::halfPi;
        mixDryGain = mixTarget >= 1.0f ? 0.0 : std::cos(mixAngle);
        mixWetGain = mixTarget <= 0.0f ? 0.0 : std::sin(mixAngle);
    }
}

void ReverbProcessor::process(juce::AudioBuffer<float>& buffer, const dsp_engines::ReverbParameters& params, bool bypass)
{
    const int numSamples = buffer.getNumSamples();
    if (numSamples <= 0 || buffer.getNumChannels() == 0 || maxBlock <= 0)
        return;

    const auto requested = sanitiseParameters(params);
    setMixTarget(requested.mix);
    float peakInL = 0.0f, peakInR = 0.0f;
    float peakOutL = 0.0f, peakOutR = 0.0f;
    double energySum = 0.0;
    const bool analyse = visualMeter.enabled.load(std::memory_order_relaxed);

    // 宿主的实际块长可能超过 prepare 的提示值；复用已分配缓冲区分段处理。
    for (int offset = 0; offset < numSamples;)
    {
        if (activeCategory < 0)
        {
            activeCategory = requested.category;
            activeMode = requested.mode;
        }
        else if (!isCrossfading && (requested.category != activeCategory || requested.mode != activeMode))
        {
            previousCategory = activeCategory;
            previousBank = activeBank;
            previousParameters = activeParameters;
            activeBank = 1 - activeBank;
            activeCategory = requested.category;
            activeMode = requested.mode;
            crossfadeProgress = 0;
            isCrossfading = true;
        }

        // 淡化未完成时保持这两条状态链；后续切换合并为当前请求，在淡化结束后执行。
        auto current = requested;
        current.category = activeCategory;
        current.mode = activeMode;
        activeParameters = current;

        int count = std::min(maxBlock, numSamples - offset);
        if (isCrossfading)
            count = std::min(count, crossfadeSamples - crossfadeProgress);

        const float* inputL = buffer.getReadPointer(0, offset);
        const float* inputR = buffer.getNumChannels() > 1 ? buffer.getReadPointer(1, offset) : inputL;
        for (int i = 0; i < count; ++i)
        {
            // 无效输入不能污染反馈状态；正常浮点音频保持原值。
            const float left = std::isfinite(inputL[i]) ? inputL[i] : 0.0f;
            const float right = std::isfinite(inputR[i]) ? inputR[i] : 0.0f;
            dryL[static_cast<size_t>(i)] = left;
            dryR[static_cast<size_t>(i)] = right;
            peakInL = std::max(peakInL, std::abs(left));
            peakInR = std::max(peakInR, std::abs(right));
        }

        engines[activeBank][activeCategory]->process(dryL.data(), dryR.data(), curL.data(), curR.data(), count, current);
        if (isCrossfading)
        {
            engines[previousBank][previousCategory]->process(dryL.data(), dryR.data(), prevL.data(), prevR.data(), count, previousParameters);
            for (int i = 0; i < count; ++i)
            {
                const auto index = static_cast<size_t>(i);
                const auto fadeIndex = static_cast<size_t>(crossfadeProgress++);
                curL[index] = prevL[index] * crossfadeOldGains[fadeIndex] + curL[index] * crossfadeNewGains[fadeIndex];
                curR[index] = prevR[index] * crossfadeOldGains[fadeIndex] + curR[index] * crossfadeNewGains[fadeIndex];
            }
            if (crossfadeProgress == crossfadeSamples)
            {
                isCrossfading = false;
                engines[previousBank][previousCategory]->reset();
            }
        }

        float* outputL = buffer.getWritePointer(0, offset);
        float* outputR = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1, offset) : nullptr;
        for (int i = 0; i < count; ++i)
        {
            advanceMix();
            const auto index = static_cast<size_t>(i);
            const float dryGain = static_cast<float>(mixDryGain);
            const float wetGain = static_cast<float>(mixWetGain);
            float left = dryL[index] * dryGain + curL[index] * wetGain;
            float right = dryR[index] * dryGain + curR[index] * wetGain;
            bypassGain = bypass ? std::min(1.0, bypassGain + bypassStep) : std::max(0.0, bypassGain - bypassStep);
            if (bypassGain > 0)
            {
                const float amount = static_cast<float>(bypassGain);
                left = left * (1 - amount) + dryL[index] * amount;
                right = right * (1 - amount) + dryR[index] * amount;
            }
            // 单声道总线折叠两边湿声，不丢弃右侧混响网络。
            outputL[i] = outputR != nullptr ? left : 0.5f * (left + right);
            if (outputR != nullptr)
                outputR[i] = right;
            peakOutL = std::max(peakOutL, std::abs(outputL[i]));
            peakOutR = std::max(peakOutR, std::abs(outputR != nullptr ? right : outputL[i]));
            energySum += std::abs(curL[index]) + std::abs(curR[index]);
            if (analyse) visualMeter.push(dryL[index], dryR[index], curL[index], curR[index],
                outputL[i], outputR != nullptr ? right : outputL[i]);
        }
        offset += count;
    }

    inPeakL.store(peakInL, std::memory_order_relaxed);
    inPeakR.store(peakInR, std::memory_order_relaxed);
    outPeakL.store(peakOutL, std::memory_order_relaxed);
    outPeakR.store(peakOutR, std::memory_order_relaxed);
    reverbEnergy.store(static_cast<float>(energySum / numSamples), std::memory_order_relaxed);
}
