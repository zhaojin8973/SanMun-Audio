#pragma once
#include "params/UpgradeState.h"

static bool testDynamicSelectivity()
{
    struct Result { double left, right; float reduction; };
    const auto measure = [](double rate, int blockSize, float signalFrequency, int placement, int engine)
    {
        auto eq = std::make_unique<EqProcessor>(); eq->prepare({rate, static_cast<juce::uint32>(blockSize), 2}); eq->setProcessingMode(engine);
        for (int i = 0; i < Params::NUM_BANDS; ++i)
            eq->updateBand(i, Params::Peak, i == 0, 1000, 0, 4, false, 0, placement, rate, i == 0, i == 0, 6, -18);
        double inputL = 0, inputR = 0, outputL = 0, outputR = 0;
        const int blocks = static_cast<int>(rate / blockSize);
        juce::AudioBuffer<float> buffer(2, blockSize);
        for (int b = 0; b < blocks; ++b)
        {
            for (int s = 0; s < blockSize; ++s)
            {
                const float x = .5f * static_cast<float>(std::sin(2 * juce::MathConstants<double>::pi * signalFrequency * (b * blockSize + s) / rate));
                buffer.setSample(0, s, x); buffer.setSample(1, s, x * .1f);
                if (b > blocks / 2) { inputL += x * x; inputR += x * x * .01; }
            }
            eq->process(buffer);
            if (b > blocks / 2) for (int s = 0; s < blockSize; ++s) { outputL += std::pow(buffer.getSample(0, s), 2); outputR += std::pow(buffer.getSample(1, s), 2); }
        }
        return Result{10 * std::log10(outputL / inputL), 10 * std::log10(outputR / inputR), eq->getBand(0).getDynamicReduction()};
    };
    std::cout << "[运行] 动态 EQ 的频段选择、立体声联动与块长一致性" << std::endl;
    for (const auto rate : {44100.0, 48000.0, 96000.0})
    {
        const auto small = measure(rate, 64, 1000, 0, 0), large = measure(rate, 512, 1000, 0, 0);
        std::cout << rate << " Hz: 目标频段 " << small.left << " dB，右声道 " << small.right << " dB，衰减表 " << small.reduction << " dB" << std::endl;
        TEST_ASSERT(small.left < -4 && small.left > -6.3, "目标频段必须得到受范围限制的实际衰减");
        TEST_ASSERT(std::abs(small.left - small.right) < .03, "立体声联动必须保持声像");
        TEST_ASSERT(std::abs(small.left - large.left) < .15, "动态响应不能随宿主块长明显改变");
        const auto outside = measure(rate, 256, 100, 0, 0);
        TEST_ASSERT(outside.reduction < .05f && std::abs(outside.left) < .1, "带外信号不应触发该频段压缩");
    }
    const auto leftOnly = measure(48000, 256, 1000, Params::PlacementLeft, 0);
    TEST_ASSERT(leftOnly.left < -4 && std::abs(leftOnly.right) < .1, "左声道动态不得影响右声道");
    for (const auto rate : {44100.0, 48000.0, 96000.0})
    {
        const auto linearSmall = measure(rate, 64, 1000, Params::PlacementStereo, 1);
        const auto linearLarge = measure(rate, 512, 1000, Params::PlacementStereo, 1);
        TEST_ASSERT(linearSmall.left < -4 && linearSmall.left > -6.3 && linearSmall.reduction > 5,
                    "线性相位动态必须经 FIR 产生可测衰减");
        TEST_ASSERT(std::abs(linearSmall.left - linearLarge.left) < .25,
                    "线性相位动态不能随宿主块长明显改变");
    }
    const auto linearLeft = measure(48000, 256, 1000, Params::PlacementLeft, 1);
    TEST_ASSERT(linearLeft.left < -4 && std::abs(linearLeft.right) < .1,
                "线性相位单侧动态不得改变另一声道");
    DynamicBand band; band.prepare(96000); band.configure(0, 1000, 4, true, 6, -18);
    for (int i = 0; i < 96000; ++i) { float l = .5f * std::sin(2 * juce::MathConstants<float>::pi * 1000 * i / 96000.f), r = l; band.process(l, r); }
    band.configure(0, 1000, 4, false, 6, -18);
    for (int i = 0; i < 96000; ++i) { float l = 0, r = 0; band.process(l, r); TEST_ASSERT(std::isfinite(l), "动态释放输出必须有限"); }
    TEST_ASSERT(band.getReduction() < .05f, "关闭后动态衰减必须释放");
    return true;
}

static bool testCharacterAndUpgradeState()
{
    std::cout << "[运行] 音色谐波、完整旁通、新增参数保存与旧状态恢复" << std::endl;
    for (int mode = 0; mode < 3; ++mode)
    {
        auto eq = std::make_unique<EqProcessor>(); eq->prepare({48000, 256, 2}); eq->setCharacterMode(mode);
        for (int i = 0; i < Params::NUM_BANDS; ++i) eq->updateBand(i, 0, false, 1000, 0, 1, false, 0, 0, 48000, false);
        juce::AudioBuffer<float> buffer(2, 256); double re = 0, im = 0; int count = 0;
        for (int b = 0; b < 150; ++b)
        {
            if (b == 100) eq->setGlobalBypass(true);
            for (int s = 0; s < 256; ++s) { const float x = .5f * static_cast<float>(std::sin(2 * juce::MathConstants<double>::pi * 937.5 * (b * 256 + s) / 48000)); buffer.setSample(0, s, x); buffer.setSample(1, s, x); }
            eq->process(buffer);
            for (int s = 0; s < 256; ++s)
            {
                const double phase = 2 * juce::MathConstants<double>::pi * 937.5 * (b * 256 + s) / 48000;
                TEST_ASSERT(std::isfinite(buffer.getSample(0, s)), "音色模式输出必须有限");
                if (b >= 50 && b < 100) { re += buffer.getSample(0, s) * std::cos(phase * (mode == 1 ? 2 : 3)); im += buffer.getSample(0, s) * std::sin(phase * (mode == 1 ? 2 : 3)); ++count; }
                if (b >= 120) TEST_ASSERT(std::abs(buffer.getSample(0, s) - .5 * std::sin(phase)) < 1e-6, "完全旁通必须移除音色处理");
            }
        }
        const double harmonic = 2 * std::hypot(re, im) / count;
        std::cout << "音色 " << mode << " 谐波幅度 " << harmonic << std::endl;
        TEST_ASSERT(mode == 0 ? harmonic < 1e-5 : harmonic > 1e-5, "音色模型必须产生可测谐波且干净模式不得染色");
    }
    auto processor = std::make_unique<SanMuQ6AudioProcessor>();
    auto legacy = processor->apvts.copyState();
    for (int i = legacy.getNumChildren(); --i >= 0;) { const auto id = legacy.getChild(i)["id"].toString(); if (id == "character_mode" || id.contains("_dyn") || id.endsWith("_dynamic")) legacy.removeChild(i, nullptr); }
    const auto set = [&](const juce::String& id, float v) { auto* p = processor->apvts.getParameter(id); p->setValueNotifyingHost(p->convertTo0to1(v)); };
    set("character_mode", 1); set("band1_dynamic", 1); set("band1_dyn_range", 11); set("band1_dyn_threshold", -33);
    juce::MemoryBlock saved; processor->getStateInformation(saved); set("character_mode", 0); set("band1_dynamic", 0);
    processor->setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
    TEST_ASSERT(processor->apvts.getRawParameterValue("band1_dynamic")->load() > .5f && processor->apvts.getRawParameterValue("character_mode")->load() > .5f, "新增状态必须正确保存恢复");
    auto xml = legacy.createXml(); xml->setAttribute("schemaVersion", "1.1"); juce::MemoryBlock oldData; juce::AudioProcessor::copyXmlToBinary(*xml, oldData);
    processor->setStateInformation(oldData.getData(), static_cast<int>(oldData.getSize()));
    TEST_ASSERT(processor->apvts.getRawParameterValue("character_mode")->load() < .5f && processor->apvts.getRawParameterValue("band1_dynamic")->load() < .5f, "旧状态必须关闭新增处理");
    TEST_ASSERT(std::abs(processor->apvts.getRawParameterValue("band1_dyn_range")->load() - 6) < .01f, "旧状态必须恢复默认动态范围");
    TEST_ASSERT(processor->apvts.getRawParameterValue("band1_dyn_auto")->load() == 0, "旧工程必须保持手动阈值");
    set("band1_dyn_auto", 1); processor->getStateInformation(saved); set("band1_dyn_auto", 0);
    processor->setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
    TEST_ASSERT(processor->apvts.getRawParameterValue("band1_dyn_auto")->load() == 1, "自动阈值状态未恢复");
    return true;
}

static bool testSpectrumUpgrade()
{
    std::cout << "[运行] 双轨频谱、低频插值与校准" << std::endl;
    for (const float frequency : {60.f, 1000.f, 10000.f})
    {
        auto analyser = std::make_unique<SpectrumAnalyser>(); analyser->prepare(48000);
        juce::AudioBuffer<float> input(2, 48000), output(2, 48000);
        for (int i = 0; i < 48000; ++i) { const float x = .1f * static_cast<float>(std::sin(2 * juce::MathConstants<double>::pi * frequency * i / 48000)); input.setSample(0, i, x); input.setSample(1, i, -x); output.setSample(0, i, x * .5f); output.setSample(1, i, -x * .5f); }
        analyser->push(input, output);
        for (int i = 0; i < 500 && analyser->sequence() < 60; ++i) juce::Thread::sleep(2);
        TEST_ASSERT(analyser->sequence() >= 60, "后台频谱必须完成采样处理");
        int peak = 0; for (int i = 1; i < SpectrumAnalyser::points; ++i) if (analyser->get(i, 0) > analyser->get(peak, 0)) peak = i;
        const float measuredFrequency = 20 * std::pow(1000.f, static_cast<float>(peak) / 499);
        std::cout << frequency << " Hz -> " << measuredFrequency << " Hz, 输入 " << analyser->get(peak, 0) << " dBFS, 输出 " << analyser->get(peak, 1) << " dBFS" << std::endl;
        TEST_ASSERT(std::abs(std::log2(measuredFrequency / frequency)) < .06, "频谱峰值频率必须准确");
        TEST_ASSERT(std::abs(analyser->get(peak, 0) + 20) < 1.8, "正弦谱峰校准偏差不得超过 1.8 dB");
        TEST_ASSERT(std::abs(analyser->get(peak, 1) - analyser->get(peak, 0) + 6.0206) < .1, "双轨必须反映实际增益差");
        if (frequency < 100) { int flat = 0; for (int i = peak - 7; i < peak + 7; ++i) if (std::abs(analyser->get(i, 0) - analyser->get(i + 1, 0)) < .00001f) ++flat; TEST_ASSERT(flat < 2, "低频峰附近不得形成重复取样平台"); }
        if (frequency == 1000) { const float before = analyser->get(peak, 0); juce::Thread::sleep(250); TEST_ASSERT(analyser->get(peak, 0) < before - 8, "宿主停播不提交采样时频谱也必须衰减"); }
    }
    return true;
}

static bool testSpectrumResolution()
{
    std::cout << "[运行] 四档分析窗口的真实分辨率与校准" << std::endl;
    int previousWidth = 500;
    for (int resolution = 0; resolution < 4; ++resolution)
    {
        auto analyser = std::make_unique<SpectrumAnalyser>(); analyser->setResolution(resolution); analyser->prepare(48000);
        juce::AudioBuffer<float> input(2, 48000), output(2, 48000);
        const float expected = resolution == 3 ? -114.f : -20.f;
        const float amplitude = juce::Decibels::decibelsToGain(expected, -150.f);
        for (int i = 0; i < 48000; ++i)
        {
            const float x = amplitude * std::sin(2 * juce::MathConstants<double>::pi * 187.5 * i / 48000);
            input.setSample(0, i, x); input.setSample(1, i, x);
            output.setSample(0, i, x * .5f); output.setSample(1, i, x * .5f);
        }
        analyser->push(input, output);
        for (int i = 0; i < 1500 && analyser->sequence() < 60; ++i) juce::Thread::sleep(2);
        TEST_ASSERT(analyser->sequence() >= 60, "不同窗口必须完成真实 FFT");
        float peak = -150; int width = 0;
        for (int i = 0; i < 500; ++i) { TEST_ASSERT(std::isfinite(analyser->get(i)), "切换分辨率后谱值无效"); peak = std::max(peak, analyser->get(i, 0)); }
        for (int i = 0; i < 500; ++i) if (analyser->get(i, 0) > peak - 12) ++width;
        std::cout << (1 << (11 + resolution)) << " 点：峰值 " << peak << " dBFS，谱峰宽度 " << width << std::endl;
        TEST_ASSERT(std::abs(peak - expected) < 1.8, "切换分辨率不应明显改变幅度校准，120 dB 范围也不能被截断");
        TEST_ASSERT(width < previousWidth, "增加窗口必须真实提高低频分辨率");
        previousWidth = width;
    }
    return true;
}

static bool testAuditionAndCharacterLevels()
{
    std::cout << "[运行] 临时监听声道路由与分级输入音色测量" << std::endl;
    for (int placement = 0; placement < 5; ++placement)
    {
        auto eq = std::make_unique<EqProcessor>(); eq->prepare({48000, 256, 2});
        for (int i = 0; i < Params::NUM_BANDS; ++i) eq->updateBand(i, 0, false, 1000, 0, 1, false, 0, placement, 48000, i == 0);
        eq->setAuditionBand(0);
        TEST_ASSERT(eq->isAnyBandSoloed(), "临时监听应允许试听已旁通频段");
        juce::AudioBuffer<float> audio(2, 256); double left = 0, right = 0;
        for (int b = 0; b < 40; ++b)
        {
            for (int s = 0; s < 256; ++s) { const float x = .2f * std::sin(2 * juce::MathConstants<double>::pi * 1000 * (b * 256 + s) / 48000); audio.setSample(0, s, x); audio.setSample(1, s, -x); }
            eq->process(audio);
            if (b > 20) for (int s = 0; s < 256; ++s) { left += std::abs(audio.getSample(0, s)); right += std::abs(audio.getSample(1, s)); }
        }
        if (placement == Params::PlacementLeft) TEST_ASSERT(left > 100 && right < .001, "左路监听泄漏到右路");
        if (placement == Params::PlacementRight) TEST_ASSERT(right > 100 && left < .001, "右路监听泄漏到左路");
        if (placement == Params::PlacementMid) TEST_ASSERT(left < .001 && right < .001, "反相信号不应出现在中置监听");
        if (placement == Params::PlacementSide || placement == Params::PlacementStereo) TEST_ASSERT(left > 100 && right > 100, "侧边或立体声监听丢失信号");
        eq->setAuditionBand(-1); TEST_ASSERT(!eq->isAnyBandSoloed(), "结束试听后仍残留独听状态");
    }
    for (const float level : {-24.f, -18.f, -12.f, -6.f, 0.f}) for (int mode = 0; mode < 3; ++mode)
    {
        auto eq = std::make_unique<EqProcessor>(); eq->prepare({48000, 256, 2}); eq->setCharacterMode(mode);
        for (int i = 0; i < Params::NUM_BANDS; ++i) eq->updateBand(i, 0, false, 1000, 0, 1, false, 0, 0, 48000, false);
        juce::AudioBuffer<float> audio(2, 256); std::array<double, 8> re{}, im{}; int count = 0;
        const float amplitude = juce::Decibels::decibelsToGain(level);
        for (int b = 0; b < 100; ++b)
        {
            for (int s = 0; s < 256; ++s) { const float x = amplitude * std::sin(2 * juce::MathConstants<double>::pi * 937.5 * (b * 256 + s) / 48000); audio.setSample(0, s, x); audio.setSample(1, s, x); }
            eq->process(audio);
            if (b >= 50) for (int s = 0; s < 256; ++s) { const float x = audio.getSample(0, s); TEST_ASSERT(std::isfinite(x), "音色扫描输出非有限"); ++count; const double phase = 2 * juce::MathConstants<double>::pi * 937.5 * (b * 256 + s) / 48000; for (int k = 0; k < 8; ++k) { re[k] += x * std::cos(phase * (k + 1)); im[k] += x * std::sin(phase * (k + 1)); } }
        }
        const double fundamental = std::hypot(re[0], im[0]); double distortion = 0;
        for (int k = 1; k < 8; ++k) distortion += re[k] * re[k] + im[k] * im[k];
        const double thd = 100 * std::sqrt(distortion) / fundamental;
        std::cout << "输入 " << level << " dBFS，模式 " << mode << "，二至八次谐波总失真 " << thd << " %，基频电平变化 " << 20 * std::log10(2 * fundamental / count / amplitude) << " dB" << std::endl;
        if (mode == 0) TEST_ASSERT(thd < .001, "干净模式不能引入谐波染色");
        const double second = std::hypot(re[1], im[1]) / fundamental;
        const double third = std::hypot(re[2], im[2]) / fundamental;
        if (mode == 1 && level <= -6) TEST_ASSERT(second > third * 1.1, "Tube 应以偶次谐波为主");
        if (mode == 2) TEST_ASSERT(third > second * 10 && thd < .5, "Discrete 应以克制的奇次谐波为主");
    }
    return true;
}
