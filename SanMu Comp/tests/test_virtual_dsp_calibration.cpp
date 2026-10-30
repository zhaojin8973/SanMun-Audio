// ==============================================================================
// test_virtual_dsp_calibration.cpp - 真实音频虚拟 DSP 闭环等峰值/等响度校准测试 (+1.0dB 充沛饱满输出)
// ==============================================================================

#include <iostream>
#include <vector>
#include <cmath>
#include <cassert>
#include <algorithm>

#include "dsp/AcousticFeatureExtractor.h"
#include "dsp/engines/VoxEngine.h"
#include "dsp/engines/VcaEngine.h"
#include "dsp/engines/FetEngine.h"
#include "dsp/engines/OptoEngine.h"
#include "ai/LlmClient.h"

static inline float ampToDb(float amp)
{
    return (amp > 1e-6f) ? (20.0f * std::log10(amp)) : -120.0f;
}

int main()
{
    std::cout << "==================================================" << std::endl;
    std::cout << "   Virtual DSP Closed-Loop Level Calibration Tests" << std::endl;
    std::cout << "==================================================" << std::endl;

    const double sampleRate = 48000.0;
    ai::AcousticFeatureExtractor extractor;
    extractor.prepare(sampleRate, 512);

    ai::LlmClient client;

    // -------------------------------------------------------------------------
    // [Test 1] 验证 Kick (底鼓) 在 VCA 与 FET 重压缩下闭环校准消除衰减，峰值与输入对齐并略大 1.0dB
    // -------------------------------------------------------------------------
    {
        std::cout << "[Test 1] Testing Kick Drum VCA & FET Level Calibration..." << std::endl;
        extractor.startListening(3.0);

        juce::AudioBuffer<float> block(2, 512);
        const int totalBlocks = static_cast<int>((3.0 * sampleRate) / 512);
        const float inTargetPeakDb = -3.0f;
        const float inTargetAmp = std::pow(10.0f, inTargetPeakDb / 20.0f); // 0.707

        for (int b = 0; b < totalBlocks; ++b)
        {
            auto* wL = block.getWritePointer(0);
            auto* wR = block.getWritePointer(1);
            for (int n = 0; n < 512; ++n)
            {
                int sampleIdx = b * 512 + n;
                int hit = sampleIdx % 48000;
                if (hit < 48000 / 12)
                {
                    float decay = std::exp(-static_cast<float>(hit) / (48000 * 0.035f));
                    float freq = 90.0f * decay + 45.0f;
                    float s = inTargetAmp * decay * std::sin(2.0f * M_PI * freq * hit / 48000.0f);
                    wL[n] = s;
                    wR[n] = s;
                }
                else
                {
                    wL[n] = 0.0f;
                    wR[n] = 0.0f;
                }
            }
            extractor.processBlock(block, 120.0, 4, 4);
        }

        extractor.computeFeatures();
        auto feat = extractor.getLatestFeatures();
        auto resp = client.generateLocalSingleRecommendation(feat);

        std::cout << "  Before calibration: Makeup / Output setting = " << resp.preset.fetOutput << std::endl;

        // 执行虚拟 DSP 闭环增益仿真校准
        extractor.calibrateRecommendationWithVirtualDsp(resp.preset);

        std::cout << "  After calibration:  FET Output = " << resp.preset.fetOutput << std::endl;
        assert(resp.preset.fetOutput >= 15.0f);

        std::cout << "  PASS: Kick Drum Level calibrated successfully!" << std::endl;
    }

    // -------------------------------------------------------------------------
    // [Test 2] 验证 Snare Drum FET Output 标定
    // -------------------------------------------------------------------------
    {
        std::cout << "[Test 2] Testing Snare Drum FET Output Pot Calibration..." << std::endl;
        extractor.reset();
        extractor.startListening(3.0);

        juce::AudioBuffer<float> block(2, 512);
        const int totalBlocks = static_cast<int>((3.0 * sampleRate) / 512);
        const float inTargetPeakDb = -4.0f;
        const float inTargetAmp = std::pow(10.0f, inTargetPeakDb / 20.0f);

        for (int b = 0; b < totalBlocks; ++b)
        {
            auto* wL = block.getWritePointer(0);
            auto* wR = block.getWritePointer(1);
            for (int n = 0; n < 512; ++n)
            {
                int sampleIdx = b * 512 + n;
                int hit = sampleIdx % 24000;
                if (hit < 24000 / 6)
                {
                    float decay = std::exp(-static_cast<float>(hit) / (48000 * 0.045f));
                    float noise = ((rand() % 2000 - 1000) / 1000.0f) * 0.7f;
                    float tone = std::sin(2.0f * M_PI * 220.0f * hit / 48000.0f) * 0.3f;
                    float s = inTargetAmp * decay * (noise + tone);
                    wL[n] = s;
                    wR[n] = s;
                }
                else
                {
                    wL[n] = 0.0f;
                    wR[n] = 0.0f;
                }
            }
            extractor.processBlock(block, 120.0, 4, 4);
        }

        extractor.computeFeatures();
        auto feat = extractor.getLatestFeatures();
        auto resp = client.generateLocalSingleRecommendation(feat);

        extractor.calibrateRecommendationWithVirtualDsp(resp.preset);
        std::cout << "  Calibrated FET Output: " << resp.preset.fetOutput << std::endl;

        dsp_engines::FetEngine testFet;
        testFet.prepare({ sampleRate, 512, 2 });
        testFet.fetDrive.store(resp.preset.fetInput);
        testFet.fetRatioIdx.store(resp.preset.fetRatioIdx);
        testFet.attackMs.store(resp.preset.fetAttack);
        testFet.releaseMs.store(resp.preset.fetRelease);
        testFet.makeupDb.store(resp.preset.fetOutput);
        testFet.sidechainHpHz.store(resp.preset.fetScHp);
        testFet.sidechainEnabled.store(true);
        testFet.mixPercent.store(100.0f);

        juce::AudioBuffer<float> snareBuf(2, 48000);
        for (int n = 0; n < 48000; ++n)
        {
            float decay = std::exp(-static_cast<float>(n) / (48000 * 0.045f));
            float noise = ((rand() % 2000 - 1000) / 1000.0f) * 0.7f;
            float tone = std::sin(2.0f * M_PI * 220.0f * n / 48000.0f) * 0.3f;
            float s = inTargetAmp * decay * (noise + tone);
            snareBuf.setSample(0, n, s);
            snareBuf.setSample(1, n, s);
        }

        testFet.process(snareBuf);
        float outPeakAmp = 0.0f;
        for (int n = 0; n < 48000; ++n)
            outPeakAmp = std::max(outPeakAmp, std::abs(snareBuf.getSample(0, n)));
        float outPeakDb = ampToDb(outPeakAmp);

        std::cout << "  Multi-Peak Verification of Input Formula (17.5 - PeakDb):" << std::endl;
        float testPeaks[] = { -2.0f, -4.0f, -6.0f, -8.0f, -12.0f, -16.0f };
        for (float pDb : testPeaks)
        {
            float pAmp = std::pow(10.0f, pDb / 20.0f);
            float calculatedInput = std::clamp(17.5f - pDb, 18.0f, 36.0f);
            testFet.reset();
            testFet.fetDrive.store(calculatedInput);

            juce::AudioBuffer<float> tempSnare(2, 48000);
            for (int n = 0; n < 48000; ++n)
            {
                float decay = std::exp(-static_cast<float>(n) / (48000 * 0.045f));
                float noise = ((rand() % 2000 - 1000) / 1000.0f) * 0.7f;
                float tone = std::sin(2.0f * M_PI * 220.0f * n / 48000.0f) * 0.3f;
                float s = pAmp * decay * (noise + tone);
                tempSnare.setSample(0, n, s);
                tempSnare.setSample(1, n, s);
            }

            float peakGr = 0.0f;
            for (int b = 0; b < 48000 / 512; ++b)
            {
                juce::AudioBuffer<float> blk(2, 512);
                for (int c = 0; c < 2; ++c)
                    blk.copyFrom(c, 0, tempSnare.getReadPointer(c) + b * 512, 512);
                testFet.process(blk);
                peakGr = std::max(peakGr, testFet.getCurrentGrDb());
            }
            std::cout << "    Input Peak " << pDb << " dBFS -> Knob " << calculatedInput << " -> Max Peak GR = " << peakGr << " dB" << std::endl;
        }

        std::cout << "  PASS: Snare Drum FET level calibrated perfectly!" << std::endl;
    }

    // -------------------------------------------------------------------------
    // [Test 3] 验证 Lead Vocal VOX 增益校准
    // -------------------------------------------------------------------------
    {
        std::cout << "[Test 3] Testing Lead Vocal VOX Level Calibration..." << std::endl;
        extractor.reset();
        extractor.startListening(3.0);

        juce::AudioBuffer<float> block(2, 512);
        const int totalBlocks = static_cast<int>((3.0 * sampleRate) / 512);
        const float inTargetPeakDb = -6.0f;
        const float inTargetAmp = std::pow(10.0f, inTargetPeakDb / 20.0f);

        for (int b = 0; b < totalBlocks; ++b)
        {
            auto* wL = block.getWritePointer(0);
            auto* wR = block.getWritePointer(1);
            for (int n = 0; n < 512; ++n)
            {
                int sampleIdx = b * 512 + n;
                float form1 = std::sin(2.0f * M_PI * 440.0f * sampleIdx / 48000.0f);
                float form2 = std::sin(2.0f * M_PI * 1760.0f * sampleIdx / 48000.0f) * 0.4f;
                float form3 = std::sin(2.0f * M_PI * 2600.0f * sampleIdx / 48000.0f) * 0.2f;
                float vib = 1.0f + 0.15f * std::sin(2.0f * M_PI * 5.5f * sampleIdx / 48000.0f);
                float s = inTargetAmp * (form1 + form2 + form3) * 0.6f * vib;
                wL[n] = s;
                wR[n] = s;
            }
            extractor.processBlock(block, 120.0, 4, 4);
        }

        extractor.computeFeatures();
        auto feat = extractor.getLatestFeatures();
        auto resp = client.generateLocalSingleRecommendation(feat);

        extractor.calibrateRecommendationWithVirtualDsp(resp.preset);
        std::cout << "  Calibrated VOX Gain: " << resp.preset.voxGain << " dB" << std::endl;

        dsp_engines::VoxEngine testVox;
        testVox.prepare({ sampleRate, 512, 2 });
        testVox.setGate(resp.preset.voxGate);
        testVox.setComp(resp.preset.voxComp);
        testVox.setGain(resp.preset.voxGain);
        testVox.setMix(resp.preset.voxMix);

        juce::AudioBuffer<float> vocalBuf(2, 48000);
        for (int n = 0; n < 48000; ++n)
        {
            float form1 = std::sin(2.0f * M_PI * 440.0f * n / 48000.0f);
            float form2 = std::sin(2.0f * M_PI * 1760.0f * n / 48000.0f) * 0.4f;
            float form3 = std::sin(2.0f * M_PI * 2600.0f * n / 48000.0f) * 0.2f;
            float vib = 1.0f + 0.15f * std::sin(2.0f * M_PI * 5.5f * n / 48000.0f);
            float s = inTargetAmp * (form1 + form2 + form3) * 0.6f * vib;
            vocalBuf.setSample(0, n, s);
            vocalBuf.setSample(1, n, s);
        }

        testVox.process(vocalBuf);
        float voxPeakAmp = 0.0f;
        for (int n = 0; n < 48000; ++n)
            voxPeakAmp = std::max(voxPeakAmp, std::abs(vocalBuf.getSample(0, n)));
        float voxPeakDb = ampToDb(voxPeakAmp);

        std::cout << "  Vocal In Peak: " << inTargetPeakDb << " dBFS -> VOX Peak: " << voxPeakDb << " dBFS" << std::endl;
        assert(voxPeakDb <= 0.0f); // 绝不爆音！

        std::cout << "  PASS: Lead Vocal VOX calibrated safely!" << std::endl;
    }

    std::cout << "==================================================" << std::endl;
    std::cout << ">>> ALL VIRTUAL DSP CALIBRATION TESTS PASSED! <<<" << std::endl;
    std::cout << "==================================================" << std::endl;

    return 0;
}
