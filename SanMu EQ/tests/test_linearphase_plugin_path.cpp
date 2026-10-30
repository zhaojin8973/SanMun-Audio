// ==============================================================================
// 临时验证测试：模拟插件完整路径（EqProcessor::process → LinearPhase 分支）
// 1. 验证 LinearPhase 有声性
// 2. 复现用户反馈：LinearPhase 下 Bell@34Hz 提升越提越衰减
// ==============================================================================

#include <juce_dsp/juce_dsp.h>
#include "dsp/EqProcessor.h"
#include "params/Parameters.h"

#include <cmath>
#include <cstdio>

// 模拟 PluginProcessor::processBlock 的参数更新循环
static void updateAllBands(EqProcessor& eq, double sr)
{
    for (int i = 0; i < Params::NUM_BANDS; ++i)
    {
        eq.updateBand(i,
                      Params::Defaults::TYPES[i],
                      true,
                      Params::Defaults::FREQUENCIES[i],
                      0.0f,
                      1.0f,
                      false,
                      0,
                      0,
                      sr);
    }
}

static void runModeTest(int mode, const char* name)
{
    EqProcessor eq;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = 48000.0;
    spec.maximumBlockSize = 512;
    spec.numChannels = 2;
    eq.prepare(spec);

    updateAllBands(eq, spec.sampleRate);
    eq.setProcessingMode(mode);

    constexpr int blockSize = 512;
    constexpr int numBlocks = 60;

    double sumSq = 0.0;
    double maxAbs = 0.0;

    for (int b = 0; b < numBlocks; ++b)
    {
        juce::AudioBuffer<float> buf(2, blockSize);
        for (int s = 0; s < blockSize; ++s)
        {
            const double t = static_cast<double>(b * blockSize + s) / spec.sampleRate;
            const float v = 0.5f * static_cast<float>(std::sin(2.0 * 3.141592653589793 * 1000.0 * t));
            buf.setSample(0, s, v);
            buf.setSample(1, s, v);
        }

        eq.process(buf);

        for (int s = 0; s < blockSize; ++s)
        {
            const float v = buf.getSample(0, s);
            sumSq += v * v;
            maxAbs = juce::jmax(maxAbs, static_cast<double>(std::abs(v)));
        }
    }

    const double rms = std::sqrt(sumSq / (numBlocks * blockSize));
    std::printf("[%s] RMS=%.4f  maxAbs=%.4f  %s\n",
                name, rms, maxAbs,
                (rms > 0.001) ? "PASS: 有声音" : "FAIL: 无声");
}

// ==============================================================================
// Bell 提升复现：单一 Bell band 于 freqHz，测 freqHz 正弦输出 RMS
// 期望：gainDb +12 的输出 ≈ 0dB 的 10^(12/20)=3.98 倍
// 额外输出与参考正弦的相位相关 cosφ（φ<90° 为正，反相时 cosφ<0）
// ==============================================================================

struct BellResult
{
    double rms = 0.0;
    double phaseCos = 0.0;   // 输出与输入（延迟补偿后）的相关度
};

static BellResult measureBellOut(double freq, float q, float gainDb, bool linearPhase)
{
    EqProcessor eq;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = 48000.0;
    spec.maximumBlockSize = 512;
    spec.numChannels = 2;
    eq.prepare(spec);

    // band0 = Bell @ freq，其余 band 关闭（避免干扰）
    for (int i = 0; i < Params::NUM_BANDS; ++i)
    {
        const bool isTarget = (i == 0);
        eq.updateBand(i, Params::Peak, isTarget,
                      isTarget ? static_cast<float>(freq) : 50.0f,
                      isTarget ? gainDb : 0.0f,
                      q, false, 0, 0, spec.sampleRate);
    }
    eq.setProcessingMode(linearPhase ? 1 : 0);

    constexpr int blockSize = 512;
    constexpr int numBlocks = 80;       // 覆盖 LATENCY + FIR 重建 + 平滑收敛
    constexpr int skipBlocks = 12;      // 跳过启动（延迟预填充 + 平滑过渡）

    double sumSq = 0.0;
    double sumInOut = 0.0;   // 输入×输出（延迟补偿后），用于相位相关
    double sumInSq = 0.0;
    int counted = 0;

    // 延迟补偿：LinearPhase 模式对齐 LATENCY=BLOCK_L+GROUP_DELAY=5120（与 blockSize 无关）
    const int latencySamples = eq.getLatencySamples();
    for (int b = 0; b < numBlocks; ++b)
    {
        juce::AudioBuffer<float> buf(2, blockSize);
        for (int s = 0; s < blockSize; ++s)
        {
            const double t = static_cast<double>(b * blockSize + s) / spec.sampleRate;
            const float v = 0.5f * static_cast<float>(std::sin(2.0 * 3.141592653589793 * freq * t));
            buf.setSample(0, s, v);
            buf.setSample(1, s, v);
        }

        eq.process(buf);

        if (b < skipBlocks)
            continue;

        // 参考输入：同一时刻延迟 latencySamples 的输入正弦（对齐输出）
        for (int s = 0; s < blockSize; ++s)
        {
            const double tRef = static_cast<double>(b * blockSize + s - latencySamples) / spec.sampleRate;
            const double ref = std::sin(2.0 * 3.141592653589793 * freq * tRef);
            const float v = buf.getSample(0, s);
            sumSq += v * v;
            sumInOut += v * ref;
            sumInSq += ref * ref;
            ++counted;
        }
    }

    BellResult r;
    r.rms = std::sqrt(sumSq / counted);
    if (sumInSq > 0.0)
        r.phaseCos = sumInOut / std::sqrt(sumSq * sumInSq);   // cos φ（0=正交，±1=同/反相）
    return r;
}

static void runBellBoostTest(double freq, float q, bool linearPhase)
{
    const char* mode = linearPhase ? "LinearPhase" : "ZeroLatency";
    const BellResult r0  = measureBellOut(freq, q, 0.0f, linearPhase);
    const BellResult r12 = measureBellOut(freq, q, 12.0f, linearPhase);
    const double ratio = r12.rms / r0.rms;
    const double expect = std::pow(10.0, 12.0 / 20.0);   // 3.98

    std::printf("[%s] Bell %.0fHz Q=%.1f: 0dB rms=%.4f cos=%.2f | +12dB rms=%.4f cos=%.2f | 比值=%.2f (期望%.2f)  %s\n",
                mode, freq, q, r0.rms, r0.phaseCos, r12.rms, r12.phaseCos, ratio, expect,
                (ratio > expect * 0.7) ? "PASS: 提升正常"
                : (ratio < 1.0) ? "FAIL: 反向（越提升越衰减）"
                : "WARN: 提升不足");
}

// ==============================================================================
// 低频 Cut 衰减方向验证：LowCut@60Hz 应衰减 40Hz 正弦（至少 -10dB），不得提升
// ==============================================================================

static double measureCutOut(double freq, bool linearPhase)
{
    EqProcessor eq;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = 48000.0;
    spec.maximumBlockSize = 512;
    spec.numChannels = 2;
    spec.maximumBlockSize = 512;
    spec.numChannels = 2;
    eq.prepare(spec);

    // band0 = LowCut@60Hz，其余关闭
    for (int i = 0; i < Params::NUM_BANDS; ++i)
    {
        const bool isTarget = (i == 0);
        eq.updateBand(i, isTarget ? Params::LowCut : Params::Peak, isTarget,
                      isTarget ? 60.0f : 50.0f,
                      0.0f, 1.0f, false, 0, 0, spec.sampleRate);   // slope 0 = 12dB/oct
    }
    eq.setProcessingMode(linearPhase ? 1 : 0);

    constexpr int blockSize = 512;
    constexpr int numBlocks = 80;
    constexpr int skipBlocks = 12;
    double sumSq = 0.0;
    int counted = 0;

    for (int b = 0; b < numBlocks; ++b)
    {
        juce::AudioBuffer<float> buf(2, blockSize);
        for (int s = 0; s < blockSize; ++s)
        {
            const double t = static_cast<double>(b * blockSize + s) / spec.sampleRate;
            const float v = 0.5f * static_cast<float>(std::sin(2.0 * 3.141592653589793 * freq * t));
            buf.setSample(0, s, v);
            buf.setSample(1, s, v);
        }
        eq.process(buf);
        if (b < skipBlocks)
            continue;
        for (int s = 0; s < blockSize; ++s)
        {
            const float v = buf.getSample(0, s);
            sumSq += v * v;
            ++counted;
        }
    }
    return std::sqrt(sumSq / counted);
}

static void runCutTest(double testFreq, bool linearPhase)
{
    const char* mode = linearPhase ? "LinearPhase" : "ZeroLatency";
    const double out = measureCutOut(testFreq, linearPhase);
    const double ref = 0.3536;   // 直通时 0.5 幅度正弦 RMS
    const double db = 20.0 * std::log10(out / ref);
    // 期望：60Hz 低切（Q=1, 12dB/oct）
    // - 200Hz 通带 ≈ 0dB（带共振约 +4dB）
    // - 100Hz 略低于 0dB
    // - 40Hz 物理 -5.8dB（Q=1 二阶高通数学解）
    // - 20Hz 深衰减（-18dB 以上）
    std::printf("[%s] LowCut@60Hz 测 %5.0fHz: RMS=%.4f (%.1f dB)\n",
                mode, testFreq, out, db);
}

// ==============================================================================
// solo 回退路径：LinearPhase + solo band0 → IIR 回退，应有声
// ==============================================================================

static void runSoloTest(bool linearPhase)
{
    EqProcessor eq;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = 48000.0;
    spec.maximumBlockSize = 512;
    spec.numChannels = 2;
    eq.prepare(spec);

    for (int i = 0; i < Params::NUM_BANDS; ++i)
    {
        const bool isTarget = (i == 0);
        eq.updateBand(i, Params::Peak, true, 1000.0f, +6.0f, 1.0f,
                      isTarget, 0, 0, spec.sampleRate);   // band0 solo
    }
    eq.setProcessingMode(linearPhase ? 1 : 0);

    constexpr int blockSize = 512;
    constexpr int numBlocks = 40;
    double sumSq = 0.0;
    for (int b = 0; b < numBlocks; ++b)
    {
        juce::AudioBuffer<float> buf(2, blockSize);
        for (int s = 0; s < blockSize; ++s)
        {
            const double t = static_cast<double>(b * blockSize + s) / spec.sampleRate;
            const float v = 0.5f * static_cast<float>(std::sin(2.0 * 3.141592653589793 * 1000.0 * t));
            buf.setSample(0, s, v);
            buf.setSample(1, s, v);
        }
        eq.process(buf);
        for (int s = 0; s < blockSize; ++s)
            sumSq += buf.getSample(0, s) * buf.getSample(0, s);
    }
    const double rms = std::sqrt(sumSq / (numBlocks * blockSize));
    std::printf("[%s] solo 路径: RMS=%.4f  %s\n",
                linearPhase ? "LinearPhase" : "ZeroLatency", rms,
                (rms > 0.001) ? "PASS: 有声" : "FAIL: 无声");
}

int main()
{
    runModeTest(0, "ZeroLatency");
    runModeTest(1, "LinearPhase");

    std::printf("\n--- Bell 提升方向验证 ---\n");
    runBellBoostTest(34.0, 1.0, true);
    runBellBoostTest(34.0, 1.0, false);
    runBellBoostTest(34.0, 10.0, true);
    runBellBoostTest(34.0, 10.0, false);
    runBellBoostTest(100.0, 1.0, true);
    runBellBoostTest(1000.0, 1.0, true);
    runBellBoostTest(1000.0, 10.0, true);

    std::printf("\n--- 低频 Cut 频率扫描（60Hz 低切 Q=1）---\n");
    const double cutFreqs[] = { 20.0, 40.0, 60.0, 100.0, 200.0 };
    for (double f : cutFreqs)
        runCutTest(f, true);
    for (double f : cutFreqs)
        runCutTest(f, false);

    std::printf("\n--- solo 回退路径 ---\n");
    runSoloTest(true);
    runSoloTest(false);
    return 0;
}
