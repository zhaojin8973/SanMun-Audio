#pragma once

#include <chrono>

static bool testUnchangedBandAndInitialSmoothing()
{
    std::cout << "[运行] 频段变更检测与非默认频率初始化" << std::endl;
    EqBand band;
    band.frequency.store(150); band.gain.store(6);
    band.prepare({96000, 256, 1});
    TEST_ASSERT(!band.update(0, true, 150, 6, 1, false, 0, 0), "相同参数不应重复标记滤波器重建");
    band.update(0, true, 150, 6.1f, 1, false, 0, 0);
    band.recalculateCoefficients(96000); band.advanceSmoothing(96000);
    double input = 0, output = 0;
    for (int i = 0; i < 96000; ++i)
    {
        const float x = .1f * std::sin(2 * juce::MathConstants<double>::pi * 150 * i / 96000);
        const float y = band.processSampleChannel(0, x);
        if (i > 48000) { input += x * x; output += y * y; }
    }
    TEST_ASSERT(std::abs(10 * std::log10(output / input) - (6 + .1 * (1 - std::exp(-128. / (96000 * .015))))) < .02, "首次平滑必须从真实初始频率和增益开始");
    return true;
}

static bool testLinearPhaseMixedPlacements()
{
    std::cout << "[运行] 线性相位混合 L/R/M/S 路由、顺序与延迟" << std::endl;
    const std::array<std::array<int, 2>, 6> cases{{{{1, 3}}, {{3, 1}}, {{2, 4}}, {{4, 2}}, {{3, 4}}, {{0, 2}}}};
    for (const int blockSize : {64, 257}) for (const auto places : cases)
    {
        constexpr double rate = 48000, frequency = 1500;
        auto eq = std::make_unique<EqProcessor>(); eq->prepare({rate, static_cast<juce::uint32>(blockSize), 2}); eq->setProcessingMode(1);
        for (int i = 0; i < Params::NUM_BANDS; ++i)
            eq->updateBand(i, 0, i < 2, frequency, i == 0 ? 6 : -3, 1, false, 0, i < 2 ? places[i] : 0, rate, i < 2);
        double left = 1, right = -.25;
        for (int i = 0; i < 2; ++i)
        {
            const double a = std::pow(10., (i == 0 ? 6 : -3) / 20.);
            if (places[i] == 0) { left *= a; right *= a; }
            else if (places[i] == 1) left *= a;
            else if (places[i] == 2) right *= a;
            else
            {
                double mid = (left + right) * .5, side = (left - right) * .5;
                if (places[i] == 3) mid *= a; else side *= a;
                left = mid + side; right = mid - side;
            }
        }
        juce::AudioBuffer<float> audio(2, blockSize);
        double measuredL = 0, measuredR = 0, reference = 0;
        for (int offset = 0; offset < 40000; offset += blockSize)
        {
            for (int s = 0; s < blockSize; ++s)
            {
                const float x = .1f * std::sin(2 * juce::MathConstants<double>::pi * frequency * (offset + s) / rate);
                audio.setSample(0, s, x); audio.setSample(1, s, -.25f * x);
            }
            eq->process(audio);
            if (offset > 20000) for (int s = 0; s < blockSize; ++s)
            {
                const double x = .1 * std::sin(2 * juce::MathConstants<double>::pi * frequency * (offset + s - LinearPhaseEq::LATENCY) / rate);
                measuredL += audio.getSample(0, s) * x; measuredR += audio.getSample(1, s) * x; reference += x * x;
            }
        }
        std::cout << "路由 " << places[0] << "/" << places[1] << "，块长 " << blockSize << "，实测 " << measuredL / reference << "/" << measuredR / reference << "，目标 " << left << "/" << right << std::endl;
        TEST_ASSERT(std::abs(measuredL / reference - left) < .015 && std::abs(measuredR / reference - right) < .015, "混合声道路由不得丢失 Side 或把 Mid 当成 Stereo");
    }
    return true;
}

static bool testMonoDynamicProcessing()
{
    std::cout << "[运行] 单声道处理内核的动态衰减" << std::endl;
    auto eq = std::make_unique<EqProcessor>(); eq->prepare({48000, 256, 1});
    for (int i = 0; i < Params::NUM_BANDS; ++i) eq->updateBand(i, 0, i == 0, 1500, 0, 4, false, 0, 0, 48000, i == 0, i == 0, 6, -30);
    juce::AudioBuffer<float> audio(1, 256);
    double energy = 0, input = 0;
    for (int b = 0; b < 180; ++b)
    {
        for (int s = 0; s < 256; ++s) { const float x = .5f * std::sin(2 * juce::MathConstants<double>::pi * 1500 * (b * 256 + s) / 48000); audio.setSample(0, s, x); if (b > 90) input += x * x; }
        eq->process(audio);
        if (b > 90) for (int s = 0; s < 256; ++s) energy += std::pow(audio.getSample(0, s), 2);
    }
    TEST_ASSERT(std::abs(10 * std::log10(energy / input) + 6) < .1, "单声道动态必须真正作用于音频");
    return true;
}

static bool testLinearPhaseDynamicProcessing()
{
    std::cout << "[运行] 线性相位动态 EQ 的检测、重建与衰减" << std::endl;
    constexpr double rate = 48000.0;
    constexpr int blockSize = 256;
    constexpr int totalSamples = 56000;
    EqProcessor eq;
    eq.prepare({ rate, static_cast<juce::uint32>(blockSize), 2 });
    eq.setProcessingMode(1);
    for (int i = 0; i < Params::NUM_BANDS; ++i)
        eq.updateBand(i, Params::Peak, i == 0, 1500.0f, 0.0f, 1.0f, false, 0,
                      Params::PlacementStereo, rate, i == 0, i == 0, 6.0f, -30.0f, false);

    std::vector<float> output(static_cast<size_t>(totalSamples), 0.0f);
    juce::AudioBuffer<float> block(2, blockSize);
    for (int offset = 0; offset < totalSamples; offset += blockSize)
    {
        const int count = std::min(blockSize, totalSamples - offset);
        block.setSize(2, count, false, false, true);
        for (int s = 0; s < count; ++s)
        {
            const float x = 0.5f * std::sin(2.0f * juce::MathConstants<float>::pi
                                             * 1500.0f * static_cast<float>(offset + s) / static_cast<float>(rate));
            block.setSample(0, s, x);
            block.setSample(1, s, x);
        }
        eq.process(block);
        for (int s = 0; s < count; ++s)
            output[static_cast<size_t>(offset + s)] = block.getSample(0, s);
    }

    double dot = 0.0, ref = 0.0;
    for (int n = LinearPhaseEq::LATENCY + 12000; n < totalSamples; ++n)
    {
        const float x = 0.5f * std::sin(2.0f * juce::MathConstants<float>::pi
                                         * 1500.0f * static_cast<float>(n - LinearPhaseEq::LATENCY) / static_cast<float>(rate));
        dot += static_cast<double>(output[static_cast<size_t>(n)]) * x;
        ref += static_cast<double>(x) * x;
    }
    const float gainDb = static_cast<float>(20.0 * std::log10(std::max(1.0e-9, std::abs(dot / ref))));
    std::cout << "  线性相位动态中心频率增益: " << gainDb << " dB，动态表: "
              << eq.getBand(0).getDynamicReduction() << " dB" << std::endl;
    TEST_ASSERT(eq.getBand(0).getDynamicReduction() > 1.0f, "Linear Phase dynamic detector must respond");
    TEST_ASSERT(std::abs(gainDb + 6) < .15f, "线性相位中心频率应实际衰减 6 dB");
    TEST_ASSERT(std::abs(eq.getAppliedDynamicReduction(0) + gainDb) < .15f, "应用衰减表必须与稳态音频吻合");
    return true;
}

static bool testLinearPhaseRealtimeBudget()
{
    using Clock = std::chrono::steady_clock;
    constexpr int blockSize = 64;
    std::cout << "[测量] 线性相位多采样率、64 样本回调预算（与正确性回归分开）" << std::endl;

    for (const double rate : {44100., 48000., 96000., 192000.})
    for (const int bandCount : {1, 6, 24})
    {
        const double blockBudgetMs = 1000.0 * blockSize / rate;
        EqProcessor eq;
        eq.prepare({rate, static_cast<juce::uint32>(blockSize), 2});
        eq.setProcessingMode(1);
        for (int i = 0; i < Params::NUM_BANDS; ++i)
            eq.updateBand(i, Params::Peak, i < bandCount,
                          80.0f * std::pow(200.0f, static_cast<float>(i) / 23.0f),
                          0.0f, 1.0f, false, 0, Params::PlacementStereo, rate,
                          i < bandCount, i < bandCount, 6.0f, -30.0f, false);

        juce::AudioBuffer<float> audio(2, blockSize);
        double worstMs = 0.0, totalMs = 0.0;
        for (int block = 0; block < 750; ++block)
        {
            for (int s = 0; s < blockSize; ++s)
            {
                const float x = 0.4f * std::sin(2.0f * juce::MathConstants<float>::pi
                                                * 1000.0f * static_cast<float>(block * blockSize + s) / static_cast<float>(rate));
                audio.setSample(0, s, x);
                audio.setSample(1, s, x);
            }
            const auto start = Clock::now();
            eq.process(audio);
            const double ms = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
            if (block > 20) { worstMs = std::max(worstMs, ms); totalMs += ms; }
        }
        std::cout << "  " << rate << " Hz / " << bandCount << " 段: 最坏 " << worstMs << " ms / 块，预算 "
                  << blockBudgetMs << " ms，平均 " << totalMs / 729.0 << " ms" << std::endl;
        if (worstMs > blockBudgetMs)
            std::cout << "  [超出预算] 此配置需要优化或增加宿主缓冲" << std::endl;
    }
    return true;
}

static bool measureLinearMultiInstance()
{
    using Clock = std::chrono::steady_clock;
    std::cout << "[测量] 四实例各 24 段、立体声白噪声、持续动态" << std::endl;
    for (const double rate : {48000., 96000., 192000.}) for (const int blockSize : {64, 256})
    {
        std::array<std::unique_ptr<EqProcessor>, 4> processors;
        for (auto& eq : processors)
        {
            eq = std::make_unique<EqProcessor>(); eq->prepare({rate, static_cast<juce::uint32>(blockSize), 2}); eq->setProcessingMode(1);
            for (int i = 0; i < 24; ++i)
                eq->updateBand(i, 0, true, 80 * std::pow(200.f, i / 23.f), 0, 1, false, 0, i % 5, rate, true, true, 12, -60);
        }
        juce::AudioBuffer<float> source(2, blockSize), audio(2, blockSize); uint32_t random = 17;
        double worst = 0, sum = 0; int samples = 0;
        for (int block = 0; block < 1000; ++block)
        {
            for (int ch = 0; ch < 2; ++ch) for (int s = 0; s < blockSize; ++s)
            { random = random * 1664525u + 1013904223u; source.setSample(ch, s, (float(random >> 8) / 8388608.f - 1) * .1f); }
            const auto start = Clock::now();
            for (auto& eq : processors)
            {
                for (int ch = 0; ch < 2; ++ch) audio.copyFrom(ch, 0, source.getReadPointer(ch), blockSize);
                eq->process(audio);
            }
            const double elapsed = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
            if (block > 32) { worst = std::max(worst, elapsed); sum += elapsed; ++samples; }
        }
        const double budget = 1000 * blockSize / rate;
        std::cout << rate << " Hz / " << blockSize << " 样本：四实例串行最坏 " << worst << " ms，平均 " << sum / samples << " ms，预算 " << budget << " ms" << (worst > budget ? "，超出预算" : "") << std::endl;
    }
    return true;
}
