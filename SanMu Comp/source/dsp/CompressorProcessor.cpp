#include "CompressorProcessor.h"

CompressorProcessor::CompressorProcessor()
{
    reset();
}

void CompressorProcessor::prepare(const juce::dsp::ProcessSpec& spec)
{
    meterSampleRate = spec.sampleRate;
    voxEngine.prepare(spec);
    cleanEngine.prepare(spec);
    vcaEngine.prepare(spec);
    fetEngine.prepare(spec);
    optoEngine.prepare(spec);
    acousticExtractor.prepare(spec.sampleRate, static_cast<int>(spec.maximumBlockSize));
    reset();
}

void CompressorProcessor::reset()
{
    processedMode = -1;
    meterInput = meterOutput = -100.0f;
    meterGr = 0.0f;
    meterBlockDurationMs.store(0.0f, std::memory_order_relaxed);
    voxEngine.reset();
    cleanEngine.reset();
    vcaEngine.reset();
    fetEngine.reset();
    optoEngine.reset();
    acousticExtractor.reset();

    currentGrDb.store(0.0f, std::memory_order_relaxed);
    outputPeakL.store(-100.0f, std::memory_order_relaxed);
    outputPeakR.store(-100.0f, std::memory_order_relaxed);
    outputRmsL.store(-100.0f, std::memory_order_relaxed);
    outputRmsR.store(-100.0f, std::memory_order_relaxed);
    inputPeakL.store(-100.0f, std::memory_order_relaxed);
    inputPeakR.store(-100.0f, std::memory_order_relaxed);
}

void CompressorProcessor::setMode(int m)
{
    mode.store(m, std::memory_order_relaxed);
}

void CompressorProcessor::setVoxGate(float db)
{
    voxEngine.setGate(db);
}

void CompressorProcessor::setVoxComp(float db)
{
    voxEngine.setComp(db);
}

void CompressorProcessor::setVoxGain(float db)
{
    voxEngine.setGain(db);
}

void CompressorProcessor::setVoxMix(float percent)
{
    voxEngine.setMix(percent);
}

void CompressorProcessor::setVoxSmooth(bool enabled)
{
    voxEngine.setSmooth(enabled);
}

void CompressorProcessor::setThreshold(float db)
{
    switch (mode.load(std::memory_order_relaxed))
    {
        case Params::ModeVCA: vcaEngine.thresholdDb.store(db, std::memory_order_relaxed); break;
        default: break;
    }
}

void CompressorProcessor::setRatio(float r)
{
    switch (mode.load(std::memory_order_relaxed))
    {
        case Params::ModeVCA: vcaEngine.ratio.store(r, std::memory_order_relaxed); break;
        default: break;
    }
}

void CompressorProcessor::setAttack(float ms)
{
    switch (mode.load(std::memory_order_relaxed))
    {
        case Params::ModeVCA: vcaEngine.attackMs.store(ms, std::memory_order_relaxed); break;
        case Params::ModeFET: fetEngine.attackMs.store(ms, std::memory_order_relaxed); break;
        default: break;
    }
}

void CompressorProcessor::setRelease(float ms)
{
    switch (mode.load(std::memory_order_relaxed))
    {
        case Params::ModeVCA: vcaEngine.releaseMs.store(ms, std::memory_order_relaxed); break;
        case Params::ModeFET: fetEngine.releaseMs.store(ms, std::memory_order_relaxed); break;
        default: break;
    }
}

void CompressorProcessor::setKnee(float db)
{
    juce::ignoreUnused(db); // 旧接口保留；活动引擎使用各自的固定拐点。
}

void CompressorProcessor::setMakeup(float db)
{
    switch (mode.load(std::memory_order_relaxed))
    {
        case Params::ModeVCA: vcaEngine.makeupDb.store(db, std::memory_order_relaxed); break;
        case Params::ModeFET: fetEngine.makeupDb.store(db, std::memory_order_relaxed); break;
        case Params::ModeOpto: optoEngine.makeupDb.store(db, std::memory_order_relaxed); break;
        default: break;
    }
}

void CompressorProcessor::setMix(float percent)
{
    switch (mode.load(std::memory_order_relaxed))
    {
        case Params::ModeVCA: vcaEngine.mixPercent.store(percent, std::memory_order_relaxed); break;
        case Params::ModeFET: fetEngine.mixPercent.store(percent, std::memory_order_relaxed); break;
        case Params::ModeOpto: optoEngine.mixPercent.store(percent, std::memory_order_relaxed); break;
        case Params::ModeVOX: voxEngine.setMix(percent); break;
        default: break;
    }
}

void CompressorProcessor::setOutputTrim(float db)
{
    switch (mode.load(std::memory_order_relaxed))
    {
        case Params::ModeVCA: vcaEngine.outputTrimDb.store(db, std::memory_order_relaxed); break;
        case Params::ModeFET: fetEngine.outputTrimDb.store(db, std::memory_order_relaxed); break;
        case Params::ModeOpto: optoEngine.outputTrimDb.store(db, std::memory_order_relaxed); break;
        default: break;
    }
}

void CompressorProcessor::setHeadroom(float db)
{
    switch (mode.load(std::memory_order_relaxed))
    {
        case Params::ModeFET: fetEngine.headroomDb.store(db, std::memory_order_relaxed); break;
        case Params::ModeOpto: optoEngine.headroomDb.store(db, std::memory_order_relaxed); break;
        default: break;
    }
}

void CompressorProcessor::setSidechainEnabled(bool e)
{
    switch (mode.load(std::memory_order_relaxed))
    {
        case Params::ModeVCA: vcaEngine.sidechainEnabled.store(e, std::memory_order_relaxed); break;
        case Params::ModeFET: fetEngine.sidechainEnabled.store(e, std::memory_order_relaxed); break;
        case Params::ModeOpto: optoEngine.sidechainEnabled.store(e, std::memory_order_relaxed); break;
        default: break;
    }
}

void CompressorProcessor::setSidechainHp(float hz)
{
    switch (mode.load(std::memory_order_relaxed))
    {
        case Params::ModeVCA: vcaEngine.sidechainHpHz.store(hz, std::memory_order_relaxed); break;
        case Params::ModeFET: fetEngine.sidechainHpHz.store(hz, std::memory_order_relaxed); break;
        case Params::ModeOpto: optoEngine.sidechainHpHz.store(hz, std::memory_order_relaxed); break;
        default: break;
    }
}

void CompressorProcessor::setStereoLink(float percent)
{
    switch (mode.load(std::memory_order_relaxed))
    {
        case Params::ModeVCA: vcaEngine.stereoLink.store(percent, std::memory_order_relaxed); break;
        case Params::ModeFET: fetEngine.stereoLink.store(percent, std::memory_order_relaxed); break;
        case Params::ModeOpto: optoEngine.stereoLink.store(percent, std::memory_order_relaxed); break;
        default: break;
    }
}

void CompressorProcessor::setAutoGain(bool e)
{
    autoGain.store(e, std::memory_order_relaxed);
}

void CompressorProcessor::setGlobalBypass(bool b)
{
    globalBypass.store(b, std::memory_order_relaxed);
}

void CompressorProcessor::setFetDrive(float drive)
{
    fetEngine.fetDrive.store(drive, std::memory_order_relaxed);
}

void CompressorProcessor::setFetRatioIndex(int idx)
{
    fetEngine.fetRatioIdx.store(idx, std::memory_order_relaxed);
}

void CompressorProcessor::setOptoPeakReduction(float pr)
{
    optoEngine.optoPeakReduction.store(pr, std::memory_order_relaxed);
}

void CompressorProcessor::setOptoIsLimit(bool limit)
{
    optoEngine.isLimit.store(limit, std::memory_order_relaxed);
}

void CompressorProcessor::process(juce::AudioBuffer<float>& buffer, double currentBpm, int timeSigNum, int timeSigDen)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = std::min(buffer.getNumChannels(), 2);
    if (numSamples <= 0 || numChannels <= 0)
        return;

    // 实时声学特征捕获（供 AI Listen 模块使用，内部零锁无阻塞）
    acousticExtractor.processBlock(buffer, currentBpm, timeSigNum, timeSigDen);

    // 1. 采集输入峰值电平（供电平表使用）
    float inPeakL = 0.0f, inPeakR = 0.0f;
    const auto* readL = buffer.getReadPointer(0);
    const auto* readR = (numChannels > 1) ? buffer.getReadPointer(1) : readL;

    for (int n = 0; n < numSamples; ++n)
    {
        inPeakL = std::max(inPeakL, std::fabs(readL[n]));
        inPeakR = std::max(inPeakR, std::fabs(readR[n]));
    }
    inputPeakL.store(juce::Decibels::gainToDecibels(inPeakL), std::memory_order_relaxed);
    inputPeakR.store(juce::Decibels::gainToDecibels(inPeakR), std::memory_order_relaxed);

    const bool bypass = globalBypass.load(std::memory_order_relaxed);
    // 2. 调度至对应独立压缩引擎
    const int activeMode = mode.load(std::memory_order_relaxed);
    if (processedMode != activeMode)
    {
        // 只重置即将激活的模式，防止旧包络在切换时回放。
        switch (activeMode)
        {
            case Params::ModeVOX: voxEngine.reset(); break;
            case Params::ModeVCA: vcaEngine.reset(); break;
            case Params::ModeFET: fetEngine.reset(); break;
            case Params::ModeOpto: optoEngine.reset(); break;
        }
        processedMode = activeMode;
    }
    constexpr int chunkSize = 512;
    std::array<std::array<float, chunkSize>, 2> dry {};
    for (int offset = 0; offset < numSamples; offset += chunkSize)
    {
        const int count = std::min(chunkSize, numSamples - offset);
        if (bypass)
            for (int c = 0; c < numChannels; ++c)
                std::copy_n(buffer.getReadPointer(c, offset), count, dry[static_cast<size_t>(c)].data());
        float* channels[] = {buffer.getWritePointer(0, offset), numChannels > 1 ? buffer.getWritePointer(1, offset) : nullptr};
        juce::AudioBuffer<float> chunk(channels, numChannels, count);
        switch (activeMode)
        {
            case Params::ModeVOX:
                voxEngine.process(chunk, bypass);
                currentGrDb.store(voxEngine.getCurrentGrDb(), std::memory_order_relaxed);
                break;

            case Params::ModeVCA:
                vcaEngine.process(chunk);
                currentGrDb.store(vcaEngine.getCurrentGrDb(), std::memory_order_relaxed);
                break;

            case Params::ModeFET:
                fetEngine.process(chunk);
                currentGrDb.store(fetEngine.getCurrentGrDb(), std::memory_order_relaxed);
                break;

            case Params::ModeOpto:
                optoEngine.process(chunk);
                currentGrDb.store(optoEngine.getCurrentGrDb(), std::memory_order_relaxed);
                break;

            default:
                voxEngine.process(chunk, bypass);
                currentGrDb.store(voxEngine.getCurrentGrDb(), std::memory_order_relaxed);
                break;
        }

        if (bypass && activeMode != Params::ModeVOX)
            for (int c = 0; c < numChannels; ++c)
                buffer.copyFrom(c, offset, dry[static_cast<size_t>(c)].data(), count);
    }
    if (bypass) currentGrDb.store(0.0f, std::memory_order_relaxed);

    // 3. 采集输出峰值与 RMS 电平（供电平表与波形绘制使用）
    float outPeakL = 0.0f, outPeakR = 0.0f;
    float sumL = 0.0f, sumR = 0.0f;
    const auto* outL = buffer.getReadPointer(0);
    const auto* outR = (numChannels > 1) ? buffer.getReadPointer(1) : outL;

    for (int n = 0; n < numSamples; ++n)
    {
        const float sL = outL[n];
        const float sR = outR[n];
        outPeakL = std::max(outPeakL, std::fabs(sL));
        outPeakR = std::max(outPeakR, std::fabs(sR));
        sumL += sL * sL;
        sumR += sR * sR;
    }

    const float invN = 1.0f / static_cast<float>(numSamples);
    outputPeakL.store(juce::Decibels::gainToDecibels(outPeakL), std::memory_order_relaxed);
    outputPeakR.store(juce::Decibels::gainToDecibels(outPeakR), std::memory_order_relaxed);
    outputRmsL.store(juce::Decibels::gainToDecibels(std::sqrt(sumL * invN)), std::memory_order_relaxed);
    outputRmsR.store(juce::Decibels::gainToDecibels(std::sqrt(sumR * invN)), std::memory_order_relaxed);
    auto accumulate = [](std::atomic<float>& destination, float value)
    {
        auto previous = destination.load(std::memory_order_relaxed);
        while (previous < value && !destination.compare_exchange_weak(previous, value, std::memory_order_relaxed)) {}
    };
    meterBlockDurationMs.store(static_cast<float>(1000.0 * numSamples / meterSampleRate), std::memory_order_relaxed);
    accumulate(meterInput, std::max(inputPeakL.load(), inputPeakR.load()));
    accumulate(meterOutput, std::max(outputPeakL.load(), outputPeakR.load()));
    accumulate(meterGr, currentGrDb.load());

}
