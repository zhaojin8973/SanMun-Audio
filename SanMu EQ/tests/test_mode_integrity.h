#pragma once

static bool testMinimumPhaseTimeline()
{
    std::cout << "[运行] 前三种模式参数自动化的采样时间一致性" << std::endl;
    for (int mode = 0; mode < 3; ++mode)
    {
        std::vector<float> reference;
        for (const int blockSize : {64, 257, 1024})
        {
            auto eq = std::make_unique<EqProcessor>(); eq->prepare({48000, static_cast<juce::uint32>(blockSize), 2}); eq->setCharacterMode(mode);
            for (int i = 0; i < 24; ++i) eq->updateBand(i, 0, i == 0, 1500, 0, 1, false, 0, 0, 48000, i == 0);
            juce::AudioBuffer<float> block(2, blockSize); std::vector<float> output(24000);
            for (int offset = 0; offset < 24000;)
            {
                if (offset == 4096) eq->updateBand(0, 0, true, 1500, 12, 4, false, 0, 0, 48000);
                if (offset == 12288) eq->updateBand(0, 0, true, 2200, -6, .7f, false, 0, 0, 48000);
                int count = std::min(blockSize, 24000 - offset);
                for (int event : {4096, 12288}) if (offset < event) count = std::min(count, event - offset);
                block.setSize(2, count, false, false, true);
                for (int i = 0; i < count; ++i)
                {
                    const float x = .1f * std::sin(2 * juce::MathConstants<double>::pi * 1500 * (offset + i) / 48000);
                    block.setSample(0, i, x); block.setSample(1, i, x);
                }
                eq->process(block); std::copy_n(block.getReadPointer(0), count, output.begin() + offset); offset += count;
            }
            if (reference.empty()) reference = output;
            else
            {
                double error = 0; for (size_t i = 0; i < output.size(); ++i) error = std::max(error, double(std::abs(output[i] - reference[i])));
                std::cout << "  模式 " << mode << "，块长 " << blockSize << "，最大偏差 " << error << std::endl;
                TEST_ASSERT(error < 2e-5, "相同采样时刻的参数变化不得随宿主块长改变音频");
            }
        }
    }
    return true;
}

static bool testMinimumPhaseReprepare()
{
    std::cout << "[运行] 非默认预设重新初始化和高频中心增益" << std::endl;
    auto eq = std::make_unique<EqProcessor>(); eq->prepare({48000, 256, 2});
    for (int i = 0; i < 24; ++i) eq->updateBand(i, 0, i == 0, 10000, 6, 4, false, 0, 0, 48000, i == 0);
    for (const double rate : {48000., 96000., 44100.})
    {
        eq->prepare({rate, 256, 2});
        double input = 0, output = 0;
        juce::AudioBuffer<float> block(2, 256);
        for (int offset = 0; offset < int(rate); offset += 256)
        {
            for (int i = 0; i < 24; ++i) eq->updateBand(i, 0, i == 0, 10000, 6, 4, false, 0, 0, rate, i == 0);
            for (int s = 0; s < 256; ++s)
            {
                const float x = .1f * std::sin(2 * juce::MathConstants<double>::pi * 10000 * (offset + s) / rate);
                block.setSample(0, s, x); block.setSample(1, s, x); if (offset > rate / 2) input += x * x;
            }
            eq->process(block);
            if (offset > rate / 2) for (int s = 0; s < 256; ++s) output += std::pow(block.getSample(0, s), 2);
        }
        const double gain = 10 * std::log10(output / input);
        std::cout << "  " << rate << " Hz，10 kHz / +6 dB 实测 " << gain << " dB" << std::endl;
        TEST_ASSERT(std::abs(gain - 6) < .15, "重新准备后必须按真实内部采样率重建频率和增益");
    }
    return true;
}

static bool testAutoGainPlacementSymmetry()
{
    std::cout << "[运行] 自动补偿对左右和中侧位置的对称性" << std::endl;
    for (int engine : {0, 1})
    {
        std::array<double, 4> gains{};
        for (int placement = 1; placement <= 4; ++placement)
        {
            auto eq = std::make_unique<EqProcessor>(); eq->prepare({48000, 256, 2}); eq->setProcessingMode(engine); eq->setAutoGain(true);
            for (int i = 0; i < 24; ++i) eq->updateBand(i, 1, i == 0, 3000, 12, 1, false, 0, placement, 48000, i == 0);
            juce::AudioBuffer<float> block(2, 256); double in = 0, out = 0;
            for (int offset = 0; offset < 48000; offset += 256)
            {
                for (int s = 0; s < 256; ++s)
                {
                    const float a = .1f * std::sin((offset + s) * .13f), b = .1f * std::cos((offset + s) * .217f);
                    block.setSample(0, s, placement == 2 ? b : a); block.setSample(1, s, placement == 2 ? a : b);
                    if (offset > 32000) in += a * a + b * b;
                }
                eq->process(block);
                if (offset > 32000) for (int s = 0; s < 256; ++s) out += std::pow(block.getSample(0, s), 2) + std::pow(block.getSample(1, s), 2);
            }
            gains[static_cast<size_t>(placement - 1)] = 10 * std::log10(out / in);
        }
        std::cout << "  模式 " << engine << "，L/R/M/S 电平 " << gains[0] << "/" << gains[1] << "/" << gains[2] << "/" << gains[3] << " dB" << std::endl;
        TEST_ASSERT(std::abs(gains[0] - gains[1]) < .04 && std::abs(gains[2] - gains[3]) < .06, "自动补偿不能只读取左路或 Mid 的响应");
    }
    return true;
}
