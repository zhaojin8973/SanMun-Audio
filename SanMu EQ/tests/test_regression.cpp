// ==============================================================================
// test_regression.cpp - M0 基线与缺陷修复（R01~R08）与 226 参数布局契约验收测试
// ==============================================================================

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "dsp/EqProcessor.h"
#include "params/Parameters.h"
#include "presets/PresetManager.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

#define TEST_ASSERT(expr, msg) \
    do { \
        if (!(expr)) { \
            std::cerr << "FAIL: " << msg << " (" << #expr << ") at " << __FILE__ << ":" << __LINE__ << std::endl; \
            return false; \
        } \
    } while (false)

// 单声道轨必须能协商主总线，并在真实处理器路径中输出音频。
static bool testMonoHostLayout()
{
    SanMuQ6AudioProcessor processor;
    auto mono = processor.getBusesLayout();
    mono.inputBuses.set(0, juce::AudioChannelSet::mono());
    mono.outputBuses.set(0, juce::AudioChannelSet::mono());
    TEST_ASSERT(processor.checkBusesLayoutSupported(mono), "Mono to mono must be available to hosts");
    TEST_ASSERT(processor.setBusesLayout(mono), "Host must be able to activate mono buses");

    auto mismatch = mono;
    mismatch.outputBuses.set(0, juce::AudioChannelSet::stereo());
    TEST_ASSERT(!processor.checkBusesLayoutSupported(mismatch), "EQ must reject mono to stereo");
    auto stereo = mismatch;
    stereo.inputBuses.set(0, juce::AudioChannelSet::stereo());
    TEST_ASSERT(processor.checkBusesLayoutSupported(stereo), "Original stereo layout must remain supported");

    processor.prepareToPlay(48000.0, 256);
    juce::AudioBuffer<float> audio(1, 256);
    juce::MidiBuffer midi;
    double energy = 0.0;
    for (int block = 0; block < 8; ++block)
    {
        for (int i = 0; i < 256; ++i)
            audio.setSample(0, i, 0.2f * std::sin(2.0f * juce::MathConstants<float>::pi * 1000.0f * (block * 256 + i) / 48000.0f));
        processor.processBlock(audio, midi);
        for (int i = 0; i < 256; ++i)
        {
            const float sample = audio.getSample(0, i);
            TEST_ASSERT(std::isfinite(sample), "Mono output must stay finite");
            energy += static_cast<double>(sample) * sample;
        }
    }
    TEST_ASSERT(energy > 0.001, "Mono EQ must produce audible output");
    processor.releaseResources();
    std::cout << "  [PASS] Mono EQ layout negotiation and audio path." << std::endl;
    return true;
}

// ------------------------------------------------------------------------------
// 测试 1：参数兼容性契约（前 58 项完全一致 + 扩至 226 参数）
// ------------------------------------------------------------------------------
// 固定正弦与阶跃用于区分向上、向下和温和 AUTO，不以界面读数代替音频验证。
static bool testDynamicDirections()
{
    for (double rate : {48000.0, 96000.0, 192000.0})
    {
        DynamicBand band; band.prepare(rate);
        const auto run = [&](float amplitude, double seconds) {
            double energy = 0; const int count = int(rate * seconds);
            for (int i = 0; i < count; ++i) {
                float left = amplitude * std::sin(2.0 * juce::MathConstants<double>::pi * 1000 * i / rate), right = left;
                band.process(left, right); if (!std::isfinite(left) || !std::isfinite(right)) return -1.0;
                if (i > count / 2) energy += double(left) * left;
            }
            return energy;
        };
        band.configure(0, 1000, 1, true, 6, -24, false, false);
        TEST_ASSERT(run(.3f, .6) > 0 && band.getReduction() > 4, "手动向下压缩应衰减强信号");
        band.reset(); band.configure(0, 1000, 1, true, 6, -24, false, true);
        const auto upward = run(.008f, .6);
        TEST_ASSERT(upward > .000032 * rate * .3 * 2.0 && band.getReduction() < -4, "向上压缩必须实际提升低于阈值的音频");
        TEST_ASSERT(run(.3f, 1.2) > 0 && std::abs(band.getReduction()) < .1f, "向上压缩不能继续提升高于阈值的信号");
        TEST_ASSERT(run(0, 3) >= 0 && std::abs(band.getReduction()) < .001f, "静音必须释放到零增益变化");
        band.reset(); band.configure(0, 1000, 1, true, 6, -24, true, false);
        TEST_ASSERT(run(.2f, 3) > 0 && band.isThresholdReady() && band.getReduction() < .7f, "AUTO 稳态应保持轻微压缩");
        TEST_ASSERT(run(.9f, .2) > 0 && band.getReduction() <= 2.101f, "AUTO 瞬态应遵守温和增益上限");
        band.configure(0, 1001, 1.01f, true, 6, -24, true, false);
        run(.2f, .001);
        TEST_ASSERT(band.isThresholdReady(), "扫频不能反复清空自动阈值学习状态");
    }
    std::cout << "通过：动态方向、真实增益、AUTO 上限与静音保护" << std::endl;
    return true;
}

static bool testUpwardAudioPaths()
{
    for (int mode = 0; mode <= 3; ++mode) {
        EqProcessor eq; constexpr double rate = 48000; constexpr int size = 512;
        eq.prepare({rate, size, 2});
        if (mode > 0) { eq.setLinearQuality(mode - 1); eq.setProcessingMode(1); }
        for (int band = 0; band < 24; ++band)
            eq.updateBand(band, 0, band == 0, 1500, 0, 1, false, 0, 0, rate, band == 0, band == 0, 6, -18, false, true);
        juce::AudioBuffer<float> audio(2, size);
        const int end = eq.getLatencySamples() + 96000; double input = 0, output = 0;
        for (int offset = 0; offset < end; offset += size) {
            for (int i = 0; i < size; ++i) {
                const float x = .02f * std::sin(2 * juce::MathConstants<double>::pi * 1500 * (offset+i) / rate);
                audio.setSample(0, i, x); audio.setSample(1, i, x);
                if (offset > end - 24000) input += x*x;
            }
            eq.process(audio);
            for (int i = 0; i < size; ++i) {
                TEST_ASSERT(std::isfinite(audio.getSample(0,i)), "向上动态处理输出必须有限");
                if (offset > end - 24000) output += double(audio.getSample(0,i))*audio.getSample(0,i);
            }
        }
        const double gain = 10 * std::log10(output/input);
        std::cout << "向上处理路径 " << mode << " 实测增益 " << gain << " dB，反馈 " << eq.getAppliedDynamicReduction(0) << std::endl;
        TEST_ASSERT(gain > 5.3 && gain < 6.5 && eq.getAppliedDynamicReduction(0) < -5.5f, "四种相位精度路径必须实际提升音频并正确反馈");
    }
    return true;
}

static bool testParameterContract()
{
    std::cout << "[RUN] Testing Parameter Layout Contract (226 params, first 58 identical)..." << std::endl;

    SanMuQ6AudioProcessor proc;
    const auto& params = proc.getParameters();

    TEST_ASSERT(params.size() == 347, "向上压缩追加在已有 323 项之后");

    auto getParamID = [&](int index) -> juce::String {
        if (auto* p = dynamic_cast<juce::AudioProcessorParameterWithID*>(params[index]))
            return p->paramID;
        return {};
    };

    for (int n = 1; n <= 24; ++n) {
        TEST_ASSERT(getParamID(298+n) == Params::IDs::bandDynAuto(n), "原 AUTO 参数编号不能变化");
        TEST_ASSERT(getParamID(322+n) == Params::IDs::bandDynUpward(n), "向上压缩必须追加在末尾");
    }
    // 检查前 58 项
    // 6 bands * 8 params = 48
    int idx = 0;
    for (int b = 1; b <= 6; ++b)
    {
        TEST_ASSERT(getParamID(idx++) == Params::IDs::bandEnabled(b), "Param mismatch");
        TEST_ASSERT(getParamID(idx++) == Params::IDs::bandType(b), "Param mismatch");
        TEST_ASSERT(getParamID(idx++) == Params::IDs::bandFreq(b), "Param mismatch");
        TEST_ASSERT(getParamID(idx++) == Params::IDs::bandGain(b), "Param mismatch");
        TEST_ASSERT(getParamID(idx++) == Params::IDs::bandQ(b), "Param mismatch");
        TEST_ASSERT(getParamID(idx++) == Params::IDs::bandSolo(b), "Param mismatch");
        TEST_ASSERT(getParamID(idx++) == Params::IDs::bandSlope(b), "Param mismatch");
        TEST_ASSERT(getParamID(idx++) == Params::IDs::bandPlacement(b), "Param mismatch");
    }
    TEST_ASSERT(idx == 48, "First 48 must be band 1..6 params");

    TEST_ASSERT(getParamID(idx++) == Params::IDs::INPUT_GAIN_L, "Param 49 must be INPUT_GAIN_L");
    TEST_ASSERT(getParamID(idx++) == Params::IDs::INPUT_GAIN_R, "Param 50 must be INPUT_GAIN_R");
    TEST_ASSERT(getParamID(idx++) == Params::IDs::OUTPUT_GAIN_L, "Param 51 must be OUTPUT_GAIN_L");
    TEST_ASSERT(getParamID(idx++) == Params::IDs::OUTPUT_GAIN_R, "Param 52 must be OUTPUT_GAIN_R");
    TEST_ASSERT(getParamID(idx++) == Params::IDs::PHASE_INVERT, "Param 53 must be PHASE_INVERT");
    TEST_ASSERT(getParamID(idx++) == Params::IDs::EQ_MODE, "Param 54 must be EQ_MODE");
    TEST_ASSERT(getParamID(idx++) == Params::IDs::THEME, "Param 55 must be THEME");
    TEST_ASSERT(getParamID(idx++) == Params::IDs::GAIN_RANGE, "Param 56 must be GAIN_RANGE");
    TEST_ASSERT(getParamID(idx++) == Params::IDs::GLOBAL_BYPASS, "Param 57 must be GLOBAL_BYPASS");
    TEST_ASSERT(getParamID(idx++) == Params::IDs::AUTO_GAIN, "Param 58 must be AUTO_GAIN");
    TEST_ASSERT(idx == 58, "First 58 parameter contract satisfied");

    // 检查第 59~202 项（频段 7~24）
    for (int b = 7; b <= 24; ++b)
    {
        TEST_ASSERT(getParamID(idx++) == Params::IDs::bandEnabled(b), "Param mismatch in bands 7..24");
        TEST_ASSERT(getParamID(idx++) == Params::IDs::bandType(b), "Param mismatch in bands 7..24");
        TEST_ASSERT(getParamID(idx++) == Params::IDs::bandFreq(b), "Param mismatch in bands 7..24");
        TEST_ASSERT(getParamID(idx++) == Params::IDs::bandGain(b), "Param mismatch in bands 7..24");
        TEST_ASSERT(getParamID(idx++) == Params::IDs::bandQ(b), "Param mismatch in bands 7..24");
        TEST_ASSERT(getParamID(idx++) == Params::IDs::bandSolo(b), "Param mismatch in bands 7..24");
        TEST_ASSERT(getParamID(idx++) == Params::IDs::bandSlope(b), "Param mismatch in bands 7..24");
        TEST_ASSERT(getParamID(idx++) == Params::IDs::bandPlacement(b), "Param mismatch in bands 7..24");
    }
    TEST_ASSERT(idx == 202, "Bands 7..24 total 144 params");

    // 检查最后 24 项（band1_present ~ band24_present）
    for (int b = 1; b <= 24; ++b)
    {
        TEST_ASSERT(getParamID(idx++) == Params::IDs::bandPresent(b), "Param mismatch in present flags");
    }
    TEST_ASSERT(idx == 226, "All 226 params verified");
    TEST_ASSERT(getParamID(idx++) == Params::IDs::CHARACTER_MODE, "Character mode must be appended after legacy parameters");
    for (int n = 1; n <= Params::MAX_BANDS; ++n)
    {
        TEST_ASSERT(getParamID(idx++) == Params::IDs::bandDynamic(n), "动态开关编号改变");
        TEST_ASSERT(getParamID(idx++) == Params::IDs::bandDynRange(n), "动态范围编号改变");
        TEST_ASSERT(getParamID(idx++) == Params::IDs::bandDynThreshold(n), "手动阈值编号改变");
    }
    for (int n = 1; n <= Params::MAX_BANDS; ++n) TEST_ASSERT(getParamID(idx++) == Params::IDs::bandDynAuto(n), "自动阈值未按序追加");

    std::cout << "  [PASS] Parameter layout verified successfully." << std::endl;
    return true;
}

// ------------------------------------------------------------------------------
// 测试 2：R01 线性相位声道路由（PlacementLeft 仅影响左声道，右声道不变）
// ------------------------------------------------------------------------------
static bool testR01LinearPhaseStereoRouting()
{
    std::cout << "[RUN] Testing R01 (Linear Phase Stereo Placement Routing)..." << std::endl;

    EqProcessor eq;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = 48000.0;
    spec.maximumBlockSize = 512;
    spec.numChannels = 2;
    eq.prepare(spec);

    // 设置模式为 Linear Phase
    eq.setProcessingMode(1);

    // 关闭所有频段，只开 Band 0：1000 Hz, +6.0 dB, Q=1.0, PlacementLeft
    for (int i = 0; i < Params::NUM_BANDS; ++i)
    {
        eq.updateBand(i, Params::Peak, (i == 0), 1000.0f, (i == 0 ? 6.0f : 0.0f), 1.0f,
                      false, 0, (i == 0 ? Params::PlacementLeft : Params::PlacementStereo),
                      spec.sampleRate, (i == 0));
    }

    // 喂入立体声测试信号（1 kHz 正弦波，幅值 0.1，两声道完全一致）
    constexpr int blockSize = 512;
    constexpr int numBlocks = 30; // 稳定通过延迟线并收敛

    double sumSqL = 0.0;
    double sumSqR = 0.0;
    int measuredSamples = 0;

    for (int b = 0; b < numBlocks; ++b)
    {
        juce::AudioBuffer<float> buf(2, blockSize);
        for (int s = 0; s < blockSize; ++s)
        {
            const double t = static_cast<double>(b * blockSize + s) / spec.sampleRate;
            const float v = 0.1f * static_cast<float>(std::sin(2.0 * juce::MathConstants<double>::pi * 1000.0 * t));
            buf.setSample(0, s, v);
            buf.setSample(1, s, v);
        }

        eq.process(buf);

        // 延迟为 5120 采样（约 10 个 block），从第 15 个 block 开始统计稳定输出
        if (b >= 15)
        {
            for (int s = 0; s < blockSize; ++s)
            {
                sumSqL += buf.getSample(0, s) * buf.getSample(0, s);
                sumSqR += buf.getSample(1, s) * buf.getSample(1, s);
                ++measuredSamples;
            }
        }
    }

    const float rmsIn = 0.1f / std::sqrt(2.0f);
    const float rmsL = static_cast<float>(std::sqrt(sumSqL / measuredSamples));
    const float rmsR = static_cast<float>(std::sqrt(sumSqR / measuredSamples));

    const float gainDbL = 20.0f * std::log10(rmsL / rmsIn);
    const float gainDbR = 20.0f * std::log10(rmsR / rmsIn);

    std::cout << "  Left Gain: " << gainDbL << " dB (expected ~+6.0 dB), Right Gain: "
              << gainDbR << " dB (expected ~0.0 dB)" << std::endl;

    TEST_ASSERT(std::abs(gainDbL - 6.0f) < 0.6f, "Linear phase Left channel should have ~+6.0 dB boost");
    TEST_ASSERT(std::abs(gainDbR - 0.0f) < 0.3f, "Linear phase Right channel must remain ~0.0 dB untouched");

    std::cout << "  [PASS] R01 verified: Left channel boosted +6dB, Right channel untouched." << std::endl;
    return true;
}

// ------------------------------------------------------------------------------
// 测试 3：R02 线性相位旁通延迟一致性与无梳状滤波
// ------------------------------------------------------------------------------
static bool testR02LinearPhaseBypassLatency()
{
    std::cout << "[RUN] Testing R02 (Linear Phase Bypass Latency Consistency)..." << std::endl;

    EqProcessor eq;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = 48000.0;
    spec.maximumBlockSize = 512;
    spec.numChannels = 2;
    eq.prepare(spec);

    eq.setProcessingMode(1); // Linear Phase
    TEST_ASSERT(eq.getLatencySamples() == LinearPhaseEq::LATENCY, "Reported latency must be 5120 samples");

    // 1. 测试未旁通时的脉冲响应峰值位置
    constexpr int totalSamples = 8192;
    juce::AudioBuffer<float> pulseBuf(2, totalSamples);
    pulseBuf.clear();
    pulseBuf.setSample(0, 0, 1.0f); // t=0 单脉冲
    pulseBuf.setSample(1, 0, 1.0f);

    constexpr int block = 512;
    int normalPeakSample = -1;
    float maxNormal = 0.0f;

    for (int offset = 0; offset < totalSamples; offset += block)
    {
        juce::AudioBuffer<float> b(pulseBuf.getArrayOfWritePointers(), 2, offset, block);
        eq.process(b);
    }

    for (int s = 0; s < totalSamples; ++s)
    {
        float val = std::abs(pulseBuf.getSample(0, s));
        if (val > maxNormal)
        {
            maxNormal = val;
            normalPeakSample = s;
        }
    }

    std::cout << "  Normal Linear Phase Peak Sample: " << normalPeakSample << " (expected 5120)" << std::endl;
    TEST_ASSERT(normalPeakSample == LinearPhaseEq::LATENCY, "Normal peak must be at exactly 5120 samples");

    // 2. 测试完全旁通时的脉冲响应峰值位置
    eq.reset();
    eq.setGlobalBypass(true);

    pulseBuf.clear();
    pulseBuf.setSample(0, 0, 1.0f);
    pulseBuf.setSample(1, 0, 1.0f);

    int bypassPeakSample = -1;
    float maxBypass = 0.0f;

    for (int offset = 0; offset < totalSamples; offset += block)
    {
        juce::AudioBuffer<float> b(pulseBuf.getArrayOfWritePointers(), 2, offset, block);
        eq.process(b);
    }

    for (int s = 0; s < totalSamples; ++s)
    {
        float val = std::abs(pulseBuf.getSample(0, s));
        if (val > maxBypass)
        {
            maxBypass = val;
            bypassPeakSample = s;
        }
    }

    std::cout << "  Bypassed Linear Phase Peak Sample: " << bypassPeakSample << " (expected 5120)" << std::endl;
    TEST_ASSERT(bypassPeakSample == LinearPhaseEq::LATENCY, "Bypassed peak must be at exactly 5120 samples");

    std::cout << "  [PASS] R02 verified: Latency strictly preserved across bypass transitions." << std::endl;
    return true;
}

// ------------------------------------------------------------------------------
// 测试 4：R03 频谱反相抵消消除（频域立体声功率合成）
// ------------------------------------------------------------------------------
static bool testR03SpectrumPhaseCancellation()
{
    std::cout << "[RUN] Testing R03 (Spectrum Out-of-Phase Cancellation Immunity)..." << std::endl;

    EqProcessor eq;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = 48000.0;
    spec.maximumBlockSize = 512;
    spec.numChannels = 2;
    eq.prepare(spec);

    // 喂入 180° 反相信号：L = +sin, R = -sin（1000 Hz, -20 dBFS 峰值即 0.1f）
    constexpr int blockSize = 512;
    for (int b = 0; b < 40; ++b)
    {
        juce::AudioBuffer<float> buf(2, blockSize);
        for (int s = 0; s < blockSize; ++s)
        {
            const double t = static_cast<double>(b * blockSize + s) / spec.sampleRate;
            const float v = 0.1f * static_cast<float>(std::sin(2.0 * juce::MathConstants<double>::pi * 1000.0 * t));
            buf.setSample(0, s, v);
            buf.setSample(1, s, -v); // 180° 反相
        }
        eq.process(buf);
    }

    TEST_ASSERT(eq.isSpectrumReady(), "Spectrum must be ready");

    // 查找 1000 Hz 附近的频谱最大读数
    float maxDb = -100.0f;
    for (int i = 0; i < EqProcessor::SPECTRUM_POINTS; ++i)
    {
        maxDb = std::max(maxDb, eq.getSpectrumPoint(i));
    }

    std::cout << "  Out-of-phase Measured Peak dBFS: " << maxDb << " (expected ~-20 dBFS, old code had -90 dBFS)" << std::endl;
    TEST_ASSERT(maxDb > -26.0f, "Spectrum must reflect actual acoustic energy even with 180 out-of-phase signals");

    std::cout << "  [PASS] R03 verified: No phase cancellation in spectrum analysis." << std::endl;
    return true;
}

// ------------------------------------------------------------------------------
// 测试 5：R04 频谱绝对标定精度（-20 dBFS 整 bin 正弦应准确显示 -20 dBFS）
// ------------------------------------------------------------------------------
static bool testR04SpectrumCalibration()
{
    std::cout << "[RUN] Testing R04 (Spectrum Absolute dBFS Calibration Accuracy)..." << std::endl;

    EqProcessor eq;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = 44100.0;
    spec.maximumBlockSize = 512;
    spec.numChannels = 2;
    eq.prepare(spec);

    // 44100 / 8192 = 5.3833 Hz per bin.
    // bin 209: 209 * 44100 / 8192 = 1125.1068 Hz.
    const double testFreq = 209.0 * 44100.0 / 8192.0;
    const float testAmp = 0.1f; // -20 dBFS 峰值

    constexpr int blockSize = 512;
    for (int b = 0; b < 50; ++b)
    {
        juce::AudioBuffer<float> buf(2, blockSize);
        for (int s = 0; s < blockSize; ++s)
        {
            const double t = static_cast<double>(b * blockSize + s) / spec.sampleRate;
            const float v = testAmp * static_cast<float>(std::cos(2.0 * juce::MathConstants<double>::pi * testFreq * t));
            buf.setSample(0, s, v);
            buf.setSample(1, s, v);
        }
        eq.process(buf);
    }

    TEST_ASSERT(eq.isSpectrumReady(), "Spectrum must be ready");

    float maxDb = -100.0f;
    for (int i = 0; i < EqProcessor::SPECTRUM_POINTS; ++i)
    {
        maxDb = std::max(maxDb, eq.getSpectrumPeakPoint(i));
    }

    std::cout << "  Calibrated -20 dBFS Sine Peak Display: " << maxDb << " dBFS (expected ~-20.0 dBFS, old had -11.1 dBFS)" << std::endl;
    TEST_ASSERT(std::abs(maxDb - (-20.0f)) < 1.0f, "Spectrum peak reading must be calibrated to -20.0 dBFS (+-1.0 dB)");

    std::cout << "  [PASS] R04 verified: Absolute spectrum calibration is exact." << std::endl;
    return true;
}

// ------------------------------------------------------------------------------
// 测试 6：R05 关闭频段的 Solo 不导致全局静音
// ------------------------------------------------------------------------------
static bool testR05SoloDisabledBand()
{
    std::cout << "[RUN] Testing R05 (Solo on Disabled/Absent Band)..." << std::endl;

    EqProcessor eq;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = 48000.0;
    spec.maximumBlockSize = 512;
    spec.numChannels = 2;
    eq.prepare(spec);

    // Band 0: solo=true, 但 enabled=false
    eq.updateBand(0, Params::Peak, false, 1000.0f, 0.0f, 1.0f, true, 0, 0, spec.sampleRate, true);

    TEST_ASSERT(!eq.isAnyBandSoloed(), "Disabled band with solo=true must NOT trigger isAnyBandSoloed()");

    juce::AudioBuffer<float> buf(2, 512);
    for (int s = 0; s < 512; ++s)
    {
        buf.setSample(0, s, 0.5f);
        buf.setSample(1, s, 0.5f);
    }

    eq.process(buf);

    float maxVal = 0.0f;
    for (int s = 0; s < 512; ++s)
        maxVal = std::max(maxVal, std::abs(buf.getSample(0, s)));

    std::cout << "  Output Max Amplitude: " << maxVal << " (expected 0.5, not 0.0)" << std::endl;
    TEST_ASSERT(maxVal > 0.45f, "Disabled band with solo must not silence audio output");

    std::cout << "  [PASS] R05 verified: Disabled solo band does not cause unwanted muting." << std::endl;
    return true;
}

// ------------------------------------------------------------------------------
// 测试 7：R07 预设数据完整性与工厂预设完整重置
// ------------------------------------------------------------------------------
static bool testR07PresetDataIntegrity()
{
    std::cout << "[RUN] Testing R07 (PresetData Sound Snapshot Completeness)..." << std::endl;

    SanMuQ6AudioProcessor proc;

    // 先故意改动 slope 和 placement
    auto* slopeParam = proc.apvts.getParameter(Params::IDs::bandSlope(1));
    if (slopeParam) slopeParam->setValueNotifyingHost(slopeParam->convertTo0to1(3.0f)); // 48 dB/oct
    auto* placeParam = proc.apvts.getParameter(Params::IDs::bandPlacement(1));
    if (placeParam) placeParam->setValueNotifyingHost(placeParam->convertTo0to1(4.0f)); // Side

    // 加载默认工厂预设
    const auto& defaultPreset = PresetManager::getFactoryPreset(0);
    PresetManager::applyPreset(defaultPreset, proc.apvts);

    // 检查恢复后的状态
    auto slopeVal = proc.apvts.getRawParameterValue(Params::IDs::bandSlope(1))->load();
    auto placeVal = proc.apvts.getRawParameterValue(Params::IDs::bandPlacement(1))->load();
    auto present1 = proc.apvts.getRawParameterValue(Params::IDs::bandPresent(1))->load();
    auto present7 = proc.apvts.getRawParameterValue(Params::IDs::bandPresent(7))->load();

    TEST_ASSERT(slopeVal == 0.0f, "Default factory preset must reset slope to 12 dB/oct (0)");
    TEST_ASSERT(placeVal == 0.0f, "Default factory preset must reset placement to Stereo (0)");
    TEST_ASSERT(present1 > 0.5f, "Band 1 present must be true in default preset");
    TEST_ASSERT(present7 < 0.5f, "Band 7 present must be false in default preset");

    std::cout << "  [PASS] R07 verified: Factory preset completely defines full sound state." << std::endl;
    return true;
}

// ------------------------------------------------------------------------------
// 测试 8：R08 宿主状态版本化与旧版 6 频段自动平滑迁移
// ------------------------------------------------------------------------------
static bool testR08StateVersionMigration()
{
    std::cout << "[RUN] Testing R08 (State Schema Versioning & Legacy 6-Band Migration)..." << std::endl;

    SanMuQ6AudioProcessor proc;

    // 1. 验证新状态保存包含 schemaVersion="1.1"
    juce::MemoryBlock destData;
    proc.getStateInformation(destData);
    std::unique_ptr<juce::XmlElement> savedXml(juce::AudioProcessor::getXmlFromBinary(destData.getData(), (int)destData.getSize()));
    TEST_ASSERT(savedXml != nullptr, "Saved state XML must be valid");
    TEST_ASSERT(savedXml->getStringAttribute("schemaVersion") == "1.1", "Saved state must have schemaVersion='1.1'");

    // 2. 模拟旧版本（6 频段，无 schemaVersion，无 band*_present）
    juce::XmlElement legacyXml("SanMuQ6Parameters");
    for (int b = 1; b <= 6; ++b)
    {
        legacyXml.setAttribute(Params::IDs::bandEnabled(b), 1.0f);
        legacyXml.setAttribute(Params::IDs::bandFreq(b), 1000.0f);
        legacyXml.setAttribute(Params::IDs::bandGain(b), 0.0f);
        legacyXml.setAttribute(Params::IDs::bandQ(b), 1.0f);
    }
    // 转为二进制并灌入 setStateInformation
    juce::MemoryBlock legacyData;
    juce::AudioProcessor::copyXmlToBinary(legacyXml, legacyData);

    proc.setStateInformation(legacyData.getData(), (int)legacyData.getSize());

    // 验证迁移结果：
    // band 1..6 present 应当自动补全为 1.0f
    for (int b = 1; b <= 6; ++b)
    {
        auto* p = proc.apvts.getRawParameterValue(Params::IDs::bandPresent(b));
        TEST_ASSERT(p != nullptr && p->load() > 0.5f, "Legacy bands 1..6 must migrate present=true");
    }
    // band 7..24 present 应当自动初始化为 0.0f
    for (int b = 7; b <= 24; ++b)
    {
        auto* p = proc.apvts.getRawParameterValue(Params::IDs::bandPresent(b));
        TEST_ASSERT(p != nullptr && p->load() < 0.5f, "New bands 7..24 must migrate present=false");
    }

    std::cout << "  [PASS] R08 verified: Legacy 6-band states seamlessly migrate to 24-slot architecture." << std::endl;
    return true;
}

static bool testDynamicBandResponse()
{
    std::cout << "[RUN] Testing dynamic band detector and reduction meter..." << std::endl;
    EqProcessor eq; juce::dsp::ProcessSpec spec { 48000.0, 256, 2 }; eq.prepare(spec);
    for (int i = 0; i < Params::NUM_BANDS; ++i)
        eq.updateBand(i, Params::Peak, i == 0, 1000.0f, i == 0 ? 12.0f : 0.0f, 1.0f, false, 0, 0, spec.sampleRate, i == 0, i == 0, 6.0f, -30.0f);
    for (int block = 0; block < 40; ++block)
    {
        juce::AudioBuffer<float> audio(2, 256);
        for (int s = 0; s < 256; ++s) { const float v = .5f * std::sin(2.0f * juce::MathConstants<float>::pi * 1000.0f * (block * 256 + s) / 48000.0f); audio.setSample(0, s, v); audio.setSample(1, s, v); }
        eq.process(audio);
    }
    TEST_ASSERT(eq.getBand(0).getDynamicReduction() > 0.1f, "On-band signal must produce measurable dynamic reduction");
    std::cout << "  [PASS] Dynamic reduction meter responds to on-band signal." << std::endl;
    return true;
}

#include "test_eq_upgrades.h"
#include "test_auto_threshold.h"
#include "test_final_audit.h"
#include "test_linear_rebuild.h"
#include "test_mode_integrity.h"

int main(int argc, char** argv)
{
    if (argc > 1 && juce::String(argv[1]) == "--benchmark-linear") return testLinearPhaseRealtimeBudget() ? 0 : 1;
    if (argc > 1 && juce::String(argv[1]) == "--benchmark-linear-multi") return measureLinearMultiInstance() ? 0 : 1;
    if (argc > 1 && juce::String(argv[1]) == "--measure-linear-low") return measureLinearLowFrequency() ? 0 : 1;
    if (argc > 1 && juce::String(argv[1]) == "--check-modes")
    {
        bool passed = testMinimumPhaseTimeline(); passed = testMinimumPhaseReprepare() && passed;
        passed = testAutoGainPlacementSymmetry() && passed; return passed ? 0 : 1;
    }
    std::cout << "==================================================================" << std::endl;
    std::cout << "Starting SanMu EQ M0 & Parameter Contract Regression Tests" << std::endl;
    std::cout << "==================================================================" << std::endl;

    if (!testMonoHostLayout()) return 1;
    bool auditPassed = testUnchangedBandAndInitialSmoothing();
    auditPassed = testMonoDynamicProcessing() && auditPassed;
    auditPassed = testLinearPhaseMixedPlacements() && auditPassed;
    auditPassed = testLinearPhaseDynamicProcessing() && auditPassed;
    auditPassed = testLinearAtomicKernel() && auditPassed;
    auditPassed = testLinearTimelineAndHistory() && auditPassed;
    auditPassed = testLinearStateCompatibility() && auditPassed;
    auditPassed = testLinearShapesAndRates() && auditPassed;
    auditPassed = testMinimumPhaseTimeline() && auditPassed;
    auditPassed = testMinimumPhaseReprepare() && auditPassed;
    auditPassed = testAutoGainPlacementSymmetry() && auditPassed;
    if (!auditPassed) return 1;
    if (!(testDynamicDirections() && testUpwardAudioPaths() && testParameterContract())) return 1;
    if (!testR01LinearPhaseStereoRouting()) return 1;
    if (!testR02LinearPhaseBypassLatency()) return 1;
    if (!testR03SpectrumPhaseCancellation()) return 1;
    if (!testR04SpectrumCalibration()) return 1;
    if (!testR05SoloDisabledBand()) return 1;
    if (!testR07PresetDataIntegrity()) return 1;
    if (!testR08StateVersionMigration()) return 1;
    if (!testDynamicBandResponse()) return 1;
    if (!testDynamicSelectivity()) return 1;
    if (!testCharacterAndUpgradeState()) return 1;
    if (!testSpectrumUpgrade()) return 1;
    if (!testSpectrumResolution()) return 1;
    if (!testAuditionAndCharacterLevels()) return 1;
    if (!testAutoThreshold()) return 1;

    std::cout << "==================================================================" << std::endl;
    std::cout << "ALL REGRESSION TESTS PASSED (0 FAILURES, EFFECTIVE ASSERTIONS)" << std::endl;
    std::cout << "==================================================================" << std::endl;
    return 0;
}
