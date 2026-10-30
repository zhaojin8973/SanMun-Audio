#pragma once

static bool testAutoThreshold()
{
    std::cout << "[运行] 自动阈值的电平适应、突发响应、静音和手动恢复" << std::endl;
    for (const double rate : {44100.0, 48000.0, 96000.0})
    {
        double reference = -1;
        for (const float inputDb : {-48.f, -24.f, -6.f})
        {
            DynamicBand band; band.prepare(rate); band.configure(0, 1000, 2, true, 6, -24, true);
            const auto run = [&](float levelDb, double seconds, double frequency)
            {
                const float amplitude = juce::Decibels::decibelsToGain(levelDb);
                for (int i = 0; i < static_cast<int>(rate * seconds); ++i)
                {
                    float left = amplitude * std::sin(2 * juce::MathConstants<double>::pi * frequency * i / rate), right = left;
                    band.process(left, right);
                    if (!std::isfinite(left) || !std::isfinite(right)) return false;
                }
                return true;
            };
            TEST_ASSERT(run(-120, .25, 1000), "静音输出非有限");
            TEST_ASSERT(!band.isThresholdReady() && band.getReduction() < .001f, "静音不应学习噪声底或触发压缩");
            TEST_ASSERT(run(inputDb, 3, 1000), "稳态输出非有限");
            const float steady = band.getReduction();
            std::cout << rate << " Hz，输入 " << inputDb << " dBFS，自动阈值 " << band.getThresholdDb() << " dB，稳态衰减 " << steady << " dB" << std::endl;
            TEST_ASSERT(band.isThresholdReady() && steady > .05f && steady < .35f, "自动阈值应保持温和稳态衰减");
            if (reference > 0) TEST_ASSERT(std::abs(reference - steady) < .15, "相同频谱的绝对电平改变不应显著改变自动衰减");
            reference = steady;
            TEST_ASSERT(run(inputDb + 12, .1, 1000), "突发输出非有限");
            TEST_ASSERT(band.getReduction() > steady + 1 && band.getReduction() <= 2.101f, "突发信号应触发更大且受范围限制的衰减");
            TEST_ASSERT(run(-120, 3, 1000), "静音释放非有限");
            TEST_ASSERT(band.getReduction() < .01f, "静音后衰减应释放");
            band.configure(0, 1000, 2, true, 6, 0, false);
            TEST_ASSERT(run(-24, 1, 1000), "手动阈值输出非有限");
            TEST_ASSERT(band.getReduction() < .01f && band.getThresholdDb() == 0, "返回手动模式未使用保存的阈值");
            band.configure(0, 3000, 2, true, 6, -24, true);
            TEST_ASSERT(run(-120, .1, 1000), "重新学习输出非有限");
            TEST_ASSERT(!band.isThresholdReady(), "扫频换检测带后应重新建立参考");
        }
    }
    return true;
}
