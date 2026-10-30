// ==============================================================================
// EqProcessor.cpp - 6 段 EQ 级联处理器实现（第三阶段：2x 过采样）
// ==============================================================================

#include "EqProcessor.h"

// ==============================================================================
// 构造函数
// ==============================================================================

EqProcessor::EqProcessor()
{
    // 设置各频段的默认滤波器类型
    for (int i = 0; i < Params::NUM_BANDS; ++i)
    {
        bands[static_cast<size_t>(i)].type.store(
            Params::Defaults::TYPES[i], std::memory_order_relaxed);
        bands[static_cast<size_t>(i)].frequency.store(
            Params::Defaults::FREQUENCIES[i], std::memory_order_relaxed);
    }
}

// ==============================================================================
// 生命周期
// ==============================================================================

void EqProcessor::prepare(const juce::dsp::ProcessSpec& spec)
{
    // ---- 记录采样率（Solo 带通滤波器需要） ----
    currentSampleRate = spec.sampleRate;
    autoGainDirty = true;

    // ---- 预分配零分配缓冲区（R06） ----
    const int numChannels = static_cast<int>(spec.numChannels);
    const int maxBlock = static_cast<int>(spec.maximumBlockSize);
    dryBuffer.setSize(numChannels, maxBlock);
    soloTempBuffer.setSize(numChannels, maxBlock);

    const int delayCapacity = juce::nextPowerOfTwo(PrecisionLinearEq::latencyFor(spec.sampleRate, 2) + maxBlock + 1024);
    linearPhaseDelayBuffer.setSize(numChannels, delayCapacity);
    linearPhaseDelayBuffer.clear();
    lpDelayWritePos = 0;

    // ---- 为各频段准备滤波处理 ----
    for (auto& band : bands)
    {
        band.prepare(spec);
        band.recalculateCoefficients(spec.sampleRate * OVERSAMPLING_FACTOR);
    }
    minimumControlFill = 0;

    // ---- 为 Solo 带通滤波器准备（持久实例，避免每块重建） ----
    juce::dsp::ProcessSpec monoSpect = spec;
    monoSpect.numChannels = 1;
    auto dummyCoeffs = new juce::dsp::IIR::Coefficients<float>(
        1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);  // 单位增益
    for (int i = 0; i < Params::NUM_BANDS; ++i)
    {
        soloBpL[static_cast<size_t>(i)].coefficients = dummyCoeffs;
        soloBpR[static_cast<size_t>(i)].coefficients = dummyCoeffs;
        soloBpL[static_cast<size_t>(i)].prepare(monoSpect);
        soloBpR[static_cast<size_t>(i)].prepare(monoSpect);
    }
    soloBpPrepared = true;

    // ---- 初始化线性相位引擎（标准档保留固定延迟；高档使用多采样率长核） ----
    linearPhase = std::make_unique<LinearPhaseEq>();
    linearPhase->prepare(spec);
    linearPhase->prepareIdentityKernel();
    for (int quality = 0; quality < 3; ++quality)
    {
        precisionLinear[static_cast<size_t>(quality)] = std::make_unique<PrecisionLinearEq>();
        precisionLinear[static_cast<size_t>(quality)]->prepare(spec.sampleRate, quality);
    }
    firDirty = true;
    linearPhaseRebuildCountdown = 0;
    linearControlFill = 0;

    spectrumAnalyser.prepare(spec.sampleRate);

    // ---- 初始化 2× 过采样（ZeroLatency 级联路径；声道数创建后不可变，
    //      插件总线固定 stereo ✓；宿主重入 prepareToPlay 时整体重建） ----
    oversampling = std::make_unique<juce::dsp::Oversampling<float>>(
        static_cast<size_t>(spec.numChannels),
        1,   // 2^1 = 2×
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
        true,   // maxQuality
        true);  // useIntegerLatency：Thiran 补齐小数延迟 → 可上报整数
    oversampling->initProcessing(static_cast<size_t>(spec.maximumBlockSize));
    oversamplingLatencySamples = static_cast<int>(
        std::llround(oversampling->getLatencyInSamples()));

    // 默认 ZeroLatency 模式：延迟 = 过采样整数化延迟（约 2-3 采样 ≈0.05ms）
    latencySamples = oversamplingLatencySamples;

    // ---- 全局旁通平滑器（20ms 斜坡消除爆音）与自动增益平滑器（80ms） ----
    bypassSmoother.reset(spec.sampleRate, 0.02);
    bypassSmoother.setCurrentAndTargetValue(globalBypass.load(std::memory_order_relaxed) ? 1.0f : 0.0f);

    autoGainSmoother.reset(spec.sampleRate, 0.08);
    autoGainSmoother.setCurrentAndTargetValue(1.0f);
    for (auto& reduction : displayedReduction) reduction.store(0);
    tubeMix.reset(spec.sampleRate * OVERSAMPLING_FACTOR, .03);
    discreteMix.reset(spec.sampleRate * OVERSAMPLING_FACTOR, .03);
    tubeMix.setCurrentAndTargetValue(0); discreteMix.setCurrentAndTargetValue(0);
    characterPreviousInput = {}; characterPreviousOutput = {};
    if (processingMode == ProcessingMode::LinearPhase)
    {
        latencySamples = getLinearLatency();
        for (auto& band : bands)
        {
            const bool automatic = band.dynamicProcessor.isAutomatic();
            band.dynamicProcessor.prepare(currentSampleRate);
            band.dynamicProcessor.configure(band.type.load(), band.frequency.load(), band.q.load(),
                band.dynamicEnabled.load() && band.present.load() && band.enabled.load(),
                band.dynamicRange.load(), band.dynamicThreshold.load(), automatic, band.dynamicUpward.load());
        }
    }
    for (auto& reduction : displayedReduction) reduction.store(0);
}

void EqProcessor::reset()
{
    minimumControlFill = 0;
    dryBuffer.clear();
    soloTempBuffer.clear();
    linearPhaseDelayBuffer.clear();
    lpDelayWritePos = 0;

    for (auto& band : bands)
        band.reset();

    for (int i = 0; i < Params::NUM_BANDS; ++i)
    {
        soloBpL[static_cast<size_t>(i)].reset();
        soloBpR[static_cast<size_t>(i)].reset();
    }

    for (auto& engine : precisionLinear) if (engine) engine->reset(true);
    spectrumAnalyser.reset();
    firDirty = true;

    linearPhaseRebuildCountdown = 0;
    linearControlFill = 0;

    bypassSmoother.setCurrentAndTargetValue(globalBypass.load(std::memory_order_relaxed) ? 1.0f : 0.0f);
    autoGainSmoother.setCurrentAndTargetValue(1.0f);

}

// ==============================================================================
// 参数更新
// ==============================================================================

void EqProcessor::updateBand(int index, int type, bool enabled,
                             float frequency, float gain, float q,
                             bool solo, int slope, int placement, double sampleRate,
                             bool present, bool dynamic, float dynamicRange, float dynamicThreshold, bool dynamicAuto, bool dynamicUpward)
{
    jassert(index >= 0 && index < Params::NUM_BANDS);

    auto& band = bands[static_cast<size_t>(index)];

    // 检测参数变化
    bool changed = band.update(type, enabled, frequency, gain, q, solo, slope, placement, present, dynamic, dynamicRange, dynamicThreshold, dynamicAuto, dynamicUpward);

    // 仅在参数确实变化时重新计算系数（避免不必要的 CPU 开销）
    if (changed)
    {
        // 系数按 2×sr 设计（过采样级联在该采样率下运行）；
        // FIR 幅度采样与 UI 曲线评估也统一用 2×sr —— 三者口径一致
        band.recalculateCoefficients(sampleRate * OVERSAMPLING_FACTOR);
        autoGainDirty = true;
        firDirty = true;  // 线性相位 FIR 需按新幅度响应重建
    }
}

// ==============================================================================
// 处理模式切换
// ==============================================================================

void EqProcessor::setProcessingMode(int mode)
{
    ProcessingMode newMode = (mode == 1) ? ProcessingMode::LinearPhase
                                         : ProcessingMode::ZeroLatency;

    if (newMode == processingMode)
        return;  // 无变化，避免每块重置状态

    processingMode = newMode;
    minimumControlFill = 0;
    linearPhaseDelayBuffer.clear();
    lpDelayWritePos = 0;

    const double dynamicRate = currentSampleRate * (newMode == ProcessingMode::LinearPhase
                                                  ? 1.0 : OVERSAMPLING_FACTOR);
    for (auto& band : bands)
    {
        band.dynamicProcessor.prepare(dynamicRate);
        band.dynamicProcessor.configure(band.type.load(), band.frequency.load(), band.q.load(),
            band.dynamicEnabled.load() && band.present.load() && band.enabled.load(),
            band.dynamicRange.load(), band.dynamicThreshold.load(), band.dynamicProcessor.isAutomatic(), band.dynamicUpward.load());
    }

    if (newMode == ProcessingMode::LinearPhase)
    {
        // 清空 OLA 状态并标记重建 FIR，避免旧状态/旧核污染
        if (linearPhase) linearPhase->reset(true);
        for (auto& engine : precisionLinear) if (engine) engine->reset(true);
        firDirty = true;
        linearPhaseRebuildCountdown = 0;
        linearControlFill = 0;
        latencySamples = getLinearLatency();
    }
    else
    {
        // 重置各频段最小相位 IIR 状态，避免滤波状态断裂
        for (auto& band : bands)
            band.reset();
        // 过采样整数化延迟（约 2-3 采样 ≈ 0.05ms，宿主自动补偿）
        latencySamples = oversamplingLatencySamples;
        // 清空半带滤波历史状态（切换模式前的残留防瞬态）
        if (oversampling)
            oversampling->reset();
    }
}

// ==============================================================================
// 线性相位 FIR 设计的静态幅度采样回调
// ==============================================================================

float EqProcessor::sampleMagnitude(float freqHz, double sr, int channel, void* ctx)
{
    auto* self = static_cast<EqProcessor*>(ctx);
    return self->getMagnitudeAtFrequencyAudioThread(freqHz, sr, channel);
}

void EqProcessor::sampleMatrix(float freqHz, double sr, float matrix[4], void* ctx)
{
    static_cast<EqProcessor*>(ctx)->getMatrixAtFrequencyAudioThread(freqHz, sr, matrix);
}

// ==============================================================================
// Solo 检测
// ==============================================================================

bool EqProcessor::isAnyBandSoloed() const
{
    const int audition = auditionBand.load();
    if (audition >= 0 && bands[static_cast<size_t>(audition)].present.load()) return true;
    for (const auto& band : bands)
    {
        if (band.present.load(std::memory_order_relaxed)
            && band.enabled.load(std::memory_order_relaxed)
            && band.solo.load(std::memory_order_relaxed))
            return true;
    }
    return false;
}

// ==============================================================================
// 正常模式处理（非 Solo）
// ==============================================================================

void EqProcessor::processMinimumPhase(juce::AudioBuffer<float>& buffer)
{
    juce::dsp::AudioBlock<float> input(buffer);
    auto block = oversampling ? oversampling->processSamplesUp(input) : input;
    const double internalRate = currentSampleRate * (oversampling ? OVERSAMPLING_FACTOR : 1);
    constexpr int quantum = 128;
    for (size_t offset = 0; offset < block.getNumSamples();)
    {
        if (minimumControlFill == 0)
            for (auto& band : bands) band.advanceSmoothing(internalRate, quantum);
        const size_t count = std::min(static_cast<size_t>(quantum - minimumControlFill), block.getNumSamples() - offset);
        auto slice = block.getSubBlock(offset, count);
        processNormal(slice); applyCharacter(slice);
        minimumControlFill = (minimumControlFill + static_cast<int>(count)) % quantum;
        offset += count;
    }
    if (oversampling) oversampling->processSamplesDown(input);
}

void EqProcessor::processNormal(juce::dsp::AudioBlock<float>& block)
{
    auto numChannels = block.getNumChannels();
    if (numChannels < 2)
    {
        for (auto& band : bands)
        {
            if (!band.present.load(std::memory_order_relaxed)
                || !band.enabled.load(std::memory_order_relaxed))
                continue;
            // processChannel 只负责静态滤波；单声道路径也必须经过动态检测器。
            auto* samples = block.getChannelPointer(0);
            const auto numSamples = block.getNumSamples();
            for (size_t s = 0; s < numSamples; ++s)
            {
                float unused = 0.0f;
                band.dynamicProcessor.process(samples[s], unused, 1);
                samples[s] = band.processSampleChannel(0, samples[s]);
            }
        }
        return;
    }

    auto* leftSamples = block.getChannelPointer(0);
    auto* rightSamples = block.getChannelPointer(1);
    const size_t numSamples = block.getNumSamples();

    for (auto& band : bands)
    {
        if (!band.present.load(std::memory_order_relaxed)
            || !band.enabled.load(std::memory_order_relaxed))
            continue;

        const int place = band.placement.load(std::memory_order_relaxed);

        if (place == Params::PlacementStereo)
        {
            for (size_t s = 0; s < numSamples; ++s)
            {
                band.dynamicProcessor.process(leftSamples[s], rightSamples[s]);
                leftSamples[s] = band.processSampleChannel(0, leftSamples[s]);
                rightSamples[s] = band.processSampleChannel(1, rightSamples[s]);
            }
        }
        else if (place == Params::PlacementLeft)
        {
            for (size_t s = 0; s < numSamples; ++s)
            {
                float unused = 0; band.dynamicProcessor.process(leftSamples[s], unused, 1);
                leftSamples[s] = band.processSampleChannel(0, leftSamples[s]);
            }
        }
        else if (place == Params::PlacementRight)
        {
            for (size_t s = 0; s < numSamples; ++s)
            {
                float unused = 0; band.dynamicProcessor.process(rightSamples[s], unused, 1);
                rightSamples[s] = band.processSampleChannel(1, rightSamples[s]);
            }
        }
        else if (place == Params::PlacementMid)
        {
            // M/S 编码: M = (L+R)*0.5, S = (L-R)*0.5
            // 过滤 M, S 保持不变, 解码: L = M + S, R = M - S
            for (size_t s = 0; s < numSamples; ++s)
            {
                const float l = leftSamples[s];
                const float r = rightSamples[s];
                float m = 0.5f * (l + r);
                const float side = 0.5f * (l - r);
                float unused = 0; band.dynamicProcessor.process(m, unused, 1);
                const float mFiltered = band.processSampleChannel(0, m);
                leftSamples[s] = mFiltered + side;
                rightSamples[s] = mFiltered - side;
            }
        }
        else if (place == Params::PlacementSide)
        {
            // 过滤 S, M 保持不变, 解码: L = M + S, R = M - S
            for (size_t s = 0; s < numSamples; ++s)
            {
                const float l = leftSamples[s];
                const float r = rightSamples[s];
                const float m = 0.5f * (l + r);
                float side = 0.5f * (l - r);
                float unused = 0; band.dynamicProcessor.process(side, unused, 1);
                const float sFiltered = band.processSampleChannel(1, side);
                leftSamples[s] = m + sFiltered;
                rightSamples[s] = m - sFiltered;
            }
        }
    }
}

// ==============================================================================
// Solo 模式处理——带通滤波隔离（预分配零分配，R06）
// ==============================================================================

void EqProcessor::processSolo(juce::AudioBuffer<float>& buffer, double sampleRate)
{
    if (!soloBpPrepared)
        return;

    auto numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    // 复用预分配的 dryBuffer（已在 process 开始时拷贝输入，零分配）
    // 将输出清零；临时监听优先于历史预设中的持续独听。
    const int audition = auditionBand.load();
    buffer.clear();

    for (int i = 0; i < Params::NUM_BANDS; ++i)
    {
        auto& band = bands[static_cast<size_t>(i)];

        if (!band.present.load(std::memory_order_relaxed)
            || (audition >= 0 ? i != audition : (!band.enabled.load(std::memory_order_relaxed) || !band.solo.load(std::memory_order_relaxed))))
            continue;

        const float soloQ = band.q.load(std::memory_order_relaxed);

        // 栈上计算系数并写入预分配对象，扫频时不在音频线程分配堆内存。
        const auto bpCoeffs = juce::dsp::IIR::ArrayCoefficients<float>::makeBandPass(
            sampleRate, juce::jmin(float(sampleRate * .49), band.frequency.load(std::memory_order_relaxed)), soloQ);
        *soloBpL[static_cast<size_t>(i)].coefficients = bpCoeffs;
        *soloBpR[static_cast<size_t>(i)].coefficients = bpCoeffs;

        // 复用预分配的 soloTempBuffer = 原始干声副本（零堆分配）
        for (int ch = 0; ch < numChannels; ++ch)
            soloTempBuffer.copyFrom(ch, 0, dryBuffer.getReadPointer(ch), numSamples);

        const int placement = band.placement.load(std::memory_order_relaxed);
        if (numChannels >= 2)
        {
            auto* left = soloTempBuffer.getWritePointer(0); auto* right = soloTempBuffer.getWritePointer(1);
            for (int s = 0; s < numSamples; ++s)
            {
                const float l = left[s], r = right[s];
                if (placement == Params::PlacementLeft) right[s] = 0;
                else if (placement == Params::PlacementRight) left[s] = 0;
                else if (placement == Params::PlacementMid) left[s] = right[s] = .5f * (l + r);
                else if (placement == Params::PlacementSide) { left[s] = .5f * (l - r); right[s] = -left[s]; }
            }
        }
        // 逐声道应用持久带通滤波器
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto& bpFilter = (ch == 0) ? soloBpL[static_cast<size_t>(i)]
                                        : soloBpR[static_cast<size_t>(i)];

            juce::dsp::AudioBlock<float> tempBlock(soloTempBuffer);
            auto channelBlock = tempBlock.getSingleChannelBlock(static_cast<size_t>(ch));
            juce::dsp::ProcessContextReplacing<float> context(channelBlock);
            bpFilter.process(context);
        }

        // 将带通过滤后的结果累加到输出
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* outData  = buffer.getWritePointer(ch);
            auto* tempData = soloTempBuffer.getReadPointer(ch);
            for (int s = 0; s < numSamples; ++s)
                outData[s] += tempData[s];
        }
    }
}

// ==============================================================================
// 自动增益响度补偿评估（Pink Noise 对数感知能量加权）
// ==============================================================================

float EqProcessor::calculateAutoGainDb() const
{
    static constexpr float testFreqs[16] = {
        35.0f, 65.0f, 125.0f, 250.0f, 500.0f, 750.0f,
        1000.0f, 1500.0f, 2000.0f, 3000.0f, 4000.0f, 6000.0f,
        8000.0f, 10000.0f, 13000.0f, 16000.0f
    };
    static constexpr float weights[16] = {
        0.5f, 0.8f, 1.0f, 1.2f, 1.4f, 1.5f,
        1.5f, 1.4f, 1.3f, 1.2f, 1.1f, 1.0f,
        0.8f, 0.6f, 0.4f, 0.3f
    };

    double weightedPower = 0, sumW = 0;
    for (int point = 0; point < 16; ++point)
    {
        double matrix[4] {1, 0, 0, 1};
        for (const auto& band : bands)
        {
            if (!band.present.load() || !band.enabled.load()) continue;
            const double a = std::pow(10., band.getMagnitudeAtFrequencyAudioThread(testFreqs[point], currentSampleRate * OVERSAMPLING_FACTOR) / 20.);
            double b[4] {a, 0, 0, a};
            switch (band.placement.load())
            {
                case Params::PlacementLeft: b[3] = 1; break;
                case Params::PlacementRight: b[0] = 1; break;
                case Params::PlacementMid:
                case Params::PlacementSide:
                    b[0] = b[3] = .5 * (a + 1);
                    b[1] = b[2] = .5 * (a - 1) * (band.placement.load() == Params::PlacementMid ? 1 : -1); break;
                default: break;
            }
            const double next[4] {b[0]*matrix[0]+b[1]*matrix[2], b[0]*matrix[1]+b[1]*matrix[3],
                                  b[2]*matrix[0]+b[3]*matrix[2], b[2]*matrix[1]+b[3]*matrix[3]};
            std::copy_n(next, 4, matrix);
        }
        // 两个非相关输入的平均静态能量；左右和中侧变换具有相同权重。
        double power = 0; for (const double value : matrix) power += value * value * .5;
        weightedPower += std::max(.063095734, power) * weights[point]; sumW += weights[point];
    }
    return juce::jlimit(-18.f, 18.f, static_cast<float>(-10 * std::log10(std::max(1e-12, weightedPower / sumW))));
}

// ==============================================================================
// 音频处理入口——双模式分发
// ==============================================================================

void EqProcessor::process(juce::AudioBuffer<float>& buffer)
{
    if (buffer.getNumSamples() <= 0 || buffer.getNumChannels() <= 0) return;
    // ---- 第一步：计算输入电平 ----
    computeInputLevels(buffer);

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    // ---- 第一步半：应用输入 L/R 增益（EQ 前，逐声道） ----
    if (numChannels >= 2)
    {
        buffer.applyGain(0, 0, numSamples, inputGainL);
        buffer.applyGain(1, 0, numSamples, inputGainR);
    }
    else
    {
        buffer.applyGain(inputGainL);
    }

    // 更新旁通目标值 (1.0 = 旁通, 0.0 = 正常处理)
    const bool isBypassed = globalBypass.load(std::memory_order_relaxed);
    bypassSmoother.setTargetValue(isBypassed ? 1.0f : 0.0f);

    // 零分配保存一份 Pre-EQ 干声副本（R06）
    for (int ch = 0; ch < numChannels; ++ch)
        dryBuffer.copyFrom(ch, 0, buffer.getReadPointer(ch), numSamples);

    // 线性相位延迟对齐（R02）：线性相位模式下，dryBuffer 经过 5120 采样延迟线，
    // 使得完全旁通与旁通过渡交叉淡化始终与湿声 5120 延迟完美同相，绝不产生梳状滤波或延迟跳跃
    if (processingMode == ProcessingMode::LinearPhase)
    {
        const int delayCap = linearPhaseDelayBuffer.getNumSamples();
        const int delayMask = delayCap - 1;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float* src = dryBuffer.getReadPointer(ch);
            float* delayData = linearPhaseDelayBuffer.getWritePointer(ch);
            float* dst = dryBuffer.getWritePointer(ch);
            int wPos = lpDelayWritePos;
            for (int s = 0; s < numSamples; ++s)
            {
                delayData[wPos] = src[s];
                const int rPos = (wPos - getLinearLatency() + delayCap) & delayMask;
                dst[s] = delayData[rPos];
                wPos = (wPos + 1) & delayMask;
            }
        }
        lpDelayWritePos = (lpDelayWritePos + numSamples) & delayMask;
    }

    // ---- 第二步：按模式分发处理 ----
    const bool fullyBypassed = (! bypassSmoother.isSmoothing() && bypassSmoother.getCurrentValue() >= 0.999f);

    if (fullyBypassed)
    {
        // 保持线性引擎输入历史推进，解除旁通时不会重播旧 FIFO。
        if (processingMode == ProcessingMode::LinearPhase) processLinear(buffer);
        else processMinimumPhase(buffer);
        // 完全旁通时直接输出延迟对齐的干声（R02：线性相位恒为 5120 采样延迟）
        for (int ch = 0; ch < numChannels; ++ch)
            buffer.copyFrom(ch, 0, dryBuffer.getReadPointer(ch), numSamples);
        bypassSmoother.skip(numSamples);
    }
    else
    {
        if (processingMode == ProcessingMode::LinearPhase)
        {
            processLinear(buffer);
            // 线性相位只改变均衡滤波器的相位/延迟；三种音色的非线性仍然保留，
            // 因此 Tube/Discrete 不会因为开启线性相位而失去谐波特质。
            juce::dsp::AudioBlock<float> characterBlock(buffer);
            applyCharacter(characterBlock);
            if (isAnyBandSoloed()) processSolo(buffer, currentSampleRate);
        }
        else
        {
            processMinimumPhase(buffer);
            if (isAnyBandSoloed()) processSolo(buffer, currentSampleRate);
        }

        // 旁通过渡混合：wet * (1 - b) + dry * b（R02：此时 dryBuffer 与 wet 均已对齐 5120 采样延迟）
        if (bypassSmoother.isSmoothing() || bypassSmoother.getCurrentValue() > 0.001f)
        {
            for (int s = 0; s < numSamples; ++s)
            {
                const float b = bypassSmoother.getNextValue();
                const float wetScale = 1.0f - b;
                for (int ch = 0; ch < numChannels; ++ch)
                {
                    buffer.setSample(ch, s, buffer.getSample(ch, s) * wetScale + dryBuffer.getSample(ch, s) * b);
                }
            }
        }
        else
        {
            bypassSmoother.skip(numSamples);
        }
    }

    // ---- 自动增益补偿（Auto Gain） ----
    if (autoGainEnabled.load(std::memory_order_relaxed) && ! isBypassed)
    {
        if (autoGainDirty) { cachedAutoGain = juce::Decibels::decibelsToGain(calculateAutoGainDb()); autoGainDirty = false; }
        autoGainSmoother.setTargetValue(cachedAutoGain);
    }
    else
    {
        autoGainSmoother.setTargetValue(1.0f);
    }
    autoGainSmoother.applyGain(buffer, buffer.getNumSamples());

    // ---- 第三步：应用输出 L/R 增益（第九轮，替代 输出增益+声像平衡） ----
    if (buffer.getNumChannels() >= 2)
    {
        buffer.applyGain(0, 0, buffer.getNumSamples(), outputGainL);
        buffer.applyGain(1, 0, buffer.getNumSamples(), outputGainR);
    }
    else
    {
        buffer.applyGain(outputGainL);
    }

    // ---- 第四步：计算输出电平 ----
    computeOutputLevels(buffer);
    // 界面只读稳定原子值，不跨线程解引用 prepare 时会重建的线性相位对象。
    const bool hideReduction = isBypassed || isAnyBandSoloed();
    for (int i = 0; i < Params::MAX_BANDS; ++i)
        displayedReduction[static_cast<size_t>(i)].store(
            hideReduction ? 0.f : processingMode == ProcessingMode::LinearPhase
                ? (linearQuality == 0 ? linearPhase->getAppliedReduction(i) : precisionLinear[static_cast<size_t>(linearQuality)]->getAppliedReduction(i))
                : bands[static_cast<size_t>(i)].getDynamicReduction(),
            std::memory_order_relaxed);
    meterSequence.fetch_add(1, std::memory_order_relaxed);

    // ---- 第五步：Post-EQ 频谱采集（反映 EQ 处理后的实际输出信号） ----
    accumulateSpectrum(buffer);
}

void EqProcessor::applyCharacter(juce::dsp::AudioBlock<float>& block)
{
    const int mode = characterMode.load(std::memory_order_relaxed);
    tubeMix.setTargetValue(mode == 1 ? 1.f : 0.f);
    discreteMix.setTargetValue(mode == 2 ? 1.f : 0.f);
    if (mode == 0 && !tubeMix.isSmoothing() && !discreteMix.isSmoothing()) return;
    const float dcPole = std::exp(-2.f * juce::MathConstants<float>::pi * 5.f / static_cast<float>(currentSampleRate * OVERSAMPLING_FACTOR));
    const float bias = .16f, biasValue = std::tanh(bias), tubeScale = .65f * (1.f - biasValue * biasValue);
    for (size_t i = 0; i < block.getNumSamples(); ++i)
    {
        const float tube = tubeMix.getNextValue(), discrete = discreteMix.getNextValue();
        for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
        {
            auto* data = block.getChannelPointer(ch); const float x = data[i];
            // 仅对非线性残差去直流，保留干声的低频与小信号单位增益。
            const std::array<float, 2> residual { (std::tanh(.65f * x + bias) - biasValue) / tubeScale - x, std::tanh(.22f * x) / .22f - x };
            std::array<float, 2> ac{};
            for (size_t k = 0; k < 2; ++k)
            {
                ac[k] = residual[k] - characterPreviousInput[k][ch] + dcPole * characterPreviousOutput[k][ch];
                characterPreviousInput[k][ch] = residual[k]; characterPreviousOutput[k][ch] = ac[k];
            }
            data[i] = x + tube * ac[0] + discrete * ac[1];
        }
    }
}

// ==============================================================================
// 输入电平计算（在音频线程中 DSP 处理前调用）
// ==============================================================================

void EqProcessor::computeInputLevels(const juce::AudioBuffer<float>& buffer)
{
    auto numSamples = buffer.getNumSamples();
    auto numChannels = buffer.getNumChannels();

    // 左声道（或单声道）
    {
        auto* data = buffer.getReadPointer(0);
        float sumSq = 0.0f;
        float peak = 0.0f;
        for (int s = 0; s < numSamples; ++s)
        {
            float absVal = std::abs(data[s]);
            if (absVal > peak) peak = absVal;
            sumSq += data[s] * data[s];
        }
        inputLeftPeakDb.store(juce::Decibels::gainToDecibels(peak, -100.0f));
        float rms = std::sqrt(sumSq / static_cast<float>(numSamples));
        inputLeftRmsDb.store(juce::Decibels::gainToDecibels(rms, -100.0f));
    }

    // 右声道
    if (numChannels > 1)
    {
        auto* data = buffer.getReadPointer(1);
        float sumSq = 0.0f;
        float peak = 0.0f;
        for (int s = 0; s < numSamples; ++s)
        {
            float absVal = std::abs(data[s]);
            if (absVal > peak) peak = absVal;
            sumSq += data[s] * data[s];
        }
        inputRightPeakDb.store(juce::Decibels::gainToDecibels(peak, -100.0f));
        float rms = std::sqrt(sumSq / static_cast<float>(numSamples));
        inputRightRmsDb.store(juce::Decibels::gainToDecibels(rms, -100.0f));
    }
}

// ==============================================================================
// 输出电平计算（在音频线程中调用）
// ==============================================================================

void EqProcessor::computeOutputLevels(const juce::AudioBuffer<float>& buffer)
{
    auto numSamples = buffer.getNumSamples();
    auto numChannels = buffer.getNumChannels();

    // 左声道（或单声道）
    {
        auto* data = buffer.getReadPointer(0);
        float sumSq = 0.0f;
        float peak = 0.0f;
        for (int s = 0; s < numSamples; ++s)
        {
            float absVal = std::abs(data[s]);
            if (absVal > peak) peak = absVal;
            sumSq += data[s] * data[s];
        }
        leftPeakDb.store(juce::Decibels::gainToDecibels(peak, -100.0f));
        float rms = std::sqrt(sumSq / static_cast<float>(numSamples));
        leftRmsDb.store(juce::Decibels::gainToDecibels(rms, -100.0f));
    }

    // 右声道
    if (numChannels > 1)
    {
        auto* data = buffer.getReadPointer(1);
        float sumSq = 0.0f;
        float peak = 0.0f;
        for (int s = 0; s < numSamples; ++s)
        {
            float absVal = std::abs(data[s]);
            if (absVal > peak) peak = absVal;
            sumSq += data[s] * data[s];
        }
        rightPeakDb.store(juce::Decibels::gainToDecibels(peak, -100.0f));
        float rms = std::sqrt(sumSq / static_cast<float>(numSamples));
        rightRmsDb.store(juce::Decibels::gainToDecibels(rms, -100.0f));
    }
    else
    {
        // 单声道实例两条显示使用同一测量，避免右表残留旧值。
        rightPeakDb.store(leftPeakDb.load());
        rightRmsDb.store(leftRmsDb.load());
    }
}

// ==============================================================================
// 频响曲线计算（UI 线程调用）
// ==============================================================================

float EqProcessor::getMagnitudeAtFrequency(float freqHz, double sampleRate) const
{
    // 防止采样率为 0 时除零
    if (sampleRate <= 0.0)
        return 0.0f;

    // 级联滤波器：各级 dB 响应直接相加。
    // 参数 sampleRate 为插件原生采样率；系数按 2×sr 设计（过采样），
    // 评估统一用 2×sr 口径（UI 调用方无需感知）
    const double evalSr = sampleRate * OVERSAMPLING_FACTOR;
    double totalDb = 0.0;

    for (const auto& band : bands)
    {
        if (!band.present.load(std::memory_order_relaxed)
            || !band.enabled.load(std::memory_order_relaxed))
            continue;

        // band.getMagnitudeAtFrequency 已返回 dB 值
        totalDb += band.getMagnitudeAtFrequency(freqHz, evalSr);
    }

    return static_cast<float>(totalDb);
}

float EqProcessor::getMagnitudeAtFrequencyAudioThread(float freqHz, double sampleRate, int channel) const
{
    // 音频线程专用：Linear Phase FIR 设计采样整链目标响应。
    // 走无锁的 band.getMagnitudeAtFrequencyAudioThread（读目标系数），
    // 避免 UI 那条 mutex 路径在实时线程里阻塞。
    if (sampleRate <= 0.0)
        return 0.0f;

    const double evalSr = sampleRate * OVERSAMPLING_FACTOR;
    double totalDb = 0.0;

    // 检查是否有活跃的 Mid/Side 频段
    bool hasMidSideBands = false;
    bool hasLeftRightBands = false;
    for (const auto& band : bands)
    {
        if (band.present.load(std::memory_order_relaxed) && band.enabled.load(std::memory_order_relaxed))
        {
            const int p = band.placement.load(std::memory_order_relaxed);
            if (p == Params::PlacementMid || p == Params::PlacementSide) hasMidSideBands = true;
            if (p == Params::PlacementLeft || p == Params::PlacementRight) hasLeftRightBands = true;
        }
    }
    const bool runMsMode = (hasMidSideBands && !hasLeftRightBands);

    for (const auto& band : bands)
    {
        if (!band.present.load(std::memory_order_relaxed)
            || !band.enabled.load(std::memory_order_relaxed))
            continue;

        const int place = band.placement.load(std::memory_order_relaxed);
        bool include = false;

        if (runMsMode)
        {
            // channel 0 = Mid, channel 1 = Side
            if (place == Params::PlacementStereo)
                include = true;
            else if (place == Params::PlacementMid && channel == 0)
                include = true;
            else if (place == Params::PlacementSide && channel == 1)
                include = true;
        }
        else
        {
            // channel 0 = Left, channel 1 = Right
            if (place == Params::PlacementStereo)
                include = true;
            else if (place == Params::PlacementLeft && channel == 0)
                include = true;
            else if (place == Params::PlacementRight && channel == 1)
                include = true;
            else if (place == Params::PlacementMid) // Mid affects both L and R
                include = true;
        }

        if (include)
            totalDb += band.getMagnitudeAtFrequencyAudioThread(freqHz, evalSr);
    }

    return static_cast<float>(totalDb);
}

void EqProcessor::processLinear(juce::AudioBuffer<float>& buffer)
{
    if (linearQuality == 0)
    {
        processLegacyLinear(buffer);
        return;
    }
    auto* linear = precisionLinear[static_cast<size_t>(linearQuality)].get();
    // 固定 64 采样控制时钟；宿主残块只推进音频，不提前发布控制快照。
    const int channels = std::min(2, buffer.getNumChannels());
    if (channels == 0) return;
    for (int offset = 0; offset < buffer.getNumSamples();)
    {
        const int count = std::min(64 - linearControlFill, buffer.getNumSamples() - offset);
        float* pointers[2] {buffer.getWritePointer(0, offset),
                            channels > 1 ? buffer.getWritePointer(1, offset) : nullptr};
        juce::AudioBuffer<float> slice(pointers, channels, count);
        // 在写入线性相位输出前读取当前 slice，检测支路使用真正的 Pre-EQ 音频。
        if (updateLinearDynamicControls(slice)) firDirty = true;
        linearControlFill += count;
        const bool tick = linearControlFill == 64;
        if (tick)
        {
            linearControlFill = 0;
            linearPhaseRebuildCountdown = std::max(0, linearPhaseRebuildCountdown - 64);
            if (firDirty && !linear->isRebuildPending() && linearPhaseRebuildCountdown == 0)
            {
                for (int i = 0; i < Params::NUM_BANDS; ++i)
                {
                    auto& band = bands[static_cast<size_t>(i)];
                    band.prepareAudioResponse(currentSampleRate * OVERSAMPLING_FACTOR);
                    auto& snapshot = linearBandSnapshot[static_cast<size_t>(i)];
                    snapshot.active = band.present.load() && band.enabled.load();
                    snapshot.type = band.type.load();
                    snapshot.placement = buffer.getNumChannels() == 1 ? Params::PlacementStereo : band.placement.load();
                    snapshot.frequency = band.frequency.load();
                    snapshot.q = band.q.load();
                    snapshot.dynamicGainDb = snapshot.active && snapshot.type < 3 && band.dynamicEnabled.load()
                                          ? band.dynamicProcessor.getDynamicGainDb() : 0.0f;
                    const auto dynamicType = snapshot.type == Params::LowShelf ? TptSvf::Type::LowShelf
                                           : snapshot.type == Params::HighShelf ? TptSvf::Type::HighShelf : TptSvf::Type::Bell;
                    snapshot.dynamicResponse.prepare(dynamicType, snapshot.frequency,
                        snapshot.q * (snapshot.type == Params::Peak ? 1.f : .70710678f), snapshot.dynamicGainDb);
                    linear->setPendingDynamicGain(i, snapshot.dynamicGainDb);
                }
                linear->beginRebuild(&EqProcessor::sampleMatrix, this);
                firDirty = false;
                linearPhaseRebuildCountdown = std::max(1, static_cast<int>(currentSampleRate * REBUILD_INTERVAL_SECONDS));
            }
        }
        linear->processBlock(slice, tick);
        offset += count;
    }
}

void EqProcessor::processLegacyLinear(juce::AudioBuffer<float>& buffer)
{
    if (linearPhase == nullptr) return;
    const int channels = std::min(2, buffer.getNumChannels());
    if (channels == 0) return;
    for (int offset = 0; offset < buffer.getNumSamples();)
    {
        const int count = std::min(64 - linearControlFill, buffer.getNumSamples() - offset);
        float* pointers[2] {buffer.getWritePointer(0, offset), channels > 1 ? buffer.getWritePointer(1, offset) : nullptr};
        juce::AudioBuffer<float> slice(pointers, channels, count);
        // 在写入线性相位输出前读取当前 slice，检测支路使用真正的 Pre-EQ 音频。
        if (updateLinearDynamicControls(slice)) firDirty = true;
        linearControlFill += count;
        const bool tick = linearControlFill == 64;
        if (tick)
        {
            linearControlFill = 0;
            linearPhaseRebuildCountdown = std::max(0, linearPhaseRebuildCountdown - 64);
            if (firDirty && !linearPhase->isRebuildPending() && linearPhaseRebuildCountdown == 0)
            {
                for (int i = 0; i < Params::NUM_BANDS; ++i)
                {
                    auto& band = bands[static_cast<size_t>(i)];
                    band.prepareAudioResponse(currentSampleRate * OVERSAMPLING_FACTOR);
                    auto& snapshot = linearBandSnapshot[static_cast<size_t>(i)];
                    snapshot.active = band.present.load() && band.enabled.load();
                    snapshot.type = band.type.load(); snapshot.placement = channels == 1 ? Params::PlacementStereo : band.placement.load();
                    snapshot.frequency = band.frequency.load(); snapshot.q = band.q.load();
                    snapshot.dynamicGainDb = snapshot.active && snapshot.type < 3 && band.dynamicEnabled.load() ? band.dynamicProcessor.getDynamicGainDb() : 0.0f;
                    const auto dynamicType = snapshot.type == Params::LowShelf ? TptSvf::Type::LowShelf : snapshot.type == Params::HighShelf ? TptSvf::Type::HighShelf : TptSvf::Type::Bell;
                    snapshot.dynamicResponse.prepare(dynamicType, snapshot.frequency, snapshot.q * (snapshot.type == Params::Peak ? 1.f : .70710678f), snapshot.dynamicGainDb);
                    linearPhase->setPendingDynamicGain(i, snapshot.dynamicGainDb);
                }
                linearPhase->beginIncrementalRebuild(&EqProcessor::sampleMatrix, this, currentSampleRate);
                firDirty = false;
                linearPhaseRebuildCountdown = std::max(1, static_cast<int>(currentSampleRate * REBUILD_INTERVAL_SECONDS));
            }
        }
        linearPhase->processBlock(slice, tick ? 64 : 0);
        offset += count;
    }
}

bool EqProcessor::updateLinearDynamicControls(const juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    if (numSamples <= 0 || buffer.getNumChannels() == 0)
        return false;

    const float* left = buffer.getReadPointer(0);
    const float* right = buffer.getNumChannels() > 1 ? buffer.getReadPointer(1) : left;
    bool changed = false;

    for (int bandIndex = 0; bandIndex < Params::NUM_BANDS; ++bandIndex)
    {
        auto& band = bands[static_cast<size_t>(bandIndex)];
        if (!band.present.load(std::memory_order_relaxed)
            || !band.enabled.load(std::memory_order_relaxed)
            || band.type.load(std::memory_order_relaxed) >= 3)
            continue;

        const bool dynamicEnabled = band.dynamicEnabled.load(std::memory_order_relaxed);
        if (!dynamicEnabled && std::abs(band.dynamicProcessor.getDynamicGainDb()) < 0.0001f)
            continue;

        const int placement = buffer.getNumChannels() == 1 ? Params::PlacementLeft : band.placement.load(std::memory_order_relaxed);
        for (int sample = 0; sample < numSamples; ++sample)
        {
            switch (placement)
            {
                case Params::PlacementLeft:
                    band.dynamicProcessor.processControl(left[sample], 0.0f, 1);
                    break;
                case Params::PlacementRight:
                    band.dynamicProcessor.processControl(right[sample], 0.0f, 1);
                    break;
                case Params::PlacementMid:
                    band.dynamicProcessor.processControl(0.5f * (left[sample] + right[sample]), 0.0f, 1);
                    break;
                case Params::PlacementSide:
                    band.dynamicProcessor.processControl(0.5f * (left[sample] - right[sample]), 0.0f, 1);
                    break;
                default:
                    band.dynamicProcessor.processControl(left[sample], right[sample], 2);
                    break;
            }
        }

        const float after = band.dynamicProcessor.getDynamicGainDb();
        changed = changed || std::abs(after - linearBandSnapshot[static_cast<size_t>(bandIndex)].dynamicGainDb) > 0.001f;
    }

    return changed;
}

void EqProcessor::getMatrixAtFrequencyAudioThread(float freqHz, double sampleRate, float matrix[4]) const
{
    matrix[0] = 1.0f;
    matrix[1] = 0.0f;
    matrix[2] = 0.0f;
    matrix[3] = 1.0f;
    if (sampleRate <= 0.0)
        return;

    for (int i = 0; i < Params::NUM_BANDS; ++i)
    {
        const auto& snapshot = linearBandSnapshot[static_cast<size_t>(i)];
        if (!snapshot.active)
            continue;

        const auto& band = bands[static_cast<size_t>(i)];
        const double squared = static_cast<double>(freqHz) * freqHz;
        const float gain = static_cast<float>(std::sqrt(band.getPreparedPowerAtFrequency(squared)
                                                      * snapshot.dynamicResponse.power(squared)));
        float bandMatrix[4] { gain, 0.0f, 0.0f, gain };
        switch (snapshot.placement)
        {
            case Params::PlacementLeft:
                bandMatrix[3] = 1.0f;
                break;
            case Params::PlacementRight:
                bandMatrix[0] = 1.0f;
                break;
            case Params::PlacementMid:
            {
                const float half = 0.5f * (gain - 1.0f);
                bandMatrix[0] = 1.0f + half;
                bandMatrix[1] = half;
                bandMatrix[2] = half;
                bandMatrix[3] = 1.0f + half;
                break;
            }
            case Params::PlacementSide:
            {
                const float half = 0.5f * (gain - 1.0f);
                bandMatrix[0] = 1.0f + half;
                bandMatrix[1] = -half;
                bandMatrix[2] = -half;
                bandMatrix[3] = 1.0f + half;
                break;
            }
            default:
                break;
        }

        const float next[4] {
            bandMatrix[0] * matrix[0] + bandMatrix[1] * matrix[2],
            bandMatrix[0] * matrix[1] + bandMatrix[1] * matrix[3],
            bandMatrix[2] * matrix[0] + bandMatrix[3] * matrix[2],
            bandMatrix[2] * matrix[1] + bandMatrix[3] * matrix[3]
        };
        std::copy(std::begin(next), std::end(next), matrix);
    }
}

void EqProcessor::accumulateSpectrum(const juce::AudioBuffer<float>& buffer)
{
    spectrumAnalyser.push(dryBuffer, buffer);
}

void EqProcessor::setLinearQuality(int quality)
{
    const int next = juce::jlimit(0, 2, quality);
    if (next == linearQuality) return;
    linearQuality = next;
    if (linearPhase) linearPhase->reset(true);
    if (auto* engine = precisionLinear[static_cast<size_t>(next)].get()) engine->reset(true);
    linearPhaseDelayBuffer.clear(); lpDelayWritePos = 0;
    linearControlFill = 0; linearPhaseRebuildCountdown = 0; firDirty = true;
    if (processingMode == ProcessingMode::LinearPhase && precisionLinear[static_cast<size_t>(next)]) latencySamples = getLinearLatency();
}
