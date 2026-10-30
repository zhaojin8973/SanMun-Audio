// ==============================================================================
// EqBand.cpp - 单个 EQ 频段实现
// 直接管理左右声道 IIR::Filter，支持 Cut 滤波器多级级联
// 频响计算使用 JUCE 内置的 getMagnitudeForFrequency()
// ==============================================================================

#include "EqBand.h"
#include "TptSvf.h"
#include "../params/Parameters.h"

// ==============================================================================
// 生命周期
// ==============================================================================

void EqBand::prepare(const juce::dsp::ProcessSpec& spec)
{
    svfL.setSampleRate(spec.sampleRate);
    svfR.setSampleRate(spec.sampleRate);

    for (auto& es : extraSvfL) es.setSampleRate(spec.sampleRate);
    for (auto& es : extraSvfR) es.setSampleRate(spec.sampleRate);
    activeExtraCount = 0;
    dynamicProcessor.prepare(spec.sampleRate * 2.0);

    reset();
    smoothingParamsReady = false;
    // 首次准备时用默认参数设置主级滤波器
    recalculateCoefficients(spec.sampleRate);
}

void EqBand::reset()
{
    svfL.reset();
    svfR.reset();
    for (auto& es : extraSvfL) es.reset();
    for (auto& es : extraSvfR) es.reset();
    dynamicProcessor.reset();
}

// ==============================================================================
// 参数更新
// ==============================================================================

bool EqBand::update(int newType, bool newEnabled,
                    float newFrequency, float newGain, float newQ,
                    bool newSolo, int newSlope, int newPlacement,
                    bool newPresent, bool newDynamic, float newDynRange,
                    float newDynThreshold, bool newDynAuto, bool newDynUpward)
{
    bool changed = false;

    constexpr float epsilon = 0.001f;

    const bool currPresent = present.load(std::memory_order_relaxed);
    if (newPresent != currPresent)                 { present.store(newPresent, std::memory_order_relaxed);        changed = true; }
    if (newType != previousType)                   { type.store(newType, std::memory_order_relaxed);              changed = true; }
    if (newEnabled != previousEnabled)              { enabled.store(newEnabled, std::memory_order_relaxed);        changed = true; }
    if (newSolo != previousSolo)                    { solo.store(newSolo, std::memory_order_relaxed);              changed = true; }
    if (newSlope != previousSlope)                  { slope.store(newSlope, std::memory_order_relaxed);            changed = true; }
    if (newPlacement != previousPlacement)          { placement.store(newPlacement, std::memory_order_relaxed);    changed = true; }
    if (std::abs(newFrequency - previousFrequency) > epsilon) { frequency.store(newFrequency, std::memory_order_relaxed); changed = true; }
    if (std::abs(newGain - previousGain) > epsilon)         { gain.store(newGain, std::memory_order_relaxed);              changed = true; }
    if (std::abs(newQ - previousQ) > epsilon)               { q.store(newQ, std::memory_order_relaxed);                    changed = true; }
    if (newDynamic != dynamicEnabled.load(std::memory_order_relaxed)) { dynamicEnabled.store(newDynamic, std::memory_order_relaxed); changed = true; }
    if (std::abs(newDynRange - dynamicRange.load(std::memory_order_relaxed)) > epsilon) { dynamicRange.store(newDynRange, std::memory_order_relaxed); changed = true; }
    if (std::abs(newDynThreshold - dynamicThreshold.load(std::memory_order_relaxed)) > epsilon) { dynamicThreshold.store(newDynThreshold, std::memory_order_relaxed); changed = true; }
    if (dynamicUpward.exchange(newDynUpward) != newDynUpward) changed = true;
    dynamicProcessor.configure(newType, newFrequency, newQ, newDynamic && newEnabled && newPresent, newDynRange, newDynThreshold, newDynAuto, newDynUpward);

    return changed;
}

// ==============================================================================
// 系数计算
// ==============================================================================

static TptSvf::Type toSvfType(int type)
{
    switch (type)
    {
        case 0: return TptSvf::Type::Bell;
        case 1: return TptSvf::Type::LowShelf;
        case 2: return TptSvf::Type::HighShelf;
        case 3: return TptSvf::Type::HighPass;   // LowCut
        case 4: return TptSvf::Type::LowPass;    // HighCut
        default: return TptSvf::Type::Bell;
    }
}

EqBand::BandSvfParams EqBand::computeSvfParams(
    int type, float frequency, float gain, float q, int slope, double /*sampleRate*/)
{
    BandSvfParams result;

    const bool isCut = (type == Params::LowCut || type == Params::HighCut);
    const int numStages = isCut ? (slope + 1) : 1;

    result.mainType    = toSvfType(type);
    result.mainCutoff  = frequency;
    result.mainGainDb  = gain;

    if (isCut)
    {
        // 顶级母带标准 Butterworth 多阶级联极点 Q 理论值：
        // 阶数 2 (12dB/oct, 1 stage):  Q = 0.707107
        // 阶数 4 (24dB/oct, 2 stages): Q = { 1.306563, 0.541196 }
        // 阶数 6 (36dB/oct, 3 stages): Q = { 1.931852, 0.707107, 0.517638 }
        // 阶数 8 (48dB/oct, 4 stages): Q = { 2.562915, 0.899976, 0.601345, 0.509796 }
        //
        // 当用户 Q = 1.0 时，所有斜率均为 100% 完美平直的 Butterworth 响应（通带 0.0dB 绝不拱起、绝不塌陷）；
        // 当用户调节 Q > 1.0 时，主级共振峰按比例放大呈现可控共振；
        // 当用户调节 Q < 1.0 时，平滑柔和变软过渡。
        static constexpr float bwMainQ[4] = {
            0.70710678f, // 12 dB/oct
            1.306563f,   // 24 dB/oct
            1.931852f,   // 36 dB/oct
            2.562915f    // 48 dB/oct
        };

        static constexpr float bwExtraQ[3][3] = {
            { 0.541196f, 0.0f,      0.0f },       // 24 dB/oct (第 2 级)
            { 0.707107f, 0.517638f, 0.0f },       // 36 dB/oct (第 2, 3 级)
            { 0.899976f, 0.601345f, 0.509796f }  // 48 dB/oct (第 2, 3, 4 级)
        };

        const int slopeIdx = juce::jlimit(0, 3, slope);
        result.mainQ = bwMainQ[slopeIdx] * q;
        result.extraCount = numStages - 1;

        for (int i = 0; i < result.extraCount; ++i)
        {
            result.extraType[static_cast<size_t>(i)] =
                (type == Params::LowCut) ? TptSvf::Type::HighPass : TptSvf::Type::LowPass;
            result.extraQ[static_cast<size_t>(i)] = bwExtraQ[slopeIdx - 1][i];
        }
    }
    else if (type == Params::LowShelf || type == Params::HighShelf)
    {
        // 搁架式滤波器标准 Butterworth 平坦过渡：Q_ref = 0.70710678
        // 当旋钮 Q = 1.0 时，呈现 100% 单调平滑过渡，无任何中频下陷或共振过冲；
        // 当用户调大 Q 时，呈现经典英式模拟台/Pultec 风格的谐振过渡。
        result.mainQ = 0.70710678f * q;
        result.extraCount = 0;
    }
    else
    {
        result.mainQ = q;
        result.extraCount = 0;
    }

    return result;
}

void EqBand::recalculateCoefficients(double sampleRate)
{
    // 一次性读取原子参数快照，避免函数内多处 load 造成中途状态不一致
    const int    typeV      = type.load(std::memory_order_relaxed);
    const bool   enabledV   = enabled.load(std::memory_order_relaxed);
    const bool   soloV      = solo.load(std::memory_order_relaxed);
    const int    slopeV     = slope.load(std::memory_order_relaxed);
    const int    placementV = placement.load(std::memory_order_relaxed);
    const float  frequencyV = frequency.load(std::memory_order_relaxed);
    const float  gainV      = gain.load(std::memory_order_relaxed);
    const float  qV         = q.load(std::memory_order_relaxed);

    const bool isCut = (typeV == Params::LowCut || typeV == Params::HighCut);
    numStages = isCut ? (slopeV + 1) : 1;

    BandSvfParams params = computeSvfParams(
        typeV, frequencyV, gainV, qV, slopeV, sampleRate);

    svfL.setSampleRate(sampleRate);
    svfR.setSampleRate(sampleRate);

    // 主级：首次直接到位；否则交给 advanceSmoothing 逐块平滑。
    // 在修改 ready 标志前保存 firstInit，保证非默认预设的平滑起点正确。
    const bool firstInit = !smoothingParamsReady;
    if (firstInit)
    {
        svfL.setType(params.mainType);
        svfL.setCutoff(params.mainCutoff);
        svfL.setQ(params.mainQ);
        svfL.setGainDb(params.mainGainDb);
        svfR.setType(params.mainType);
        svfR.setCutoff(params.mainCutoff);
        svfR.setQ(params.mainQ);
        svfR.setGainDb(params.mainGainDb);
        smoothingActive = false;
    }
    else
    {
        smoothingActive = true;
    }
    smoothingParamsReady = true;

    // 额外级（Butterworth，不平滑，直接设置，定长预分配无堆分配）
    activeExtraCount = juce::jlimit(0, MAX_STAGES - 1, params.extraCount);
    for (int i = 0; i < activeExtraCount; ++i)
    {
        auto& esL = extraSvfL[static_cast<size_t>(i)];
        auto& esR = extraSvfR[static_cast<size_t>(i)];
        esL.setSampleRate(sampleRate);
        esL.setType(params.extraType[static_cast<size_t>(i)]);
        esL.setCutoff(params.mainCutoff);
        esL.setQ(params.extraQ[static_cast<size_t>(i)]);
        esL.setGainDb(0.0);
        esR.setSampleRate(sampleRate);
        esR.setType(params.extraType[static_cast<size_t>(i)]);
        esR.setCutoff(params.mainCutoff);
        esR.setQ(params.extraQ[static_cast<size_t>(i)]);
        esR.setGainDb(0.0);
    }

    // ---- 参数域平滑：更新目标参数；首次直接到位，之后交给 advanceSmoothing ----
    targetFreq = frequencyV;
    targetGain = gainV;
    targetQ    = qV;

    if (firstInit)
    {
        smoothedFreq = targetFreq;
        smoothedGain = targetGain;
        smoothedQ    = targetQ;
    }

    // 记录当前参数
    previousType     = typeV;
    previousEnabled  = enabledV;
    previousSolo     = soloV;
    previousSlope    = slopeV;
    previousPlacement = placementV;
    previousFrequency = frequencyV;
    previousGain     = gainV;
    previousQ        = qV;
}

// ==============================================================================
// 系数平滑 — 每块朝目标逼近，消除参数跳变爆音
// ==============================================================================

void EqBand::advanceSmoothing(double sampleRate, int elapsedSamples)
{
    if (!smoothingActive)
        return;

    // 15 ms 参数平滑；推进量来自内部采样时钟，与宿主块长无关。
    const float step = 1.f - std::exp(-static_cast<float>(elapsedSamples / (sampleRate * .015)));
    smoothedFreq += (targetFreq - smoothedFreq) * step;
    smoothedGain += (targetGain - smoothedGain) * step;
    smoothedQ    += (targetQ    - smoothedQ)    * step;

    // 用平滑后的连续参数重算 SVF 参数；type/slope 为离散值，直接取当前目标。
    const int typeV  = type.load(std::memory_order_relaxed);
    const int slopeV = slope.load(std::memory_order_relaxed);

    BandSvfParams smoothed = computeSvfParams(
        typeV, smoothedFreq, smoothedGain, smoothedQ, slopeV, sampleRate);

    svfL.setSampleRate(sampleRate);
    svfL.setType(smoothed.mainType);
    svfL.setCutoff(smoothed.mainCutoff);
    svfL.setQ(smoothed.mainQ);
    svfL.setGainDb(smoothed.mainGainDb);
    svfR.setSampleRate(sampleRate);
    svfR.setType(smoothed.mainType);
    svfR.setCutoff(smoothed.mainCutoff);
    svfR.setQ(smoothed.mainQ);
    svfR.setGainDb(smoothed.mainGainDb);
    for (int i = 0; i < activeExtraCount; ++i)
    {
        extraSvfL[static_cast<size_t>(i)].setCutoff(smoothed.mainCutoff);
        extraSvfR[static_cast<size_t>(i)].setCutoff(smoothed.mainCutoff);
    }

    // 收敛判断：平滑参数足够接近目标
    const bool converged =
        std::abs(targetFreq - smoothedFreq) < 0.01f
        && std::abs(targetGain - smoothedGain) < 0.001f
        && std::abs(targetQ    - smoothedQ)    < 0.001f;

    if (converged)
    {
        smoothedFreq = targetFreq;
        smoothedGain = targetGain;
        smoothedQ    = targetQ;
        smoothingActive = false;
    }
}

// ==============================================================================
// 音频处理 — 按单个声道处理，级联所有激活级
// ==============================================================================

void EqBand::processChannel(int channel, juce::dsp::AudioBlock<float>& block)
{
    jassert(channel == 0 || channel == 1);

    const int numSamples = static_cast<int>(block.getNumSamples());
    auto* data = block.getChannelPointer(0);

    auto& svf = (channel == 0) ? svfL : svfR;
    for (int i = 0; i < numSamples; ++i)
        data[i] = svf.processSample(data[i]);

    auto& extraSvfs = (channel == 0) ? extraSvfL : extraSvfR;
    for (int s = 0; s < activeExtraCount; ++s)
    {
        auto& es = extraSvfs[static_cast<size_t>(s)];
        for (int i = 0; i < numSamples; ++i)
            data[i] = es.processSample(data[i]);
    }
}

float EqBand::processSampleChannel(int channel, float sample)
{
    jassert(channel == 0 || channel == 1);

    auto& svf = (channel == 0) ? svfL : svfR;
    float s = svf.processSample(sample);

    auto& extraSvfs = (channel == 0) ? extraSvfL : extraSvfR;
    for (int i = 0; i < activeExtraCount; ++i)
        s = extraSvfs[static_cast<size_t>(i)].processSample(s);

    return s;
}

// ==============================================================================
// 频响计算 — 使用 JUCE 内置方法
// ==============================================================================

double EqBand::getMagnitudeAtFrequency(double freqHz, double sampleRate) const
{
    // UI 线程专用、无锁：读原子参数，参数不变时复用缓存系数。
    const int    typeV  = type.load(std::memory_order_relaxed);
    const float  freqV  = frequency.load(std::memory_order_relaxed);
    const float  gainV  = gain.load(std::memory_order_relaxed);
    const float  qV     = q.load(std::memory_order_relaxed);
    const int    slopeV = slope.load(std::memory_order_relaxed);

    constexpr float  paramEps = 1e-6f;
    constexpr double rateEps  = 1e-9;
    const bool sameParams =
        cachedResponseType       == typeV
        && cachedResponseSlope   == slopeV
        && std::abs(cachedResponseFrequency - freqV) < paramEps
        && std::abs(cachedResponseGain    - gainV) < paramEps
        && std::abs(cachedResponseQ       - qV)    < paramEps
        && std::abs(cachedResponseSampleRate - sampleRate) < rateEps;

    if (!sameParams)
    {
        cachedResponseParams = computeSvfParams(
            typeV, freqV, gainV, qV, slopeV, sampleRate);
        cachedResponseType       = typeV;
        cachedResponseSlope      = slopeV;
        cachedResponseFrequency  = freqV;
        cachedResponseGain       = gainV;
        cachedResponseQ          = qV;
        cachedResponseSampleRate = sampleRate;
    }

    // 逐级 dB 求和：主级 + 额外级。
    double totalDb = TptSvf::magnitudeDb(
        cachedResponseParams.mainType, cachedResponseParams.mainCutoff,
        cachedResponseParams.mainQ, cachedResponseParams.mainGainDb, freqHz);

    for (int i = 0; i < cachedResponseParams.extraCount; ++i)
    {
        totalDb += TptSvf::magnitudeDb(
            cachedResponseParams.extraType[static_cast<size_t>(i)],
            cachedResponseParams.mainCutoff,
            cachedResponseParams.extraQ[static_cast<size_t>(i)], 0.0, freqHz);
    }

    return totalDb;
}

double EqBand::getMagnitudeAtFrequencyAudioThread(double freqHz, double sampleRate) const
{
    const BandSvfParams params = computeSvfParams(type.load(std::memory_order_relaxed),
                                                  targetFreq, targetGain, targetQ,
                                                  slope.load(std::memory_order_relaxed), sampleRate);
    double totalDb = TptSvf::magnitudeDb(params.mainType, params.mainCutoff,
                                         params.mainQ, params.mainGainDb, freqHz);
    for (int i = 0; i < params.extraCount; ++i)
        totalDb += TptSvf::magnitudeDb(params.extraType[static_cast<size_t>(i)],
                                      params.mainCutoff, params.extraQ[static_cast<size_t>(i)],
                                      0.0, freqHz);
    return totalDb;
}

double EqBand::getPreparedMagnitudeAtFrequency(double freqHz) const
{
    double totalDb = TptSvf::magnitudeDb(
        audioResponseParams.mainType, audioResponseParams.mainCutoff,
        audioResponseParams.mainQ, audioResponseParams.mainGainDb, freqHz);

    for (int i = 0; i < audioResponseParams.extraCount; ++i)
    {
        totalDb += TptSvf::magnitudeDb(
            audioResponseParams.extraType[static_cast<size_t>(i)],
            audioResponseParams.mainCutoff,
            audioResponseParams.extraQ[static_cast<size_t>(i)], 0.0, freqHz);
    }

    return totalDb;
}

void EqBand::prepareAudioResponse(double sampleRate)
{
    audioResponseParams = computeSvfParams(type.load(std::memory_order_relaxed),
                                           targetFreq, targetGain, targetQ,
                                           slope.load(std::memory_order_relaxed), sampleRate);
    const auto& p = audioResponseParams;
    audioPowerResponse[0].prepare(p.mainType, p.mainCutoff, p.mainQ, p.mainGainDb);
    for (int i = 0; i < p.extraCount; ++i)
        audioPowerResponse[static_cast<size_t>(i + 1)].prepare(p.extraType[static_cast<size_t>(i)], p.mainCutoff, p.extraQ[static_cast<size_t>(i)], 0);
}

double EqBand::getPreparedPowerAtFrequency(double frequencySquared) const
{
    double power = audioPowerResponse[0].power(frequencySquared);
    for (int i = 0; i < audioResponseParams.extraCount; ++i)
        power *= audioPowerResponse[static_cast<size_t>(i + 1)].power(frequencySquared);
    return power;
}

// ==============================================================================
// Cut 共振峰频率
// ==============================================================================

double EqBand::getCutPeakFrequency(double sampleRate) const
{
    const int   typeV      = type.load(std::memory_order_relaxed);
    const float frequencyV = frequency.load(std::memory_order_relaxed);

    if (typeV != Params::LowCut && typeV != Params::HighCut)
        return frequencyV;

    const double f0 = static_cast<double>(frequencyV);
    double lo = f0, hi = f0;
    if (typeV == Params::LowCut)
    {
        lo = f0;
        hi = juce::jmin(static_cast<double>(Params::Ranges::FREQ_MAX), f0 * 2.0);
    }
    else
    {
        lo = juce::jmax(static_cast<double>(Params::Ranges::FREQ_MIN), f0 * 0.5);
        hi = f0;
    }

    if (hi - lo < 1e-9)
        return f0;

    // 对数均匀粗搜：多级级联后峰频会略偏离截止频率（主级 Q 越大越接近 f0）
    constexpr int N = 40;
    double bestDb = -1e300;
    int bestIdx = -1;
    for (int i = 0; i <= N; ++i)
    {
        const double f = lo * std::pow(hi / lo, static_cast<double>(i) / N);
        const double db = getMagnitudeAtFrequency(f, sampleRate);
        if (db > bestDb)
        {
            bestDb = db;
            bestIdx = i;
        }
    }

    // 无内部峰（Q 很小）时响应在 [f0, 2f0] 单调上升（阻带→通带），
    // 粗搜最佳点会落在右端点——那是通带 0dB，不是峰。
    // Cut 控制点此时应锚定截止频率（肥波惯例），而不是 2×f0
    if (bestIdx == 0 || bestIdx == N)
        return f0;

    // 黄金分割细化，保证控制点像素级贴住峰顶
    double a = (bestIdx > 0)
        ? lo * std::pow(hi / lo, static_cast<double>(bestIdx - 1) / N)
        : lo;
    double b = (bestIdx < N)
        ? lo * std::pow(hi / lo, static_cast<double>(bestIdx + 1) / N)
        : hi;
    constexpr double phi = 0.61803398874989485;
    double c = b - phi * (b - a);
    double d = a + phi * (b - a);
    double fc = getMagnitudeAtFrequency(c, sampleRate);
    double fd = getMagnitudeAtFrequency(d, sampleRate);

    for (int it = 0; it < 24; ++it)
    {
        if (fc > fd)
        {
            b = d; d = c; fd = fc;
            c = b - phi * (b - a);
            fc = getMagnitudeAtFrequency(c, sampleRate);
        }
        else
        {
            a = c; c = d; fc = fd;
            d = a + phi * (b - a);
            fd = getMagnitudeAtFrequency(d, sampleRate);
        }
    }

    return (a + b) * 0.5;
}
