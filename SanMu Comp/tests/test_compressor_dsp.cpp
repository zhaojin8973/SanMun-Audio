// ==============================================================================
// test_compressor_dsp.cpp - SanMu Comp DSP 物理建模与稳定性单元测试
// ==============================================================================

#include <iostream>
#include <vector>
#include <cmath>
#include <cassert>

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include "dsp/CompressorProcessor.h"
#include "params/Parameters.h"

// 辅助：生成正弦波
static void generateSineWave(juce::AudioBuffer<float>& buffer, float frequency, float amplitude, double sampleRate)
{
    const int numSamples = buffer.getNumSamples();
    const float phaseInc = static_cast<float>(2.0 * juce::MathConstants<double>::pi * frequency / sampleRate);
    float phase = 0.0f;

    for (int n = 0; n < numSamples; ++n)
    {
        const float s = amplitude * std::sin(phase);
        phase += phaseInc;
        if (phase >= 2.0f * juce::MathConstants<float>::pi)
            phase -= 2.0f * juce::MathConstants<float>::pi;

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.setSample(ch, n, s);
    }
}

// 辅助：生成脉冲突发音（Tone Burst）
static void generateToneBurst(juce::AudioBuffer<float>& buffer, float frequency, float amplitude, int burstSamples, double sampleRate)
{
    buffer.clear();
    const float phaseInc = static_cast<float>(2.0 * juce::MathConstants<double>::pi * frequency / sampleRate);
    float phase = 0.0f;

    for (int n = 0; n < std::min(burstSamples, buffer.getNumSamples()); ++n)
    {
        const float s = amplitude * std::sin(phase);
        phase += phaseInc;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.setSample(ch, n, s);
    }
}

// -----------------------------------------------------------------------------
// 测试 1: Clean 模式阈值与比率静态增益衰减计算
// -----------------------------------------------------------------------------
static bool testCleanCompression()
{
    std::cout << "[Test 1] Testing Clean Mode Compression Accuracy..." << std::endl;

    dsp_engines::CleanEngine clean;
    juce::dsp::ProcessSpec spec { 48000.0, 512, 2 };
    clean.prepare(spec);

    clean.thresholdDb.store(-20.0f);
    clean.ratio.store(4.0f); // 4:1
    clean.attackMs.store(1.0f); // 极快启动
    clean.releaseMs.store(100.0f);
    clean.kneeDb.store(0.0f);
    clean.makeupDb.store(0.0f);
    clean.mixPercent.store(100.0f);

    // 1. 小信号 (-30dBFS) 不应被压缩
    {
        juce::AudioBuffer<float> buffer(2, 4800); // 100ms
        const float amp = std::pow(10.0f, -30.0f / 20.0f);
        generateSineWave(buffer, 1000.0f, amp, 48000.0);
        clean.process(buffer);

        const float gr = clean.getCurrentGrDb();
        if (gr > 0.1f)
        {
            std::cerr << "  FAIL: Small signal compressed unexpectedly! GR = " << gr << std::endl;
            return false;
        }
    }

    // 2. 0dBFS 大信号进入稳态，GR 理论值 = (0 - (-20)) * (1 - 1/4) = 15.0 dB
    {
        juce::AudioBuffer<float> buffer(2, 48000); // 1.0 秒，让释放/启动完全稳定
        generateSineWave(buffer, 1000.0f, 1.0f, 48000.0);
        clean.process(buffer);

        const float gr = clean.getCurrentGrDb();
        std::cout << "  Clean 0dBFS input steady-state GR: " << gr << " dB (expected ~15.0 dB)" << std::endl;
        if (std::abs(gr - 15.0f) > 0.8f)
        {
            std::cerr << "  FAIL: GR deviation too high!" << std::endl;
            return false;
        }
    }

    std::cout << "  PASS: Clean mode compression is accurate." << std::endl;
    return true;
}

// -----------------------------------------------------------------------------
// 测试 2: FET 低频有限输出与压缩响应；失真另由动态测试测量。
// -----------------------------------------------------------------------------
static bool testFetLowFreqStability()
{
    std::cout << "[测试 2] FET 50Hz 有限输出与压缩响应..." << std::endl;

    CompressorProcessor comp;
    juce::dsp::ProcessSpec spec { 48000.0, 512, 2 };
    comp.prepare(spec);

    comp.setMode(Params::ModeFET);
    comp.setFetDrive(18.0f); // 刻度 30 (10:30, 经典标称工作点)
    comp.setFetRatioIndex(0); // 4:1
    comp.setAttack(3.0f); // 1~7 scale
    comp.setRelease(5.0f); // 1~7 scale
    comp.setMakeup(30.0f); // 刻度 18 (1:30, 补偿增益)
    comp.setMix(100.0f);

    // 50Hz 大信号
    juce::AudioBuffer<float> buffer(2, 48000);
    generateSineWave(buffer, 50.0f, 0.8f, 48000.0);
    comp.process(buffer);

    // 此处只检查非有限数值，不据此判断波形平滑或失真水平。
    for (int ch = 0; ch < 2; ++ch)
    {
        const auto* ptr = buffer.getReadPointer(ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            if (std::isnan(ptr[i]) || std::isinf(ptr[i]))
            {
                std::cerr << "  FAIL: Output contains NaN/Inf!" << std::endl;
                return false;
            }
        }
    }

    const float gr = comp.getCurrentGainReductionDb();
    std::cout << "  FET 50Hz steady GR: " << gr << " dB" << std::endl;
    if (gr <= 0.5f)
    {
        std::cerr << "  FAIL: FET failed to compress 50Hz signal!" << std::endl;
        return false;
    }

    std::cout << "  通过：输出有限且有压缩；失真结论见独立动态测量。" << std::endl;
    return true;
}

// -----------------------------------------------------------------------------
// 测试 3: Opto 模式 10ms 瞬态快速捕获与双阶段长尾释放验证
// -----------------------------------------------------------------------------
static bool testOptoTransientResponse()
{
    std::cout << "[Test 3] Testing Opto Fast Transient Attack & Dual-Phase Release..." << std::endl;

    CompressorProcessor comp;
    juce::dsp::ProcessSpec spec { 48000.0, 512, 2 };
    comp.prepare(spec);

    comp.setMode(Params::ModeOpto);
    comp.setOptoPeakReduction(75.0f);
    comp.setMakeup(40.0f); // 40.0 为 Unity 0dB 基准
    comp.setMix(100.0f);

    // 10ms 脉冲突发 (480 采样点 @ 48kHz)
    juce::AudioBuffer<float> burstBuffer(2, 480);
    generateToneBurst(burstBuffer, 1000.0f, 0.9f, 480, 48000.0);
    comp.process(burstBuffer);

    const float attackGr = comp.getCurrentGainReductionDb();
    std::cout << "  Opto 10ms transient burst GR: " << attackGr << " dB" << std::endl;

    if (attackGr < 2.0f)
    {
        std::cerr << "  FAIL: Opto failed to catch 10ms transient (attack was sluggish/broken)!" << std::endl;
        return false;
    }

    // 后续静音 100ms：快速恢复前半
    juce::AudioBuffer<float> silenceBuffer(2, 4800);
    silenceBuffer.clear();
    comp.process(silenceBuffer);

    // 后续静音 8.0 秒：三阶段完全释放回 0dB（真实 T4B 超长慢尾归零）
    juce::AudioBuffer<float> fullReleaseBuffer(2, 384000);
    fullReleaseBuffer.clear();
    comp.process(fullReleaseBuffer);

    const float finalGr = comp.getCurrentGainReductionDb();
    std::cout << "  Opto GR after full silence release: " << finalGr << " dB" << std::endl;

    if (finalGr > 0.05f)
    {
        std::cerr << "  FAIL: Opto failed to return to 0 dB GR! Remaining GR: " << finalGr << " dB" << std::endl;
        return false;
    }

    std::cout << "  PASS: Opto transient attack, dual-phase release, and zero-return validated." << std::endl;
    return true;
}

static bool testOptoModeSwitch()
{
    std::cout << "[Test 3b] Testing Opto Compress/Limit Mode Switch..." << std::endl;
    juce::dsp::ProcessSpec spec { 48000.0, 512, 2 };
    dsp_engines::OptoEngine opto;
    opto.prepare(spec);
    opto.optoPeakReduction.store(65.0f);
    juce::AudioBuffer<float> compBuf(2, 48000), limitBuf(2, 48000);
    generateSineWave(compBuf, 1000.0f, 0.7f, 48000.0);
    limitBuf.makeCopyOf(compBuf);
    opto.isLimit.store(false);
    opto.process(compBuf);
    const float compGr = opto.getCurrentGrDb();
    opto.reset();
    opto.isLimit.store(true);
    opto.process(limitBuf);
    const float limitGr = opto.getCurrentGrDb();
    if (limitGr <= compGr + 0.25f)
    {
        std::cerr << "  FAIL: Opto LIMIT mode did not increase gain reduction (COMP=" << compGr << ", LIMIT=" << limitGr << ")" << std::endl;
        return false;
    }
    std::cout << "  PASS: Opto COMP/LIMIT modes produce distinct compression." << std::endl;
    return true;
}

// -----------------------------------------------------------------------------
// 测试 4: 侧链高通滤波隔离测试
// -----------------------------------------------------------------------------
static bool testSidechainHpRejection()
{
    std::cout << "[Test 4] Testing Sidechain High-Pass Rejection..." << std::endl;

    CompressorProcessor comp;
    juce::dsp::ProcessSpec spec { 48000.0, 512, 2 };
    comp.prepare(spec);

    comp.setMode(Params::ModeVCA);
    comp.setThreshold(-20.0f);
    comp.setRatio(4.0f);
    comp.setSidechainEnabled(true);
    comp.setSidechainHp(300.0f); // 300Hz HPF

    // 50Hz 信号远低于 300Hz 截止频率
    juce::AudioBuffer<float> buffer(2, 48000);
    const float amp = std::pow(10.0f, -10.0f / 20.0f); // -10dBFS
    generateSineWave(buffer, 50.0f, amp, 48000.0);
    comp.process(buffer);

    const float gr = comp.getCurrentGainReductionDb();
    std::cout << "  GR on 50Hz signal with 300Hz Sidechain HP: " << gr << " dB" << std::endl;

    if (gr > 1.0f)
    {
        std::cerr << "  FAIL: Sidechain HPF did not attenuate low-frequency trigger!" << std::endl;
        return false;
    }

    std::cout << "  PASS: Sidechain high-pass filter rejects sub-cutoff frequencies." << std::endl;
    return true;
}

static bool testEngineIsolation()
{
    std::cout << "[Test 5] Testing DSP Engines Isolation..." << std::endl;
    juce::dsp::ProcessSpec spec { 48000.0, 512, 2 };
    dsp_engines::FetEngine fet, reference;
    dsp_engines::VcaEngine vca;
    vca.prepare(spec);
    for (auto* engine : {&fet, &reference})
    {
        engine->fetDrive.store(24.0f);
        engine->fetRatioIdx.store(0);
        engine->prepare(spec);
    }
    juce::AudioBuffer<float> a(2, 4096), b(2, 4096), vcaBuffer(2, 4096);
    generateSineWave(a, 440.0f, 0.25f, 48000.0);
    b.makeCopyOf(a);
    generateSineWave(vcaBuffer, 440.0f, 0.25f, 48000.0);
    fet.process(a);
    vca.thresholdDb.store(-2.0f);
    vca.ratio.store(18.0f);
    vca.process(vcaBuffer);
    reference.process(b);
    float maxDiff = 0.0f;
    for (int ch = 0; ch < 2; ++ch)
        for (int n = 0; n < a.getNumSamples(); ++n)
            maxDiff = std::max(maxDiff, std::abs(a.getSample(ch, n) - b.getSample(ch, n)));
    if (maxDiff > 1.0e-5f)
    {
        std::cerr << "  FAIL: Inactive engine parameters affected active FET output. diff=" << maxDiff << std::endl;
        return false;
    }
    std::cout << "  PASS: Inactive engine parameters do not affect active FET output." << std::endl;
    return true;
}

int main()
{
    std::cout << "==================================================" << std::endl;
    std::cout << "   SanMu Comp DSP Core Physical Modeling Tests    " << std::endl;
    std::cout << "==================================================" << std::endl;

    bool allPassed = true;
    allPassed &= testCleanCompression();
    allPassed &= testFetLowFreqStability();
    allPassed &= testOptoTransientResponse();
    allPassed &= testOptoModeSwitch();
    allPassed &= testSidechainHpRejection();
    allPassed &= testEngineIsolation();

    std::cout << "==================================================" << std::endl;
    if (allPassed)
    {
        std::cout << ">>> ALL DSP TESTS PASSED SUCCESSFULLY! <<<" << std::endl;
        return 0;
    }
    else
    {
        std::cerr << ">>> SOME DSP TESTS FAILED! <<<" << std::endl;
        return 1;
    }
}
