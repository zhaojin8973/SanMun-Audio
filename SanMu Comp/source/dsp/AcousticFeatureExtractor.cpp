#include "AcousticFeatureExtractor.h"
#include "engines/VoxEngine.h"
#include "engines/VcaEngine.h"
#include "engines/FetEngine.h"
#include "engines/OptoEngine.h"
#include "../ai/LlmClient.h"
#include <juce_events/juce_events.h>
#include <cmath>
#include <algorithm>
#include <numeric>

namespace ai
{

static inline float ampToDb(float amp)
{
    return (amp > 1e-6f) ? (20.0f * std::log10(amp)) : -120.0f;
}

juce::String AcousticFeatures::toJsonString(const juce::String& userPrompt) const
{
    juce::DynamicObject::Ptr root = new juce::DynamicObject();

    // 1. 工程节奏与宿主信息
    juce::DynamicObject::Ptr proj = new juce::DynamicObject();
    proj->setProperty("daw_bpm", bpm);
    proj->setProperty("time_signature", juce::String(timeSigNumerator) + "/" + juce::String(timeSigDenominator));
    proj->setProperty("sample_rate", sampleRate);

    // 音乐音符细分对应毫秒数 (用于精准对齐压缩器释放时间)
    const double beatMs = 60000.0 / std::max(bpm, 20.0);
    juce::DynamicObject::Ptr subs = new juce::DynamicObject();
    subs->setProperty("quarter_note_ms", beatMs);
    subs->setProperty("eighth_note_ms", beatMs * 0.5);
    subs->setProperty("sixteenth_note_ms", beatMs * 0.25);
    proj->setProperty("musical_subdivisions", juce::var(subs.get()));
    root->setProperty("project_context", juce::var(proj.get()));

    // 2. 动态声学特征
    juce::DynamicObject::Ptr dyn = new juce::DynamicObject();
    dyn->setProperty("peak_dbfs", peakDb);
    dyn->setProperty("true_rms_dbfs", rmsDb);
    dyn->setProperty("crest_factor_db", crestFactorDb);
    dyn->setProperty("dynamic_range_db", dynamicRangeDb);
    dyn->setProperty("percussive_onsets", percussiveOnsetCount);
    dyn->setProperty("percussive_onset_ratio", percussiveOnsetRatio);
    dyn->setProperty("noise_band_flatness", noiseBandFlatness);
    dyn->setProperty("spectral_shape_variation", spectralShapeVariation);
    dyn->setProperty("centroid_variation", centroidVariation);
    dyn->setProperty("active_seconds", activeSeconds);
    dyn->setProperty("transient_character", juce::String::fromUTF8(transientCharacter.c_str()));
    root->setProperty("acoustic_dynamics", juce::var(dyn.get()));

    // 3. 五频段能量分布 (%)
    juce::DynamicObject::Ptr spec = new juce::DynamicObject();
    spec->setProperty("sub_under_60hz_pct", subEnergyPct);
    spec->setProperty("bass_60_250hz_pct", bassEnergyPct);
    spec->setProperty("mid_250_2khz_pct", midEnergyPct);
    spec->setProperty("presence_2k_6khz_pct", presenceEnergyPct);
    spec->setProperty("air_above_6khz_pct", airEnergyPct);
    root->setProperty("spectral_distribution", juce::var(spec.get()));

    // 4. 声源与风格推断
    juce::DynamicObject::Ptr inf = new juce::DynamicObject();
    inf->setProperty("source_type", juce::String::fromUTF8(inferredSource.c_str()));
    inf->setProperty("genre_hint", juce::String::fromUTF8(inferredGenreHint.c_str()));
    root->setProperty("inferred_context", juce::var(inf.get()));

    if (userPrompt.isNotEmpty())
    {
        root->setProperty("user_custom_prompt", userPrompt);
    }

    return juce::JSON::toString(juce::var(root.get()), true);
}

AcousticFeatureExtractor::AcousticFeatureExtractor()
{
    captureBuffer.resize(static_cast<size_t>(kMaxBufferSamples), 0.0f);
    captureSideBuffer.resize(static_cast<size_t>(kMaxBufferSamples), 0.0f);
    startTimerHz(30);
}

AcousticFeatureExtractor::~AcousticFeatureExtractor()
{
    stopTimer();
    onAnalysisCompleted = nullptr;
    onCaptureCompleted = nullptr;
}

void AcousticFeatureExtractor::timerCallback()
{
    if (analysisPending.exchange(false, std::memory_order_acq_rel))
    {
        if (onCaptureCompleted) onCaptureCompleted(copyCapture());
        else computeFeatures();
    }
}

void AcousticFeatureExtractor::prepare(double sr, int maxBlockSize)
{
    juce::ignoreUnused(maxBlockSize);
    sampleRate = (std::isfinite(sr) && sr > 0.0) ? sr : 48000.0;
    reset();
}

void AcousticFeatureExtractor::reset()
{
    const juce::SpinLock::ScopedLockType guard(captureLock);
    analysisPending.store(false, std::memory_order_release);
    listeningActive.store(false, std::memory_order_relaxed);
    waitingForSignal.store(false, std::memory_order_relaxed);
    samplesCollected.store(0, std::memory_order_relaxed);
    std::fill(captureBuffer.begin(), captureBuffer.end(), 0.0f);
    std::fill(captureSideBuffer.begin(), captureSideBuffer.end(), 0.0f);
}

void AcousticFeatureExtractor::startListening(double targetDurationSeconds, bool waitForSignal)
{
    const juce::SpinLock::ScopedLockType guard(captureLock);
    analysisPending.store(false, std::memory_order_release);
    if (!std::isfinite(targetDurationSeconds)) targetDurationSeconds = Params::listenDurationSeconds;
    totalTargetSamples = std::min<int64_t>(kMaxBufferSamples, static_cast<int64_t>(std::clamp(targetDurationSeconds, 1.0, 30.0) * sampleRate.load()));
    samplesCollected.store(0, std::memory_order_relaxed);
    waitingForSignal.store(waitForSignal, std::memory_order_relaxed);
    listeningActive.store(true, std::memory_order_relaxed);
}

void AcousticFeatureExtractor::stopListeningAndCompute()
{
    cancelListening();
    if (onCaptureCompleted) onCaptureCompleted(copyCapture());
    else computeFeatures();
}

double AcousticFeatureExtractor::getRecordedSeconds() const noexcept
{
    const int64_t cur = samplesCollected.load(std::memory_order_relaxed);
    return (sampleRate > 0.0) ? (static_cast<double>(cur) / sampleRate) : 0.0;
}

float AcousticFeatureExtractor::getProgress() const noexcept
{
    if (!listeningActive.load(std::memory_order_relaxed))
        return 0.0f;
    const int64_t cur = samplesCollected.load(std::memory_order_relaxed);
    return std::clamp(static_cast<float>(cur) / static_cast<float>(totalTargetSamples.load()), 0.0f, 1.0f);
}

void AcousticFeatureExtractor::processBlock(const juce::AudioBuffer<float>& buffer, double currentBpm, int timeSigNum, int timeSigDen)
{
    hostBpm.store(static_cast<float>(currentBpm), std::memory_order_relaxed);
    hostTimeSigNum.store(timeSigNum, std::memory_order_relaxed);
    hostTimeSigDen.store(timeSigDen, std::memory_order_relaxed);

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    if (numSamples <= 0 || numChannels <= 0)
        return;

    const auto* chL = buffer.getReadPointer(0);
    const auto* chR = (numChannels > 1) ? buffer.getReadPointer(1) : chL;

    // 实时计算输入峰值供 UI 动效绘制
    float blkPeak = 0.0f;
    for (int n = 0; n < numSamples; ++n)
    {
        const float absL = std::abs(chL[n]);
        const float absR = (numChannels > 1) ? std::abs(chR[n]) : absL;
        blkPeak = std::max(blkPeak, std::max(absL, absR));
    }
    liveInputPeak.store(ampToDb(blkPeak), std::memory_order_relaxed);

    const juce::SpinLock::ScopedTryLockType guard(captureLock);
    if (!guard.isLocked() || !listeningActive.load(std::memory_order_relaxed))
        return;

    const int64_t cur = samplesCollected.load(std::memory_order_relaxed);
    const int64_t target = totalTargetSamples;
    const int64_t remain = target - cur;

    if (remain <= 0)
        return;

    // 只在启动时等待有效输入；触发后的休止也属于这段连续素材。
    // 任一声道超过 -60 dBFS 即触发，反相立体声不会因中间声道抵消而漏检。
    int firstSample = 0;
    if (waitingForSignal.load(std::memory_order_relaxed))
    {
        constexpr float triggerAmplitude = 0.001f;
        for (; firstSample < numSamples; ++firstSample)
            if ((std::isfinite(chL[firstSample]) && std::abs(chL[firstSample]) >= triggerAmplitude)
                || (std::isfinite(chR[firstSample]) && std::abs(chR[firstSample]) >= triggerAmplitude)) break;
        if (firstSample == numSamples) return;
        waitingForSignal.store(false, std::memory_order_relaxed);
    }
    const int samplesToCopy = static_cast<int>(std::min(static_cast<int64_t>(numSamples - firstSample), remain));

    // 同步捕获 Mid (L+R)/2 与 Side (L-R)/2，用于立体声宽度分析
    for (int n = 0; n < samplesToCopy; ++n)
    {
        const size_t writeIdx = static_cast<size_t>((cur + n) % kMaxBufferSamples);
        const float l = std::isfinite(chL[n + firstSample]) ? chL[n + firstSample] : 0.0f;
        const float r = std::isfinite(chR[n + firstSample]) ? chR[n + firstSample] : 0.0f;
        captureBuffer[writeIdx] = 0.5f * l + 0.5f * r;
        captureSideBuffer[writeIdx] = (numChannels > 1) ? (0.5f * l - 0.5f * r) : 0.0f;
    }

    const int64_t newCount = cur + samplesToCopy;
    samplesCollected.store(newCount, std::memory_order_relaxed);

    // 达成目标采样点数后自动完成并触发异步特征计算
    if (newCount >= target)
    {
        listeningActive.store(false, std::memory_order_relaxed);
        analysisPending.store(true, std::memory_order_release);
    }
}

void AcousticFeatureExtractor::cancelListening()
{
    const juce::SpinLock::ScopedLockType guard(captureLock);
    listeningActive.store(false);
    waitingForSignal.store(false);
    analysisPending.store(false);
}

ListenCapture AcousticFeatureExtractor::copyCapture() const
{
    const juce::SpinLock::ScopedLockType guard(captureLock);
    ListenCapture result;
    const auto count = size_t(std::clamp<int64_t>(samplesCollected.load(), 0, kMaxBufferSamples));
    result.mid.assign(captureBuffer.begin(), captureBuffer.begin() + count);
    result.side.assign(captureSideBuffer.begin(), captureSideBuffer.begin() + count);
    result.sampleRate = sampleRate.load(); result.bpm = hostBpm.load();
    result.timeSigNum = hostTimeSigNum.load(); result.timeSigDen = hostTimeSigDen.load();
    return result;
}

void AcousticFeatureExtractor::computeFeatures()
{
    cancelListening();
    auto feat = analyseCapture(copyCapture());
    { std::lock_guard<std::mutex> lock(featureMutex); cachedFeatures = feat; }
    if (onAnalysisCompleted) onAnalysisCompleted(feat);
}

AcousticFeatures AcousticFeatureExtractor::analyseCapture(const ListenCapture& capture, bool inferSource)
{
    if (!(capture.sampleRate > 0) || capture.mid.size() != capture.side.size()) return {};
    const auto& captureBuffer = capture.mid;
    const auto& captureSideBuffer = capture.side;
    const int64_t totalSamples = int64_t(capture.mid.size());
    // 1. 扫描全局 Peak 并修剪首尾留白静音 (Leading & Trailing Inactive Silence Trimming)
    float peakAmp = 0.0f;
    for (int64_t i = 0; i < totalSamples; ++i)
    {
        const auto k = static_cast<size_t>(i);
        const float absS = std::max(std::abs(captureBuffer[k] + captureSideBuffer[k]),
                                    std::abs(captureBuffer[k] - captureSideBuffer[k]));
        if (absS > peakAmp) peakAmp = absS;
    }

    const float peakDbVal = ampToDb(peakAmp);
    // 活跃窗口保持相同时长，不能在 192kHz 下把统计窗口缩短为 48kHz 的四分之一。
    const int blockSize = std::max(1, int(std::lround(capture.sampleRate * (512.0 / 48000.0))));
    const float activeThresholdAmp = peakAmp * 0.05f; // -26dB 相对发声门限

    int64_t firstActiveSample = -1;
    int64_t lastActiveSample = -1;
    double activeMidSumSq = 0.0;
    double activeSideSumSq = 0.0;
    int64_t activeSampleCount = 0;

    std::vector<float> blockRms;
    blockRms.reserve(static_cast<size_t>(totalSamples / blockSize));

    for (int64_t i = 0; i < totalSamples; i += blockSize)
    {
        const int len = static_cast<int>(std::min(static_cast<int64_t>(blockSize), totalSamples - i));
        double blkMidSum = 0.0;
        double blkSideSum = 0.0;
        for (int k = 0; k < len; ++k)
        {
            const float m = captureBuffer[static_cast<size_t>(i + k)];
            const float s = captureSideBuffer[static_cast<size_t>(i + k)];
            blkMidSum += m * m;
            blkSideSum += s * s;
        }
        const float blkRms = std::sqrt(static_cast<float>((blkMidSum + blkSideSum) / len));
        if (blkRms >= activeThresholdAmp)
        {
            if (firstActiveSample < 0)
                firstActiveSample = i;
            lastActiveSample = i + len;

            activeMidSumSq += blkMidSum;
            activeSideSumSq += blkSideSum;
            activeSampleCount += len;
            blockRms.push_back(ampToDb(blkRms));
        }
    }

    if (firstActiveSample < 0)
    {
        firstActiveSample = 0;
        lastActiveSample = totalSamples;
    }



    // 实际有效播放时长区间（彻底排除 20s 中只有 6s 发声而导致的后置留白干扰！）
    const int64_t effectivePlaybackSamples = std::max(lastActiveSample - firstActiveSample, static_cast<int64_t>(1));

    float activeRmsDbVal = peakDbVal - 12.0f;
    float dutyCyclePct = 100.0f;
    float stereoWidthRatio = 0.0f;

    if (activeSampleCount > 0)
    {
        activeRmsDbVal = ampToDb(std::sqrt(static_cast<float>((activeMidSumSq + activeSideSumSq) / activeSampleCount)));
        // 占空比计算：仅相对于实际播放发声区间，不再被末尾空白稀释！
        dutyCyclePct = std::clamp((static_cast<float>(activeSampleCount) / static_cast<float>(effectivePlaybackSamples)) * 100.0f, 1.0f, 100.0f);

        const float midRms = std::sqrt(static_cast<float>(activeMidSumSq / activeSampleCount));
        const float sideRms = std::sqrt(static_cast<float>(activeSideSumSq / activeSampleCount));
        stereoWidthRatio = sideRms / std::max(midRms, 1e-15f);
    }

    // 活跃段真实波峰因数 (Active Crest Factor)
    const float activeCrestFactorVal = peakDbVal - activeRmsDbVal;

    // 计算动态离散跨度 (P90 - P10) 仅基于有效发声块
    std::sort(blockRms.begin(), blockRms.end());
    float p10 = activeRmsDbVal - 6.0f, p90 = activeRmsDbVal + 6.0f;
    if (!blockRms.empty())
    {
        p10 = blockRms[static_cast<size_t>(blockRms.size() * 0.10f)];
        p90 = blockRms[static_cast<size_t>(blockRms.size() * 0.90f)];
    }
    const float dynRange = std::max(p90 - p10, 0.0f);

    // 2 毫秒能量帧保留拍头顺序。尾音和混响会增加占空比，但不会抹掉快起慢落。
    const int onsetHop = std::max(1, int(std::lround(capture.sampleRate * 0.002)));
    std::vector<double> onsetPower;
    for (int64_t offset = firstActiveSample; offset < lastActiveSample; offset += onsetHop)
    {
        const int count = int(std::min(int64_t(onsetHop), lastActiveSample - offset));
        double power = 0;
        for (int n = 0; n < count; ++n)
        {
            const auto k = size_t(offset + n);
            power += double(captureBuffer[k]) * captureBuffer[k]
                   + double(captureSideBuffer[k]) * captureSideBuffer[k];
        }
        onsetPower.push_back(power / count);
    }
    int risingEvents = 0, percussiveEvents = 0;
    for (size_t i = 5; i + 60 < onsetPower.size(); ++i)
    {
        if (onsetPower[i] < double(peakAmp) * peakAmp * 0.0025
            || onsetPower[i] < 6.31 * std::max(onsetPower[i - 5], 1.0e-30)) continue;
        ++risingEvents;
        // 拍头后 20 毫秒内找峰，比较其后 50~100 毫秒的平均尾部能量。
        const auto peakIt = std::max_element(onsetPower.begin() + i, onsetPower.begin() + i + 10);
        const size_t peakIndex = size_t(peakIt - onsetPower.begin());
        const double tailPower = std::accumulate(onsetPower.begin() + peakIndex + 25,
                                                 onsetPower.begin() + peakIndex + 50, 0.0) / 25;
        if (*peakIt >= 1.413 * tailPower) ++percussiveEvents; // 至少 1.5 dB 的自然衰减
        i += 49; // 100 毫秒内不把同一次拍头重复计数
    }
    const float percussiveRatio = risingEvents > 0 ? float(percussiveEvents) / risingEvents : 0;

    // 2. 真实 FFT (4096 点) 频谱质心与 5 频段精确能量积分
    // 维持约 85 毫秒分析窗口及相近频率分辨率，尤其避免高采样率低频分类失真。
    const int fftOrder = std::clamp(int(std::lround(std::log2(capture.sampleRate * (4096.0 / 48000.0)))), 10, 15);
    const int fftSize = 1 << fftOrder;
    juce::dsp::FFT fft(fftOrder);
    juce::dsp::WindowingFunction<float> window(fftSize, juce::dsp::WindowingFunction<float>::hann);

    std::vector<float> fftBuffer(fftSize * 2, 0.0f);
    double totalSubEnergy = 0.0, totalBassEnergy = 0.0, totalMidEnergy = 0.0, totalPresEnergy = 0.0, totalAirEnergy = 0.0;
    double weightedFreqSum = 0.0, totalMagSum = 0.0;
    double flatnessSum = 0, flatnessWeight = 0;

    const float binWidthHz = static_cast<float>(capture.sampleRate) / static_cast<float>(fftSize);

    // 同一帧先合并中、侧能量，再统计归一化宽频带形状，避免声道相位影响。
    std::vector<std::array<double, 7>> shapeFrames;
    std::vector<double> frameCentroids;
    constexpr double shapeEdges[] = {60, 150, 300, 600, 1200, 2400, 4800, 10000};
    for (int64_t i = firstActiveSample; i + fftSize <= lastActiveSample; i += (fftSize / 2))
    {
        std::array<double, 7> shape {};
        double frameEnergy = 0, frameWeightedFrequency = 0;
        for (int component = 0; component < 2; ++component)
        {
        float framePeak = 0.0f;
        for (int k = 0; k < fftSize; ++k)
        {
            fftBuffer[static_cast<size_t>(k)] = (component == 0 ? captureBuffer : captureSideBuffer)[static_cast<size_t>(i + k)];
            framePeak = std::max(framePeak, std::abs(fftBuffer[static_cast<size_t>(k)]));
        }
        if (framePeak < activeThresholdAmp) continue;

        window.multiplyWithWindowingTable(fftBuffer.data(), fftSize);
        std::fill(fftBuffer.begin() + fftSize, fftBuffer.end(), 0.0f);
        fft.performFrequencyOnlyForwardTransform(fftBuffer.data());

        double bandLog = 0, bandSum = 0; int bandBins = 0;
        for (int bin = 1; bin < fftSize / 2; ++bin)
        {
            const float mag = fftBuffer[static_cast<size_t>(bin)];
            const float energy = mag * mag;
            const float freq = bin * binWidthHz;

            if (freq >= 700 && freq <= 6000)
            { bandLog += std::log(std::max(double(energy), 1.0e-30)); bandSum += energy; ++bandBins; }
            if (freq < 20.0f) continue; // 过滤亚音频 DC 偏移与超低次声波底噪

            frameEnergy += energy; frameWeightedFrequency += freq * energy;
            for (size_t band = 0; band < shape.size(); ++band)
                if (freq >= shapeEdges[band] && freq < shapeEdges[band + 1]) shape[band] += energy;
            weightedFreqSum += freq * energy;
            totalMagSum += energy;

            if (freq < 60.0f)
                totalSubEnergy += energy;
            else if (freq < 250.0f)
                totalBassEnergy += energy;
            else if (freq < 2000.0f)
                totalMidEnergy += energy;
            else if (freq < 6000.0f)
                totalPresEnergy += energy;
            else
                totalAirEnergy += energy;
        }
        if (bandBins > 0 && bandSum > 1.0e-25)
        { flatnessSum += std::exp(bandLog / bandBins) * bandBins; flatnessWeight += bandSum; }
        }
        if (frameEnergy > 1.0e-25)
        {
            for (auto& value : shape) value /= frameEnergy;
            shapeFrames.push_back(shape);
            frameCentroids.push_back(frameWeightedFrequency / frameEnergy);
        }
    }
    double shapeVariation = 0, centroidChange = 0;
    if (!shapeFrames.empty())
    {
        const double count = double(shapeFrames.size());
        for (size_t band = 0; band < 7; ++band)
        {
            double sum = 0, square = 0;
            for (const auto& frame : shapeFrames) { sum += frame[band]; square += frame[band] * frame[band]; }
            shapeVariation += std::sqrt(std::max(0.0, square / count - (sum / count) * (sum / count)));
        }
        const double mean = std::accumulate(frameCentroids.begin(), frameCentroids.end(), 0.0) / count;
        double variance = 0;
        for (auto value : frameCentroids) variance += (value - mean) * (value - mean);
        centroidChange = std::sqrt(variance / count) / std::max(mean, 1.0);
    }
    const float noiseFlatness = flatnessWeight > 0 ? float(flatnessSum / flatnessWeight) : 0;

    float spectralCentroidVal = 1000.0f;
    if (totalMagSum > 1e-25)
        spectralCentroidVal = static_cast<float>(weightedFreqSum / totalMagSum);

    const double totalEnergy = totalSubEnergy + totalBassEnergy + totalMidEnergy + totalPresEnergy + totalAirEnergy + 1e-30;
    const float subPct  = static_cast<float>((totalSubEnergy / totalEnergy) * 100.0);
    const float bassPct = static_cast<float>((totalBassEnergy / totalEnergy) * 100.0);
    const float midPct  = static_cast<float>((totalMidEnergy / totalEnergy) * 100.0);
    const float presPct = static_cast<float>((totalPresEnergy / totalEnergy) * 100.0);
    const float airPct  = static_cast<float>((totalAirEnergy / totalEnergy) * 100.0);
    const float lowEnergy = subPct + bassPct;

    AcousticSourceType srcTypeEnum = AcousticSourceType::Unknown;
    bool isHighConfidence = false;
    std::string transChar, srcType, genreHint, clarifyingQuestion;
    const bool valid = peakDbVal > -90 && activeSampleCount >= capture.sampleRate * 0.12;
    const bool drumEnvelope = percussiveEvents >= 2 && percussiveRatio >= 0.65f;
    const bool noisyAttack = noiseFlatness >= 0.12f;
    const bool lowPulse = lowEnergy > 75 && spectralCentroidVal < 300 && dutyCyclePct < 55;
    if (!valid)
    {
        srcType = "有效音频不足";
        clarifyingQuestion = "请播放包含代表性强弱变化的音频后重新监听。";
    }
    else if (!inferSource)
    {
        // 手动目标仅使用可测量的电平和动态，跳过所有声源类别判定。
    }
    else if ((drumEnvelope && noisyAttack) || lowPulse
             || (dutyCyclePct < 40 && activeCrestFactorVal >= 8 && noiseFlatness > .08f))
    {
        srcTypeEnum = lowPulse ? AcousticSourceType::KickDrum : AcousticSourceType::SnareDrum;
        srcType = lowPulse ? "推测低频打击乐" : "推测打击乐";
        transChar = "快起衰减型瞬态";
    }
    else if (stereoWidthRatio > .18f && dutyCyclePct > 70 && lowEnergy > 8 && midPct > 15
             && presPct + airPct > 8 && noiseFlatness > .035f && percussiveRatio < .65f)
    {
        srcTypeEnum = AcousticSourceType::MasterMixBus;
        srcType = "推测混音或总线"; transChar = "持续宽频复合动态";
    }
    else if (lowEnergy < 50 && midPct >= 40 && noiseFlatness < .12f
             && ((shapeVariation > .80 && centroidChange > .35 && presPct + airPct > .6)
                 || (midPct < 90 && presPct + airPct > 3 && stereoWidthRatio < .35f && !drumEnvelope)))
    {
        // 元音到辅音及音节的谱形变化可补充频段占比；不要求人声一定明亮或为单声道。
        srcTypeEnum = AcousticSourceType::LeadVocal;
        srcType = "推测人声或旋律主奏"; transChar = "谐波发声伴随音色变化";
    }
    else if (lowEnergy < 15 && midPct > 70 && noiseFlatness > .001f && noiseFlatness < .12f)
    {
        // 合唱与持续弦乐存在重叠证据，保留歧义并采用更温和的候选偏好。
        srcTypeEnum = AcousticSourceType::BackingVocals;
        srcType = "合唱或持续旋律（待确认）"; transChar = "中频集中持续织体";
    }
    else
    {
        srcTypeEnum = AcousticSourceType::AmbiguousInstrument;
        srcType = "推测乐器或持续音色"; transChar = "音色与动态混合特征";
    }

    // 4. 写入缓存并通知
    AcousticFeatures feat;
    feat.bpm = capture.bpm;
    feat.timeSigNumerator = capture.timeSigNum;
    feat.timeSigDenominator = capture.timeSigDen;
    feat.sampleRate = capture.sampleRate;
    feat.peakDb = peakDbVal;
    feat.rmsDb = activeRmsDbVal; // 使用真实活跃发声段 RMS
    feat.crestFactorDb = activeCrestFactorVal;
    feat.dynamicRangeDb = dynRange;
    feat.spectralCentroidHz = spectralCentroidVal;
    feat.dutyCyclePct = dutyCyclePct;
    feat.durationSeconds = float(totalSamples / capture.sampleRate);
    feat.activeSeconds = float(activeSampleCount / capture.sampleRate);
    feat.percussiveOnsetCount = percussiveEvents;
    feat.percussiveOnsetRatio = percussiveRatio;
    feat.noiseBandFlatness = noiseFlatness;
    feat.spectralShapeVariation = float(shapeVariation);
    feat.centroidVariation = float(centroidChange);
    feat.stereoWidthRatio = stereoWidthRatio;
    feat.transientCharacter = transChar;
    feat.subEnergyPct = subPct;
    feat.bassEnergyPct = bassPct;
    feat.midEnergyPct = midPct;
    feat.presenceEnergyPct = presPct;
    feat.airEnergyPct = airPct;
    feat.sourceType = srcTypeEnum;
    feat.isHighConfidence = isHighConfidence;
    feat.inferredSource = srcType;
    feat.inferredGenreHint = genreHint;
    feat.clarifyingQuestion = clarifyingQuestion;

    return feat;
}

AcousticFeatures AcousticFeatureExtractor::getLatestFeatures() const
{
    std::lock_guard<std::mutex> lock(featureMutex);
    return cachedFeatures;
}

void AcousticFeatureExtractor::calibrateRecommendationWithVirtualDsp(CompressorPresetRecommendation& rec) const
{
    calibrateCapture(copyCapture(), rec);
}

void AcousticFeatureExtractor::calibrateCapture(const ListenCapture& capture, CompressorPresetRecommendation& rec,
                                               const std::atomic<bool>* cancelled)
{
    const auto isCancelled = [&] { return cancelled && cancelled->load(); };
    rec.calibrationWithinRange = false; rec.usable = false;
    rec.calibrationPeakLimited = false;
    const auto& captureBuffer = capture.mid;
    const auto& captureSideBuffer = capture.side;
    const int64_t startSample = 0;
    const int64_t availableSamples = int64_t(std::min(capture.mid.size(), capture.side.size()));
    if (availableSamples < 512 || !(capture.sampleRate > 0) || isCancelled()) return;
    // 使用整个有效捕获区间，避免只听前三秒而漏掉后面的副歌或强音。
    const double sr = capture.sampleRate;
    const int length = static_cast<int>(availableSamples);
    juce::AudioBuffer<float> input(2, length);
    float inputPeak = 0.0f;
    for (int n = 0; n < length; ++n)
    {
        const auto i = static_cast<size_t>(startSample + n);
        input.setSample(0, n, captureBuffer[i] + captureSideBuffer[i]);
        input.setSample(1, n, captureBuffer[i] - captureSideBuffer[i]);
        inputPeak = std::max({inputPeak, std::abs(input.getSample(0, n)), std::abs(input.getSample(1, n))});
    }
    if (inputPeak < 1.0e-6f) return;
    // 活跃窗口由输入单独决定，输入输出共用同一掩码；不可让压缩后的门限改变测量范围。
    const int window = std::max(1, int(std::lround(sr * 0.020)));
    std::vector<bool> active(static_cast<size_t>((length + window - 1) / window), false);
    double inputEnergy = 0;
    int64_t measuredSamples = 0;
    for (int offset = 0; offset < length; offset += window)
    {
        const int count = std::min(window, length - offset);
        double energy = 0;
        for (int c = 0; c < 2; ++c)
            for (int n = 0; n < count; ++n)
            { const double x = input.getSample(c, offset + n); energy += x * x; }
        if (energy / (2 * count) >= double(inputPeak) * inputPeak * 0.0001)
        {
            active[static_cast<size_t>(offset / window)] = true;
            inputEnergy += energy;
            measuredSamples += 2 * count;
        }
    }
    if (measuredSamples == 0) return;
    rec.calibrationInputRmsDb = ampToDb(float(std::sqrt(inputEnergy / double(measuredSamples))));
    const float targetDb = rec.calibrationInputRmsDb + 0.2f;
    const int renderBlock = std::max(16, int(std::lround(sr * .002)));
    const juce::dsp::ProcessSpec spec {sr, juce::uint32(renderBlock), 2};
    struct Level { float rmsDb = -120, peakDb = -120, gr = 0, maxGr = 0; };
    auto render = [&](float setting)
    {
        dsp_engines::VoxEngine vox;
        dsp_engines::VcaEngine vca;
        dsp_engines::FetEngine fet;
        dsp_engines::OptoEngine opto;
        switch (rec.mode)
        {
            case Params::ModeVOX:
                vox.prepare(spec); vox.setGate(rec.voxGate); vox.setComp(rec.voxComp);
                vox.setGain(setting); vox.setMix(rec.voxMix); vox.setSmooth(rec.voxSmooth); break;
            case Params::ModeVCA:
                vca.prepare(spec); vca.thresholdDb = rec.vcaThreshold; vca.ratio = rec.vcaRatio;
                vca.attackMs = rec.vcaAttack; vca.releaseMs = rec.vcaRelease <= 0 ? 2000.0f : rec.vcaRelease;
                vca.makeupDb = setting; vca.sidechainHpHz = rec.vcaScHp; vca.mixPercent = rec.vcaMix; break;
            case Params::ModeFET:
                fet.prepare(spec); fet.fetDrive = rec.fetInput; fet.fetRatioIdx = rec.fetRatioIdx;
                fet.attackMs = rec.fetAttack; fet.releaseMs = rec.fetRelease;
                fet.makeupDb = setting; fet.sidechainHpHz = rec.fetScHp; fet.mixPercent = rec.fetMix; break;
            case Params::ModeOpto:
                opto.prepare(spec); opto.optoPeakReduction = rec.optoPR; opto.isLimit = rec.optoIsLimit;
                opto.makeupDb = setting; opto.sidechainHpHz = rec.optoScHp; opto.mixPercent = rec.optoMix; break;
            default: return Level {-120.0f, -120.0f};
        }
        // 候选参数全部设置后再初始化平滑器。否则旧输出增益的起始瞬态会支配峰值搜索。
        switch (rec.mode)
        {
            case Params::ModeVOX: vox.reset(); break;
            case Params::ModeVCA: vca.reset(); break;
            case Params::ModeFET: fet.reset(); break;
            case Params::ModeOpto: opto.reset(); break;
        }
        juce::AudioBuffer<float> block(2, renderBlock);
        float peak = 0.0f;
        double outputEnergy = 0, grSum = 0, grWeight = 0;
        float maxGr = 0;
        const int tail = rec.mode == Params::ModeVOX ? dsp_engines::VoxEngine::getLatencySamples(sr, rec.voxSmooth) : 0;
        for (int offset = 0; offset < length + tail; offset += renderBlock)
        {
            if (isCancelled()) return Level {};
            const int count = std::min(renderBlock, length + tail - offset);
            block.clear();
            const int copyCount = std::max(0, std::min(count, length - offset));
            if (copyCount > 0)
                for (int c = 0; c < 2; ++c) block.copyFrom(c, 0, input, c, offset, copyCount);
            juce::AudioBuffer<float> view(block.getArrayOfWritePointers(), 2, count);
            switch (rec.mode)
            {
                case Params::ModeVOX: vox.process(view); break;
                case Params::ModeVCA: vca.process(view); break;
                case Params::ModeFET: fet.process(view); break;
                case Params::ModeOpto: opto.process(view); break;
            }
            const float gr = rec.mode == Params::ModeVOX ? vox.getCompGrDb() : rec.mode == Params::ModeVCA ? vca.getCurrentGrDb()
                           : rec.mode == Params::ModeFET ? fet.getCurrentGrDb() : opto.getCurrentGrDb();
            maxGr = std::max(maxGr, gr);
            double frameInputEnergy = 0;
            peak = std::max({peak, view.getMagnitude(0, count)});
            for (int n = 0; n < count; ++n)
            {
                const int sourceSample = offset + n - tail;
                if (sourceSample >= 0 && sourceSample < length && active[static_cast<size_t>(sourceSample / window)])
                    for (int c = 0; c < 2; ++c)
                    { const double y = view.getSample(c, n), x = input.getSample(c, sourceSample);
                      outputEnergy += y * y; frameInputEnergy += x * x; }
            }
            grSum += gr * frameInputEnergy; grWeight += frameInputEnergy;
        }
        return Level {ampToDb(float(std::sqrt(outputEnergy / double(measuredSamples)))), ampToDb(peak),
                      float(grSum / std::max(grWeight, 1.0e-30)), maxGr};
    };
    // 调深度和配平是两个独立步骤。主体 GR 由真实引擎读取，不能用旋钮刻度冒充。
    const float referenceSetting = rec.mode == Params::ModeVOX || rec.mode == Params::ModeVCA ? 0.0f
                                 : rec.mode == Params::ModeFET ? 24.0f : 40.0f;
    if (rec.requestedGrDb > 0)
    {
        float shallow = 0, deep = rec.mode == Params::ModeVOX ? 36.0f : rec.mode == Params::ModeVCA ? 60.0f
                                  : rec.mode == Params::ModeFET ? 48.0f : 100.0f;
        auto setDepth = [&](float amount)
        {
            amount = std::round(amount * 10) * .1f;
            if (rec.mode == Params::ModeVOX) rec.voxComp = -amount;
            else if (rec.mode == Params::ModeVCA) rec.vcaThreshold = -amount;
            else if (rec.mode == Params::ModeFET) rec.fetInput = amount;
            else rec.optoPR = amount;
        };
        float bestDepth = 0, bestError = 1000;
        for (int i = 0; i < 9; ++i)
        {
            if (isCancelled()) return;
            const float amount = (shallow + deep) * .5f;
            setDepth(amount);
            const auto measured = render(referenceSetting);
            const float error = std::abs(measured.gr - rec.requestedGrDb);
            if (error < bestError) { bestError = error; bestDepth = amount; }
            if (measured.gr < rec.requestedGrDb) shallow = amount; else deep = amount;
        }
        setDepth(bestDepth);
    }
    if (isCancelled()) return;
    const auto reference = render(referenceSetting);
    // 四引擎的后置输出均为线性增益。下列只是旋钮单位换算，最终仍重放生产引擎核验。
    const auto outputDb = [&](float setting)
    {
        if (rec.mode == Params::ModeVOX || rec.mode == Params::ModeVCA) return setting;
        if (rec.mode == Params::ModeFET)
            return setting <= .2f ? -80.0f : setting < 1 ? -80 + (setting - .2f) * 57 / .8f : setting - 24;
        const float atOne = -60 + 60 * std::pow(1.0f / 40, .75f);
        return setting < 1 ? -80 + setting * (atOne + 80)
                          : setting <= 40 ? -60 + 60 * std::pow(setting / 40, .75f)
                                          : 40 * std::pow((setting - 40) / 60, 1.2f);
    };
    float low = rec.mode == Params::ModeVOX ? -36.0f : rec.mode == Params::ModeVCA ? -24.0f : 0.0f;
    float high = rec.mode == Params::ModeVOX ? 0.0f : rec.mode == Params::ModeVCA ? 24.0f : rec.mode == Params::ModeFET ? 48.0f : 100.0f;
    const float mix = rec.mode == Params::ModeVOX ? rec.voxMix : rec.mode == Params::ModeVCA ? rec.vcaMix
                    : rec.mode == Params::ModeFET ? rec.fetMix : rec.optoMix;
    const float wantedGain = std::min(targetDb - reference.rmsDb, -0.1f - reference.peakDb);
    if (mix >= 99.999f)
    {
        for (int i = 0; i < 24; ++i)
        {
            const float middle = (low + high) * .5f;
            if (outputDb(middle) - outputDb(referenceSetting) <= wantedGain) low = middle;
            else high = middle;
        }
    }
    else
    {
        // 兼容其他调用方的并联方案：干声不随输出旋钮缩放，必须完整重放。
        for (int i = 0; i < 12 && !isCancelled(); ++i)
        {
            const float middle = (low + high) * .5f;
            const auto trial = render(middle);
            if (trial.rmsDb <= targetDb && trial.peakDb <= -.1f) low = middle; else high = middle;
        }
    }
    float selected = low;
    // 与宿主参数的 0.1 刻度一致，再验证最终量化值。
    selected = std::floor(selected * 10.0f) * 0.1f;
    switch (rec.mode)
    {
        case Params::ModeVOX: rec.voxGain = selected; break;
        case Params::ModeVCA: rec.vcaMakeup = selected; break;
        case Params::ModeFET: rec.fetOutput = selected; break;
        case Params::ModeOpto: rec.optoGain = selected; break;
    }
    rec.calibrationTargetDb = targetDb;
    const auto result = render(selected);
    rec.calibratedPeakDb = result.peakDb;
    rec.calibratedRmsDb = result.rmsDb;
    rec.calibrationWithinRange = std::abs(result.rmsDb - targetDb) <= 0.35f && result.peakDb <= 0;
    rec.calibrationPeakLimited = !rec.calibrationWithinRange && result.peakDb > -0.5f;
    rec.measuredGrDb = result.gr; rec.maximumGrDb = result.maxGr;
    rec.usable = !isCancelled() && std::isfinite(result.rmsDb) && std::isfinite(result.peakDb)
                 && result.peakDb <= 0 && std::abs(result.rmsDb - rec.calibrationInputRmsDb) <= 1.2f;
}

} // namespace ai
