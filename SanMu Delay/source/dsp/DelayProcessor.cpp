// ==============================================================================
// DelayProcessor.cpp - 核心延迟系统调度实现
// ==============================================================================

#include "DelayProcessor.h"
#include <cmath>

namespace dsp
{

DelayProcessor::DelayProcessor()
{
}

void DelayProcessor::prepare(double sr, int maxBlockSize)
{
    sampleRate = sr > 0.0 ? sr : 48000.0;

    engineModern.prepare(sampleRate, maxBlockSize);
    engineTape.prepare(sampleRate, maxBlockSize);
    engineBbd.prepare(sampleRate, maxBlockSize);
    engineDiffused.prepare(sampleRate, maxBlockSize);

    duckingDetector.prepare(sampleRate);

    // 时间平滑时间常数约 40ms：平滑无喀哒且响应跟手
    constexpr float tauSec = 0.040f;
    timeSmoothCoeff = std::exp(-1.0f / (static_cast<float>(sampleRate) * tauSec));

    reset();
}

void DelayProcessor::reset()
{
    engineModern.reset();
    engineTape.reset();
    engineBbd.reset();
    engineDiffused.reset();

    duckingDetector.reset();

    lfoPhase = 0.0f;

    // 默认置为 500ms 对应采样数
    const float initSamples = static_cast<float>(sampleRate * 0.5);
    smoothedDelaySamplesL = initSamples;
    smoothedDelaySamplesR = initSamples;
}

void DelayProcessor::process(juce::AudioBuffer<float>& mainBuffer,
                            const juce::AudioBuffer<float>* sidechainBuffer,
                            const juce::AudioPlayHead* playHead,
                            const juce::AudioProcessorValueTreeState& apvts) noexcept
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = mainBuffer.getNumSamples();
    const int numChannels = mainBuffer.getNumChannels();

    if (numSamples <= 0 || numChannels <= 0)
        return;

    // ---- 1. 读取参数单一数据源 (APVTS) ----
    const bool globalBypass = *apvts.getRawParameterValue(Params::IDs::global_bypass) > 0.5f;
    if (globalBypass)
        return; // 旁通模式直接输出原干信号

    const int engineIdx     = static_cast<int>(*apvts.getRawParameterValue(Params::IDs::engine));
    const int syncMode      = static_cast<int>(*apvts.getRawParameterValue(Params::IDs::sync_mode));
    const float manualBpm   = *apvts.getRawParameterValue(Params::IDs::manual_bpm);
    const int divisionL     = static_cast<int>(*apvts.getRawParameterValue(Params::IDs::division));
    const int divisionR     = static_cast<int>(*apvts.getRawParameterValue(Params::IDs::division_r));
    float timeMsL           = *apvts.getRawParameterValue(Params::IDs::time_l);
    float timeMsR           = *apvts.getRawParameterValue(Params::IDs::time_r);
    const bool timeLink     = *apvts.getRawParameterValue(Params::IDs::time_link) > 0.5f;
    const float feedbackRaw = *apvts.getRawParameterValue(Params::IDs::feedback);
    const int topology      = static_cast<int>(*apvts.getRawParameterValue(Params::IDs::topology));

    const float lowCutHz    = *apvts.getRawParameterValue(Params::IDs::damp_lowcut);
    const float highCutHz   = *apvts.getRawParameterValue(Params::IDs::damp_highcut);
    const float mixPct      = *apvts.getRawParameterValue(Params::IDs::mix);
    const float outGainDb   = *apvts.getRawParameterValue(Params::IDs::output_gain);
    const bool freeze       = *apvts.getRawParameterValue(Params::IDs::freeze) > 0.5f;

    // Ducking 参数
    const bool duckEnable   = *apvts.getRawParameterValue(Params::IDs::duck_enable) > 0.5f;
    const int duckSource    = static_cast<int>(*apvts.getRawParameterValue(Params::IDs::duck_source));
    const float duckThresh  = *apvts.getRawParameterValue(Params::IDs::duck_threshold);
    const float duckAmount  = *apvts.getRawParameterValue(Params::IDs::duck_amount);
    const float duckRelease = *apvts.getRawParameterValue(Params::IDs::duck_release);

    // 通用调制 (LFO)
    const float modRate     = *apvts.getRawParameterValue(Params::IDs::mod_rate);
    const float modDepth    = *apvts.getRawParameterValue(Params::IDs::mod_depth);

    // 引擎专属修饰
    const float tapeDrive   = *apvts.getRawParameterValue(Params::IDs::tape_drive);
    const float tapeWow     = *apvts.getRawParameterValue(Params::IDs::tape_wow);
    const float bbdWarmth   = *apvts.getRawParameterValue(Params::IDs::bbd_warmth);
    const float diffAmount  = *apvts.getRawParameterValue(Params::IDs::diffusion_amount);

    if (timeLink)
        timeMsR = timeMsL;

    // ---- 2. 宿主 BPM 提取与目标延迟采样数换算 ----
    if (playHead != nullptr)
    {
        if (auto pos = playHead->getPosition())
        {
            if (auto bpmOpt = pos->getBpm())
            {
                if (*bpmOpt > 20.0 && *bpmOpt < 999.0)
                    lastValidBpm = *bpmOpt;
            }
        }
    }

    float targetDelaySamplesL = 0.0f;
    float targetDelaySamplesR = 0.0f;

    if (syncMode == Params::SyncHost || syncMode == Params::SyncBpm)
    {
        // SyncHost 模式下优先使用宿主 BPM，若无宿主走带则回退至 manualBpm
        const double hostBpm = lastValidBpm.load(std::memory_order_relaxed);
        const double effectiveBpm = (syncMode == Params::SyncHost && hostBpm > 20.0 && hostBpm < 999.0)
                                        ? hostBpm
                                        : static_cast<double>(manualBpm);

        const double beatLengthSec = 60.0 / effectiveBpm;
        const double divBeatsL = Params::getDivisionBeats(divisionL);
        const double divBeatsR = timeLink ? divBeatsL : Params::getDivisionBeats(divisionR);

        targetDelaySamplesL = static_cast<float>(beatLengthSec * divBeatsL * sampleRate);
        targetDelaySamplesR = static_cast<float>(beatLengthSec * divBeatsR * sampleRate);
    }
    else // SyncFree
    {
        targetDelaySamplesL = (timeMsL * 0.001f) * static_cast<float>(sampleRate);
        targetDelaySamplesR = ((timeLink ? timeMsL : timeMsR) * 0.001f) * static_cast<float>(sampleRate);
    }

    // 在 Ping-Pong 模式下，强制左右声道采用绝对统一基准时间，杜绝跛脚与节奏紊乱
    if (topology == Params::TopologyPingPong)
    {
        targetDelaySamplesR = targetDelaySamplesL;
    }

    // 安全钳位范围 (2 采样至 5.0 秒，支持大长音符)
    const float maxDelaySamples = static_cast<float>(sampleRate * 5.0);
    targetDelaySamplesL = juce::jlimit(2.0f, maxDelaySamples, targetDelaySamplesL);
    targetDelaySamplesR = juce::jlimit(2.0f, maxDelaySamples, targetDelaySamplesR);

    // ---- 3. 安全反馈上界映射 (0~100% -> 0.0 ~ 0.985) ----
    const float safeFeedback = (feedbackRaw * 0.01f) * 0.985f;

    // ---- 4. 干湿比与输出增益换算 ----
    // 等功率余弦立体声干湿比交叉平衡
    const float mixAngle = (mixPct * 0.01f) * (3.141592653589793f * 0.5f);
    const float dryGain = std::cos(mixAngle);
    const float wetGain = std::sin(mixAngle);
    const float outputGainLinear = std::pow(10.0f, outGainDb * 0.05f);

    // 更新 Ducking 弹道参数
    duckingDetector.updateCoeffs(duckRelease);

    // 确定侧链信号可用性
    const bool useExternalSidechain = (duckSource == Params::DuckSourceExternal)
                                   && (sidechainBuffer != nullptr)
                                   && (sidechainBuffer->getNumChannels() > 0);

    const float* scReadL = useExternalSidechain ? sidechainBuffer->getReadPointer(0) : nullptr;
    const float* scReadR = (useExternalSidechain && sidechainBuffer->getNumChannels() > 1)
                                ? sidechainBuffer->getReadPointer(1)
                                : scReadL;

    float* mainL = mainBuffer.getWritePointer(0);
    float* mainR = numChannels > 1 ? mainBuffer.getWritePointer(1) : nullptr;

    // ---- 5. 核心采样级音频处理大循环 ----
    dsp_engines::EngineProcessContext ctx;
    ctx.feedback = safeFeedback;
    ctx.topology = topology;
    ctx.dampLowcutHz = lowCutHz;
    ctx.dampHighcutHz = highCutHz;
    ctx.freeze = freeze;

    // 引擎专属修饰分发
    if (engineIdx == Params::EngineTape)
    {
        ctx.modParam = tapeDrive;
        ctx.wowParam = tapeWow;
    }
    else if (engineIdx == Params::EngineBbd)
    {
        ctx.modParam = bbdWarmth;
        ctx.wowParam = 0.0f;
    }
    else if (engineIdx == Params::EngineDiffused)
    {
        ctx.modParam = diffAmount;
        ctx.wowParam = 0.0f;
    }
    else
    {
        ctx.modParam = 0.0f;
        ctx.wowParam = 0.0f;
    }

    // 选定当前活跃的物理引擎
    dsp_engines::DelayEngine* activeEngine = &engineModern;
    switch (engineIdx)
    {
        case Params::EngineTape:     activeEngine = &engineTape; break;
        case Params::EngineBbd:      activeEngine = &engineBbd; break;
        case Params::EngineDiffused: activeEngine = &engineDiffused; break;
        default:                     activeEngine = &engineModern; break;
    }

    // LFO 调制步进与最大摆幅 (Depth 100% 对应最大 5.0ms 摆动，形成从细腻合唱到明显颤音的广阔音色域)
    const float lfoPhaseInc = static_cast<float>(modRate / sampleRate);
    constexpr float maxModMs = 5.0f;
    const float modDepthSamples = (modDepth * 0.01f) * (maxModMs * 0.001f) * static_cast<float>(sampleRate);

    float maxInL = 0.0f, maxInR = 0.0f;
    float maxOutL = 0.0f, maxOutR = 0.0f;
    float maxWetL = 0.0f, maxWetR = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        // 平滑插值计算本采样点延迟时间
        smoothedDelaySamplesL = targetDelaySamplesL + timeSmoothCoeff * (smoothedDelaySamplesL - targetDelaySamplesL);
        smoothedDelaySamplesR = targetDelaySamplesR + timeSmoothCoeff * (smoothedDelaySamplesR - targetDelaySamplesR);

        // 正交 LFO 调制摆幅叠加 (硬零旁通保护：Depth == 0% 时 100% 原始内核零运算)
        if (modDepth > 0.001f)
        {
            lfoPhase += lfoPhaseInc;
            if (lfoPhase >= 1.0f)
                lfoPhase -= 1.0f;

            constexpr float twoPi = juce::MathConstants<float>::twoPi;
            const float lfoAngle = lfoPhase * twoPi;
            const float modOffsetL = std::sin(lfoAngle) * modDepthSamples;
            const float modOffsetR = std::cos(lfoAngle) * modDepthSamples;

            ctx.delayTimeSamplesL = juce::jlimit(2.0f, maxDelaySamples, smoothedDelaySamplesL + modOffsetL);
            ctx.delayTimeSamplesR = juce::jlimit(2.0f, maxDelaySamples, smoothedDelaySamplesR + modOffsetR);
        }
        else
        {
            ctx.delayTimeSamplesL = smoothedDelaySamplesL;
            ctx.delayTimeSamplesR = smoothedDelaySamplesR;
        }

        const float inDryL = mainL[i];
        const float inDryR = mainR != nullptr ? mainR[i] : inDryL;

        // 1. 执行当前物理引擎处理
        float wetL = 0.0f;
        float wetR = 0.0f;
        activeEngine->processStereoSample(inDryL, inDryR, wetL, wetR, ctx);

        // 2. 动态 Ducking 处理
        float scSampleL = inDryL;
        float scSampleR = inDryR;
        if (useExternalSidechain && scReadL != nullptr)
        {
            scSampleL = scReadL[i];
            scSampleR = (scReadR != nullptr) ? scReadR[i] : scSampleL;
        }

        const float duckGain = duckingDetector.processSample(
            scSampleL, scSampleR, duckEnable, duckThresh, duckAmount);

        // 湿声按避让系数衰减
        wetL *= duckGain;
        wetR *= duckGain;

        // 3. 干湿混合与总输出电平
        const float outL = (inDryL * dryGain + wetL * wetGain) * outputGainLinear;
        const float outR = (inDryR * dryGain + wetR * wetGain) * outputGainLinear;

        // 单声道输出合并两个反馈支路；保留 Ping-Pong/Cross 的完整回声，
        // 且干声只出现一次，不让右路写入覆盖左路。
        mainL[i] = mainR != nullptr ? outL
                                    : (inDryL * dryGain + 0.5f * (wetL + wetR) * wetGain) * outputGainLinear;
        if (mainR != nullptr)
            mainR[i] = outR;

        // 遥测峰值采样统计
        maxInL = std::max(maxInL, std::abs(inDryL));
        maxInR = std::max(maxInR, std::abs(inDryR));
        maxWetL = std::max(maxWetL, std::abs(wetL));
        maxWetR = std::max(maxWetR, std::abs(wetR));
        maxOutL = std::max(maxOutL, std::abs(outL));
        maxOutR = std::max(maxOutR, std::abs(outR));
    }

    // 块结束汇报 Ducking GR
    duckingDetector.finishBlock();

    // 更新无锁原子电平读数 (dBFS)
    auto toDb = [](float linear) noexcept -> float
    {
        return linear > 1e-5f ? 20.0f * std::log10(linear) : -100.0f;
    };
    inputPeakL.store(toDb(maxInL), std::memory_order_relaxed);
    inputPeakR.store(toDb(maxInR), std::memory_order_relaxed);
    outputPeakL.store(toDb(maxOutL), std::memory_order_relaxed);
    outputPeakR.store(toDb(maxOutR), std::memory_order_relaxed);
    wetPeakL.store(toDb(maxWetL), std::memory_order_relaxed);
    wetPeakR.store(toDb(maxWetR), std::memory_order_relaxed);
}

} // namespace dsp
