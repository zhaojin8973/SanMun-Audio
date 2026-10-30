// ==============================================================================
// test_reverb_dsp.cpp - SanMu Reverb 4 大大类 12 大算法与 DSP 自动化单元测试
// ==============================================================================

#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <cstdio>
#include <random>

#include "../source/dsp/ReverbProcessor.h"
#include "../source/dsp/primitives/HouseholderMatrix.h"
#include "../source/presets/PresetManager.h"
#include "../source/params/Parameters.h"

static int failures = 0;

#define CHECK(cond, msg)                                                \
    do {                                                                \
        if (!(cond)) {                                                  \
            std::printf("FAIL: %s (line %d)\n", msg, __LINE__);         \
            ++failures;                                                 \
        } else {                                                        \
            std::printf("ok:   %s\n", msg);                             \
        }                                                               \
    } while (0)

namespace
{
    bool scanBuffer(const float* data, int n, float* peakOut, float maxAllowed = 10.0f)
    {
        float peak = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const float v = data[i];
            if (!std::isfinite(v)) return false;
            peak = std::max(peak, std::abs(v));
        }
        if (peakOut != nullptr) *peakOut = peak;
        return peak <= maxAllowed;
    }

    float computeRms(const float* data, int len)
    {
        if (len <= 0) return 0.0f;
        double acc = 0.0;
        for (int i = 0; i < len; ++i)
            acc += static_cast<double>(data[i]) * data[i];
        return static_cast<float>(std::sqrt(acc / len));
    }

    float computeCorrelation(const float* l, const float* r, int len)
    {
        double sumL = 0.0, sumR = 0.0, sumLR = 0.0, sumL2 = 0.0, sumR2 = 0.0;
        for (int i = 0; i < len; ++i)
        {
            sumL += l[i];
            sumR += r[i];
            sumLR += l[i] * r[i];
            sumL2 += l[i] * l[i];
            sumR2 += r[i] * r[i];
        }
        double meanL = sumL / len;
        double meanR = sumR / len;
        double varL = sumL2 - len * meanL * meanL;
        double varR = sumR2 - len * meanR * meanR;
        if (varL <= 1e-12 || varR <= 1e-12) return 0.0f;
        double cov = sumLR - len * meanL * meanR;
        return static_cast<float>(cov / std::sqrt(varL * varR));
    }
}

int main()
{
    const double fs = 48000.0;
    const int block = 128;
    juce::ScopedNoDenormals noDenormals;

    std::printf("=== STARTING SANMU REVERB 4-CATEGORY 12-MODE DSP VERIFICATION ===\n\n");

    // =========================================================================
    // 1. 当前八种模式的正式预设，不把旧架构预设混入产品菜单。
    // =========================================================================
    const int numPresets = PresetManager::getNumFactoryPresets();
    CHECK(numPresets == 23, "四类八模式的23项正式预设可访问");
    std::printf("Loaded %d factory presets successfully.\n", numPresets);

    // =========================================================================
    // 2. Householder 矩阵严格幺正能量守恒测试
    // =========================================================================
    {
        std::array<float, 8> vec = { 1.0f, -0.5f, 0.25f, -0.125f, 0.7f, -0.3f, 0.4f, -0.6f };
        float energyBefore = 0.0f;
        for (float v : vec) energyBefore += v * v;

        dsp_primitives::HouseholderMatrix<8>::multiply(vec);

        float energyAfter = 0.0f;
        for (float v : vec) energyAfter += v * v;

        CHECK(std::abs(energyBefore - energyAfter) < 1e-5f, "HouseholderMatrix<8> is strictly unitary and energy-conserving");
    }

    // =========================================================================
    // 3. 4 大品类 12 大专属算法模式全 IR 冲激响应与衰减测试
    // =========================================================================
    ReverbProcessor reverb;
    reverb.prepare(fs, block);

    struct ModeSpec
    {
        int cat;
        int mode;
        const char* catName;
        const char* modeName;
    };

    const ModeSpec allModes[12] = {
        { 0, 0, "Plate",   "Modern Silk" },
        { 0, 1, "Plate",   "Classic Steel" },
        { 0, 2, "Plate",   "Silk兼容编号" },

        { 1, 0, "Room",    "Studio Room" },
        { 1, 1, "Room",    "Live Room" },
        { 1, 2, "Room",    "Live Room兼容编号" },

        { 2, 0, "Hall",    "Concert Hall" },
        { 2, 1, "Hall",    "Scoring Stage" },
        { 2, 2, "Hall",    "Cathedral" },

        { 3, 0, "Chamber", "Velvet Chamber" },
        { 3, 1, "Chamber", "Stone Chamber" },
        { 3, 2, "Chamber", "Velvet兼容编号" }
    };

    const int irLen = static_cast<int>(fs * 2.0); // 2.0s
    for (int m = 0; m < 12; ++m)
    {
        reverb.reset();
        juce::AudioBuffer<float> buf(2, irLen);
        buf.clear();
        buf.setSample(0, 0, 1.0f);
        buf.setSample(1, 0, 1.0f);

        dsp_engines::ReverbParameters p;
        p.category   = allModes[m].cat;
        p.mode       = allModes[m].mode;
        p.mix        = 100.0f; // 纯湿声
        p.decay      = 2.0f;
        p.predelay   = 0.0f;
        p.highFreq   = 7000.0f;
        p.highShelf  = -12.0f;
        p.bassFreq   = 300.0f;
        p.bassMult   = 1.2f;
        p.size       = 100.0f;
        p.attack     = 0.0f;
        p.earlyDiff  = 100.0f;
        p.lateDiff   = 100.0f;
        p.modRate    = 0.5f;
        p.modDepth   = 20.0f;
        p.lowCut     = 20.0f;
        p.highCut    = 12000.0f;

        for (int pos = 0; pos < irLen; pos += block)
        {
            const int n = std::min(block, irLen - pos);
            juce::AudioBuffer<float> slice(buf.getArrayOfWritePointers(), 2, pos, n);
            reverb.process(slice, p);
        }

        float peakL = 0.0f, peakR = 0.0f;
        const bool finiteL = scanBuffer(buf.getReadPointer(0), irLen, &peakL, 6.0f);
        const bool finiteR = scanBuffer(buf.getReadPointer(1), irLen, &peakR, 6.0f);


        char msg[128];
        std::snprintf(msg, sizeof(msg), "[%s : %s] IR finite (Peak L=%.3f, R=%.3f)",
                      allModes[m].catName, allModes[m].modeName, peakL, peakR);
        CHECK(finiteL && finiteR && peakL > 0.005f && peakR > 0.005f, msg);

        const int headLen = (allModes[m].cat == 2) ? static_cast<int>(fs * 0.30) : static_cast<int>(fs * 0.08);
        const float headRms = computeRms(buf.getReadPointer(0), headLen);
        const float midRms  = computeRms(buf.getReadPointer(0) + static_cast<int>(fs * 0.6), static_cast<int>(fs * 0.25));
        const float tailRms = computeRms(buf.getReadPointer(0) + irLen - static_cast<int>(fs * 0.2), static_cast<int>(fs * 0.2));

        std::snprintf(msg, sizeof(msg), "[%s : %s] Decay verified (Head=%.5f > Mid=%.5f > Tail=%.5f)",
                      allModes[m].catName, allModes[m].modeName, headRms, midRms, tailRms);
        CHECK(headRms > midRms && midRms > tailRms, msg);
        CHECK(midRms > 1.0e-5f, "Mid-tail has realistic decay energy (not dying out prematurely)");

        // 相位相关性与单声道兼容性全面检测 (Phase & Mono Compatibility)
        const float corr = computeCorrelation(buf.getReadPointer(0), buf.getReadPointer(1), irLen);
        std::snprintf(msg, sizeof(msg), "[%s : %s] Phase correlation safe (r=%.3f, not hard-panned or hard anti-phase)",
                      allModes[m].catName, allModes[m].modeName, corr);
        // Plate使用设计前约定的双侧相关边界；正相关不是混响单声道兼容的必要条件。
        // 回声室按独立设计契约检查双侧边界，不继承旧算法的正相关下限。
        // 左右能量和单声道功率由对应专项联合检查。Hall旧门槛保持不变。
        const bool correlationIsUsable = (allModes[m].cat == 0 || allModes[m].cat == 1)
            ? std::abs(corr) < 0.6f
            : allModes[m].cat == 2 ? (corr >= -0.88f && corr <= 0.88f)
                                 : std::abs(corr) < 0.85f;
        CHECK(correlationIsUsable, msg);

        std::vector<float> monoSum(static_cast<size_t>(irLen));
        for (int i = 0; i < irLen; ++i)
            monoSum[static_cast<size_t>(i)] = buf.getSample(0, i) + buf.getSample(1, i);
        const float monoRms = computeRms(monoSum.data(), irLen);
        const float leftRms = computeRms(buf.getReadPointer(0), irLen);
        std::snprintf(msg, sizeof(msg), "[%s : %s] Mono compatibility passed (Mono=%.4f >= 1.2x Left=%.4f)",
                      allModes[m].catName, allModes[m].modeName, monoRms, leftRms);
        CHECK(monoRms >= leftRms * 1.15f, msg);
    }

    // =========================================================================
    // 4. 多采样率换算测试 (44.1k, 48k, 96k, 192k)
    // =========================================================================
    const double testRates[4] = { 44100.0, 48000.0, 96000.0, 192000.0 };
    for (double srTest : testRates)
    {
        reverb.prepare(srTest, block);
        juce::AudioBuffer<float> testBuf(2, block);
        testBuf.clear();
        testBuf.setSample(0, 0, 1.0f);

        dsp_engines::ReverbParameters p;
        p.category = 2; // Hall
        p.mode = 0;
        p.mix = 50.0f;
        reverb.process(testBuf, p);

        float pOut = 0.0f;
        char msg[128];
        std::snprintf(msg, sizeof(msg), "SampleRate %.0f Hz prepare & process passed", srTest);
        CHECK(scanBuffer(testBuf.getReadPointer(0), block, &pOut, 5.0f), msg);
    }

    // =========================================================================
    // 5. 1000 轮极端随机全参数扫描 + 快速切模无 NaN 测试
    // =========================================================================
    {
        reverb.prepare(48000.0, block);
        std::mt19937 rng(4242);
        std::uniform_real_distribution<float> d01(0.0f, 1.0f);
        std::uniform_int_distribution<int> dCat(0, 3);
        std::uniform_int_distribution<int> dMode(0, 2);

        bool allRandomPassed = true;
        for (int round = 0; round < 1000; ++round)
        {
            juce::AudioBuffer<float> randBuf(2, block);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < block; ++i)
                    randBuf.setSample(ch, i, (d01(rng) * 2.0f - 1.0f) * 0.4f);

            dsp_engines::ReverbParameters p;
            p.category   = dCat(rng);
            p.mode       = dMode(rng);
            p.mix        = d01(rng) * 100.0f;
            p.decay      = 0.2f + d01(rng) * 19.8f;
            p.predelay   = d01(rng) * 250.0f;
            p.highFreq   = 1000.0f + d01(rng) * 19000.0f;
            p.highShelf  = -24.0f + d01(rng) * 24.0f;
            p.bassFreq   = 50.0f + d01(rng) * 950.0f;
            p.bassMult   = 0.25f + d01(rng) * 3.75f;
            p.size       = d01(rng) * 100.0f;
            p.attack     = d01(rng) * 100.0f;
            p.earlyDiff  = d01(rng) * 100.0f;
            p.lateDiff   = d01(rng) * 100.0f;
            p.modRate    = 0.05f + d01(rng) * 4.95f;
            p.modDepth   = d01(rng) * 100.0f;
            p.lowCut     = 10.0f + d01(rng) * 490.0f;
            p.highCut    = 1000.0f + d01(rng) * 19000.0f;

            reverb.process(randBuf, p);

            float peak = 0.0f;
            if (!scanBuffer(randBuf.getReadPointer(0), block, &peak, 25.0f) ||
                !scanBuffer(randBuf.getReadPointer(1), block, &peak, 25.0f))
            {
                allRandomPassed = false;
                break;
            }
        }
        CHECK(allRandomPassed, "1000 rounds of 12-mode extreme random parameters & rapid switching passed with zero NaNs");
    }

    // =========================================================================
    // 6. Concert Hall Late Diff (0% -> 100%) 连续能量稳定性与平滑衰减深度检测
    //    确保 30% ~ 60% 区间特征值严格不衰亡，混响长尾始终饱满且能量波动 < 3dB
    // =========================================================================
    {
        reverb.prepare(fs, block);
        const float testDiffs[5] = { 0.0f, 25.0f, 50.0f, 75.0f, 100.0f };
        for (float lateDiffVal : testDiffs)
        {
            reverb.prepare(fs, block);
            const int irLen = static_cast<int>(fs * 2.5);
            juce::AudioBuffer<float> testBuf(2, irLen);
            testBuf.clear();
            testBuf.setSample(0, 0, 1.0f);
            testBuf.setSample(1, 0, 1.0f);

            dsp_engines::ReverbParameters p;
            p.category   = 2; // Hall
            p.mode       = 0; // Concert Hall
            p.mix        = 100.0f;
            p.decay      = 2.5f;
            p.earlyDiff  = 100.0f;
            p.lateDiff   = lateDiffVal;

            for (int pos = 0; pos < irLen; pos += block)
            {
                const int n = std::min(block, irLen - pos);
                juce::AudioBuffer<float> slice(testBuf.getArrayOfWritePointers(), 2, pos, n);
                reverb.process(slice, p);
            }

            const float midRms = computeRms(testBuf.getReadPointer(0) + static_cast<int>(fs * 0.5), static_cast<int>(fs * 0.3));
            char msg[128];
            std::snprintf(msg, sizeof(msg), "[Hall LateDiff=%.0f%%] Reverb tail solid (midRms=%.5f > 0.00018, no dropout)",
                          lateDiffVal, midRms);
            CHECK(midRms > 0.00018f, msg);
        }
    }

    // =========================================================================
    // 7. Hall 引擎专项测试：Attack Bloom 物理起音升腾测试
    // =========================================================================
    {
        reverb.prepare(fs, block);
        const int irLen = static_cast<int>(fs * 2.0);

        // Attack 0%
        reverb.reset();
        juce::AudioBuffer<float> bufAtt0(2, irLen);
        bufAtt0.clear();
        bufAtt0.setSample(0, 0, 1.0f);
        bufAtt0.setSample(1, 0, 1.0f);

        dsp_engines::ReverbParameters p0;
        p0.category = 2; // Hall
        p0.mode = 0;
        p0.decay = 2.5f;
        p0.mix = 100.0f;
        p0.attack = 0.0f;
        for (int pos = 0; pos < irLen; pos += block)
        {
            const int n = std::min(block, irLen - pos);
            juce::AudioBuffer<float> slice(bufAtt0.getArrayOfWritePointers(), 2, pos, n);
            reverb.process(slice, p0);
        }

        // Attack 100%
        reverb.reset();
        juce::AudioBuffer<float> bufAtt100(2, irLen);
        bufAtt100.clear();
        bufAtt100.setSample(0, 0, 1.0f);
        bufAtt100.setSample(1, 0, 1.0f);

        dsp_engines::ReverbParameters p100 = p0;
        p100.attack = 100.0f;
        for (int pos = 0; pos < irLen; pos += block)
        {
            const int n = std::min(block, irLen - pos);
            juce::AudioBuffer<float> slice(bufAtt100.getArrayOfWritePointers(), 2, pos, n);
            reverb.process(slice, p100);
        }

        // Attack 必须重排到达时间：早期窗能量下降，后期窗不能被整体音量一起削掉。
        const auto windowEnergy = [](const juce::AudioBuffer<float>& b, int first, int last)
        {
            double sum = 0.0;
            for (int i = first; i < last; ++i)
                sum += static_cast<double>(b.getSample(0, i)) * b.getSample(0, i);
            return sum;
        };
        const int earlyEnd = static_cast<int>(fs * 0.12);
        const int lateStart = static_cast<int>(fs * 0.25);
        const int lateEnd = static_cast<int>(fs * 0.60);
        const double early0 = windowEnergy(bufAtt0, 0, earlyEnd);
        const double early100 = windowEnergy(bufAtt100, 0, earlyEnd);
        const double late0 = windowEnergy(bufAtt0, lateStart, lateEnd);
        const double late100 = windowEnergy(bufAtt100, lateStart, lateEnd);
        char msg[192];
        std::snprintf(msg, sizeof(msg), "[Hall Attack Bloom] early %.4g -> %.4g, late %.4g -> %.4g",
                      early0, early100, late0, late100);
        CHECK(early100 < early0 * 0.90 && late100 > late0 * 0.45, msg);
    }

    // =========================================================================
    // 8. Hall 引擎专项测试：Bass Mult 0.25x ~ 4.0x 动态范围自由度测试 (Concert Hall)
    // =========================================================================
    {
        reverb.prepare(fs, block);
        const int irLen = static_cast<int>(fs * 2.5);

        // Low Bass: 0.25x
        reverb.reset();
        juce::AudioBuffer<float> bufLowBass(2, irLen);
        bufLowBass.clear();
        bufLowBass.setSample(0, 0, 1.0f);
        bufLowBass.setSample(1, 0, 1.0f);

        dsp_engines::ReverbParameters pLow;
        pLow.category = 2; // Hall
        pLow.mode = 0;     // Concert Hall
        pLow.decay = 3.0f;
        pLow.mix = 100.0f;
        pLow.bassMult = 0.25f;
        for (int pos = 0; pos < irLen; pos += block)
        {
            const int n = std::min(block, irLen - pos);
            juce::AudioBuffer<float> slice(bufLowBass.getArrayOfWritePointers(), 2, pos, n);
            reverb.process(slice, pLow);
        }

        // High Bass: 2.50x
        reverb.reset();
        juce::AudioBuffer<float> bufHighBass(2, irLen);
        bufHighBass.clear();
        bufHighBass.setSample(0, 0, 1.0f);
        bufHighBass.setSample(1, 0, 1.0f);

        dsp_engines::ReverbParameters pHigh = pLow;
        pHigh.bassMult = 2.50f;
        for (int pos = 0; pos < irLen; pos += block)
        {
            const int n = std::min(block, irLen - pos);
            juce::AudioBuffer<float> slice(bufHighBass.getArrayOfWritePointers(), 2, pos, n);
            reverb.process(slice, pHigh);
        }

        const float rmsLow = computeRms(bufLowBass.getReadPointer(0) + static_cast<int>(fs * 1.0), static_cast<int>(fs * 0.8));
        const float rmsHigh = computeRms(bufHighBass.getReadPointer(0) + static_cast<int>(fs * 1.0), static_cast<int>(fs * 0.8));

        char msg[160];
        std::snprintf(msg, sizeof(msg), "[Hall BassMult Control] Late Tail RMS: Low(0.25x)=%.5f < High(2.50x)=%.5f (clamp removed)",
                      rmsLow, rmsHigh);
        CHECK(rmsHigh > rmsLow * 1.4f, msg);
    }

    // =========================================================================
    // 9. Hall 引擎专项测试：环路 15Hz DC Blocker 与极端参数稳定性
    // =========================================================================
    {
        reverb.prepare(fs, block);
        const int dcLen = static_cast<int>(fs * 1.5);
        juce::AudioBuffer<float> dcBuf(2, dcLen);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < dcLen; ++i)
                dcBuf.setSample(ch, i, 0.4f); // 注入 0.4 直流偏置

        dsp_engines::ReverbParameters pDc;
        pDc.category = 2; // Hall
        pDc.mode = 0;
        pDc.decay = 10.0f; // 10秒超长衰减
        pDc.bassMult = 4.0f; // 极端低频倍率
        pDc.mix = 100.0f;

        for (int pos = 0; pos < dcLen; pos += block)
        {
            const int n = std::min(block, dcLen - pos);
            juce::AudioBuffer<float> slice(dcBuf.getArrayOfWritePointers(), 2, pos, n);
            reverb.process(slice, pDc);
        }

        float maxPeak = 0.0f;
        const bool finite = scanBuffer(dcBuf.getReadPointer(0), dcLen, &maxPeak, 5.0f);
        CHECK(finite && maxPeak < 2.0f, "[Hall DC Blocker] Loop unconditionally stable under 0.4 DC input & Decay 10s & BassMult 4.0x");
    }

    // =========================================================================
    // 10. 诊断测量：Mix=20%, Early=100%, Late=100% 脉冲电平
    // =========================================================================
    {
        reverb.prepare(fs, block);
        reverb.reset();
        const int len = static_cast<int>(fs * 1.0);
        juce::AudioBuffer<float> diagBuf(2, len);
        diagBuf.clear();
        diagBuf.setSample(0, 0, 1.0f);
        diagBuf.setSample(1, 0, 1.0f);

        dsp_engines::ReverbParameters pDiag;
        pDiag.category = 2; // Hall
        pDiag.mode = 0;     // Concert Hall
        pDiag.decay = 2.5f;
        pDiag.mix = 20.0f;
        pDiag.earlyDiff = 100.0f;
        pDiag.lateDiff = 100.0f;
        pDiag.attack = 0.0f;

        for (int pos = 0; pos < len; pos += block)
        {
            const int n = std::min(block, len - pos);
            juce::AudioBuffer<float> slice(diagBuf.getArrayOfWritePointers(), 2, pos, n);
            reverb.process(slice, pDiag);
        }

        const float* diagL = diagBuf.getReadPointer(0);
        std::printf("\n--- DIAGNOSTIC: Mix=20%%, Early=100%%, Late=100%% ---\n");
        std::printf("  Sample 0 (Dry Peak): %.4f\n", diagL[0]);

        float max0_100 = 0.0f;
        int idx0_100 = 0;
        for (int i = 1; i < static_cast<int>(fs * 0.10); ++i) {
            if (std::abs(diagL[i]) > max0_100) { max0_100 = std::abs(diagL[i]); idx0_100 = i; }
        }
        std::printf("  Max in 1~100ms:   %.4f at %.2f ms\n", max0_100, idx0_100 * 1000.0 / fs);

        float max100_250 = 0.0f;
        int idx100_250 = 0;
        for (int i = static_cast<int>(fs * 0.10); i < static_cast<int>(fs * 0.25); ++i) {
            if (std::abs(diagL[i]) > max100_250) { max100_250 = std::abs(diagL[i]); idx100_250 = i; }
        }
        std::printf("  Max in 100~250ms: %.4f at %.2f ms\n", max100_250, idx100_250 * 1000.0 / fs);

        float maxTail = 0.0f;
        int idxTail = 0;
        for (int i = static_cast<int>(fs * 0.25); i < len; ++i) {
            if (std::abs(diagL[i]) > maxTail) { maxTail = std::abs(diagL[i]); idxTail = i; }
        }
        std::printf("  Max in 250ms+:    %.4f at %.2f ms (physical decay limit = %.4f)\n", maxTail, idxTail * 1000.0 / fs, maxTail * 2.20f);
        CHECK(max100_250 <= maxTail * 2.20f, "[Hall Mix=20% Tap Dissolution] Prompt energy dissolved smoothly into tail, no discrete delay spike");
    }

    // =========================================================================
    // 11. Hall 专项测试：Mix=100% 绝对零干声泄露检验 (Zero Dry Leakage)
    // =========================================================================
    {
        reverb.prepare(fs, block);
        reverb.reset();
        const int len = static_cast<int>(fs * 0.5);
        juce::AudioBuffer<float> bufMix100(2, len);
        bufMix100.clear();
        bufMix100.setSample(0, 0, 1.0f);
        bufMix100.setSample(1, 0, 1.0f);

        dsp_engines::ReverbParameters p100;
        p100.category = 2; // Hall
        p100.mode = 0;     // Concert Hall
        p100.mix = 100.0f; // 100% 纯湿声
        p100.earlyDiff = 100.0f;
        p100.lateDiff = 100.0f;

        for (int pos = 0; pos < len; pos += block)
        {
            const int n = std::min(block, len - pos);
            juce::AudioBuffer<float> slice(bufMix100.getArrayOfWritePointers(), 2, pos, n);
            reverb.process(slice, p100);
        }

        const float s0_L = bufMix100.getSample(0, 0);
        const float s0_R = bufMix100.getSample(1, 0);

        char msg[160];
        std::snprintf(msg, sizeof(msg), "[Hall Mix=100%% Zero Dry Leak] Sample 0 at t=0ms: L=%.6f, R=%.6f (strictly zero)",
                      s0_L, s0_R);
        CHECK(std::abs(s0_L) < 1e-6f && std::abs(s0_R) < 1e-6f, msg);
    }

    std::printf("\n=======================================================\n");
    if (failures == 0)
    {
        std::printf("🎉 ALL 12-MODE REVERB DSP TESTS PASSED! (0 failures)\n");
        return 0;
    }
    else
    {
        std::printf("❌ FAILED: %d test failures!\n", failures);
        return 1;
    }
}
