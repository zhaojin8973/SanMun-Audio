#pragma once
#include "params/UpgradeState.h"

static void configureLinearTest(EqProcessor& eq, int block, double rate = 48000, bool dynamic = true)
{
    eq.prepare({rate, static_cast<juce::uint32>(block), 2});
    eq.setProcessingMode(1);
    for (int i = 0; i < Params::MAX_BANDS; ++i)
        eq.updateBand(i, 0, i == 0, 1500, 0, 1, false, 0, 0, rate, i == 0, i == 0 && dynamic, 6, -30);
}

static bool testLinearAtomicKernel()
{
    std::cout << "[运行] 四路滤波核整组替换与有符号核对称" << std::endl;
    for (const auto type : {TptSvf::Type::Bell, TptSvf::Type::LowShelf, TptSvf::Type::HighShelf, TptSvf::Type::LowPass, TptSvf::Type::HighPass})
    for (const float gain : {-24.f, -6.f, 0.f, 12.f, 24.f}) for (const float q : {.1f, 1.f, 4.f, 40.f})
    {
        LinearMagnitude cached; cached.prepare(type, 500, q, gain);
        for (const float frequency : {20.f, 100.f, 500.f, 1000.f, 20000.f})
        {
            const double reference = TptSvf::magnitudeDb(type, 500, q, gain, frequency);
            TEST_ASSERT(std::abs(10 * std::log10(cached.power(double(frequency) * frequency)) - reference) < 1e-4, "优化后的快照响应必须与原型等价");
        }
    }
    const auto sampler = [](float f, double, float m[4], void* context)
    {
        const float a = *static_cast<float*>(context) * (1 + .2f * std::exp(-f / 2000));
        m[0] = a; m[1] = -.2f * a; m[2] = .3f * a; m[3] = .7f * a;
    };
    auto reference = std::make_unique<LinearPhaseEq>();
    auto changing = std::make_unique<LinearPhaseEq>();
    float first = .8f, second = 1.6f;
    for (auto* engine : {reference.get(), changing.get()})
    {
        engine->prepare({48000, 64, 2}); engine->prepareIdentityKernel();
        engine->rebuildFir(sampler, &first, 48000);
    }
    changing->beginIncrementalRebuild(sampler, &second, 48000);
    double difference = 0;
    for (int block = 0; block < 160; ++block)
    {
        juce::AudioBuffer<float> a(2, 64), b(2, 64);
        for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 64; ++i)
        {
            const float x = .2f * std::sin((block * 64 + i) * (ch ? .131f : .083f));
            a.setSample(ch, i, x); b.setSample(ch, i, x);
        }
        reference->processBlock(a, 0);
        // 先完成第一条路径，故意暂停生成，使旧目标平滑跨越多个卷积帧。
        changing->processBlock(b, block < 18 ? 64 : 0);
        for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 64; ++i)
            difference = std::max(difference, double(std::abs(a.getSample(ch, i) - b.getSample(ch, i))));
    }
    TEST_ASSERT(changing->isRebuildPending() && difference < 1e-7, "半成品矩阵不得改变当前输出或平滑目标");
    for (int block = 0; block < 4; ++block)
    {
        juce::AudioBuffer<float> b(2, 64); b.clear(); changing->processBlock(b, 64);
    }
    TEST_ASSERT(!changing->isRebuildPending(), "完整四路生成后才结束重建");
    for (int path = 0; path < 4; ++path)
    {
        const auto* taps = changing->getMatrixFirTaps(path);
        for (int i = 0; i < LinearPhaseEq::FIR_M; ++i)
            TEST_ASSERT(std::abs(taps[i] - taps[LinearPhaseEq::FIR_M - 1 - i]) < 2e-7f, "四路核必须关于相同群延迟对称");
    }
    TEST_ASSERT(changing->getMatrixFirTaps(1)[LinearPhaseEq::GROUP_DELAY] < 0, "交叉路径负号不得被幅度绝对值吞掉");
    const auto largeGain = [](float, double, float m[4], void*) { m[0] = m[3] = 256; m[1] = m[2] = 0; };
    changing->rebuildFir(largeGain, nullptr, 48000);
    TEST_ASSERT(std::abs(changing->getFirTaps()[LinearPhaseEq::GROUP_DELAY] - 256) < .001f,
                "线性模式不能把正常叠加增益静默截断到 36 dB");
    return true;
}

static bool testLinearTimelineAndHistory()
{
    std::cout << "[运行] 线性相位时间轴、突发动态、旁通和监听恢复" << std::endl;
    constexpr int length = 120000;
    std::vector<float> reference;
    for (const int blockSize : {64, 257, 1024})
    {
        auto eq = std::make_unique<EqProcessor>(); configureLinearTest(*eq, blockSize);
        std::vector<float> output(length);
        juce::AudioBuffer<float> block(2, blockSize);
        for (int offset = 0; offset < length; offset += blockSize)
        {
            const int count = std::min(blockSize, length - offset); block.setSize(2, count, false, false, true);
            for (int i = 0; i < count; ++i)
            {
                const int t = offset + i;
                const float amplitude = t < 24000 || t > 64000 ? .008f : .5f;
                const float x = amplitude * std::sin(2 * juce::MathConstants<double>::pi * 1500 * t / 48000);
                block.setSample(0, i, x); block.setSample(1, i, x);
            }
            eq->process(block);
            std::copy_n(block.getReadPointer(0), count, output.begin() + offset);
        }
        if (reference.empty()) reference = output;
        else
        {
            double error = 0;
            for (int i = 0; i < length; ++i) error = std::max(error, double(std::abs(output[i] - reference[i])));
            std::cout << "  块长 " << blockSize << "，相对 64 样本逐样本最大误差 " << error << std::endl;
            TEST_ASSERT(error < 2e-6, "动态响应不得随宿主块长改变");
        }
        std::cout << "  尾部实际衰减 " << eq->getAppliedDynamicReduction(0) << " dB" << std::endl;
        TEST_ASSERT(eq->getAppliedDynamicReduction(0) < .4f, "信号退回阈值下方后实际衰减应释放");
    }
    for (const bool audition : {false, true})
    {
        auto a = std::make_unique<EqProcessor>(), b = std::make_unique<EqProcessor>();
        configureLinearTest(*a, 64); configureLinearTest(*b, 64);
        double error = 0;
        for (int offset = 0; offset < 70000; offset += 64)
        {
            if (offset == 19200) { if (audition) b->setAuditionBand(0); else b->setGlobalBypass(true); }
            if (offset == 38400) { if (audition) b->setAuditionBand(-1); else b->setGlobalBypass(false); }
            juce::AudioBuffer<float> x(2, 64), y(2, 64);
            for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 64; ++i)
            {
                const float v = .2f * std::sin((offset + i) * .19634954f) * (1 + .5f * std::sin((offset + i) * .0007f));
                x.setSample(ch, i, v); y.setSample(ch, i, v);
            }
            a->process(x); b->process(y);
            if (offset >= 40000) for (int i = 0; i < 64; ++i)
                error = std::max(error, double(std::abs(x.getSample(0, i) - y.getSample(0, i))));
        }
        TEST_ASSERT(error < 2e-6, "解除旁通或临时监听后不得出现滞留音频");
    }
    return true;
}

static bool testLinearStateCompatibility()
{
    std::cout << "[运行] 旧工程线性动态保护、显式升级、撤销和重开" << std::endl;
    auto p = std::make_unique<SanMuQ6AudioProcessor>();
    const auto set = [&](const juce::String& id, float value)
    {
        auto* param = p->apvts.getParameter(id); param->setValueNotifyingHost(param->convertTo0to1(value));
    };
    set(Params::IDs::EQ_MODE, 1);
    for (int n = 1; n <= 24; ++n) { set(Params::IDs::bandPresent(n), n == 1); set(Params::IDs::bandEnabled(n), n == 1); }
    set(Params::IDs::bandType(1), 0); set(Params::IDs::bandFreq(1), 1500); set(Params::IDs::bandGain(1), 0);
    set(Params::IDs::bandQ(1), 1); set(Params::IDs::bandDynamic(1), 1); set(Params::IDs::bandDynThreshold(1), -30);
    set(Params::IDs::bandDynAuto(1), 0); set(Params::IDs::bandDynRange(1), 6);
    auto old = p->apvts.copyState(); old.removeProperty(Params::LINEAR_VERSION, nullptr); old.removeProperty(Params::LINEAR_LEGACY_MASK, nullptr);
    juce::MemoryBlock data; juce::AudioProcessor::copyXmlToBinary(*old.createXml(), data);
    p->setStateInformation(data.getData(), static_cast<int>(data.getSize()));
    TEST_ASSERT(!p->isLinearDynamicAllowed(0), "旧状态必须保留线性动态暂停行为");
    TEST_ASSERT(p->apvts.getRawParameterValue(Params::IDs::bandDynamic(1))->load() == 1, "不得覆盖旧工程在其它模式使用的动态参数");
    p->setRateAndBufferSizeDetails(48000, 64);
    p->prepareToPlay(48000, 64);
    const auto render = [&]()
    {
        juce::AudioBuffer<float> block(2, 64); juce::MidiBuffer midi;
        for (int offset = 0; offset < 48000; offset += 64)
        {
            for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 64; ++i)
                block.setSample(ch, i, .5f * std::sin(2 * juce::MathConstants<double>::pi * 1500 * (offset + i) / 48000));
            p->processBlock(block, midi);
        }
        return p->getEqProcessor().getAppliedDynamicReduction(0);
    };
    TEST_ASSERT(render() < .01f, "旧工程加载后不能自动引入新压缩");
    set(Params::IDs::EQ_MODE, 0);
    TEST_ASSERT(render() > 5.8f, "旧工程在 Clean 模式必须继续使用原动态参数");
    set(Params::IDs::EQ_MODE, 1); p->apvts.copyState(); p->undoManager.clearUndoHistory();
    p->undoManager.beginNewTransaction(); p->enableLinearDynamicBand(0);
    TEST_ASSERT(p->isLinearDynamicAllowed(0) && render() > 5.8f, "明确启用后线性动态必须实际工作");
    p->undoManager.undo(); TEST_ASSERT(!p->isLinearDynamicAllowed(0), "撤销必须恢复旧工程保护");
    p->undoManager.redo(); TEST_ASSERT(p->isLinearDynamicAllowed(0), "重做必须恢复升级状态");
    p->getStateInformation(data);
    auto restored = std::make_unique<SanMuQ6AudioProcessor>(); restored->setStateInformation(data.getData(), static_cast<int>(data.getSize()));
    TEST_ASSERT(restored->isLinearDynamicAllowed(0) && !restored->isLinearDynamicAllowed(1), "重开必须逐频段保存升级状态");
    p->prepareToPlay(48000, 64);
    TEST_ASSERT(p->getLatencySamples() == 5120 && render() > 5.8f, "重复初始化必须保留线性相位延迟和检测器配置");
    return true;
}

static bool testLinearShapesAndRates()
{
    std::cout << "[运行] 线性动态五种声道位置、三种滤波形状和自动阈值" << std::endl;
    for (const double rate : {44100., 48000., 96000., 192000.})
    for (int type = 0; type < 3; ++type) for (const bool automatic : {false, true})
    {
        const int blockSize = rate == 44100 ? 128 : rate == 48000 ? 257 : rate == 96000 ? 512 : 1024;
        const int placement = (type + (automatic ? 3 : 0)) % 5;
        const double signal = type == 1 ? 187.5 : type == 2 ? 6000. : 1500.;
        auto eq = std::make_unique<EqProcessor>(); eq->prepare({rate, static_cast<juce::uint32>(blockSize), 2}); eq->setProcessingMode(1);
        for (int i = 0; i < 24; ++i)
            eq->updateBand(i, type, i == 0, 1500, 0, 1, false, 0, placement, rate, i == 0, i == 0, 6, -30, automatic);
        juce::AudioBuffer<float> block(2, blockSize);
        double in = 0, left = 0, right = 0;
        for (int offset = 0; offset < static_cast<int>(rate * 1.5); offset += blockSize)
        {
            for (int s = 0; s < blockSize; ++s)
            {
                const float x = .5f * std::sin(2 * juce::MathConstants<double>::pi * signal * (offset + s) / rate);
                block.setSample(0, s, x); block.setSample(1, s, placement == 4 ? -x : x);
                if (offset > rate) in += x * x;
            }
            eq->process(block);
            if (offset > rate) for (int s = 0; s < blockSize; ++s)
            { left += std::pow(block.getSample(0, s), 2); right += std::pow(block.getSample(1, s), 2); }
        }
        const double l = 10 * std::log10(left / in), r = 10 * std::log10(right / in);
        TEST_ASSERT(eq->getAppliedDynamicReduction(0) > (automatic ? .02 : 5.7), "自动或手动阈值必须产生实际应用衰减");
        TEST_ASSERT((placement == 2 ? r : l) < (automatic ? -.02 : -5.5), "三种滤波形状必须实际改变对应声道音频");
        if (automatic) TEST_ASSERT(eq->getAppliedDynamicReduction(0) <= 2.101f, "AUTO 动态衰减不能超过温和上限");
        if (placement == 1 || placement == 2)
            TEST_ASSERT(std::abs(placement == 1 ? r : l) < .05, "单侧动态不得改变另一侧电平");
        else TEST_ASSERT(std::abs(l - r) < .05, "立体声或中侧处理必须保持对应对称性");
    }
    return true;
}

static bool measureLinearLowFrequency()
{
    std::cout << "[测量] 固定 8193 抽头低频窄带中心增益（目标 +12 dB）" << std::endl;
    struct Shape { float frequency, q; };
    const auto sample = [](float f, double, float matrix[4], void* context)
    {
        const auto& shape = *static_cast<Shape*>(context);
        matrix[0] = matrix[3] = std::pow(10., TptSvf::magnitudeDb(TptSvf::Type::Bell, shape.frequency, shape.q, 12, f) / 20.);
        matrix[1] = matrix[2] = 0;
    };
    for (const double rate : {44100., 48000., 96000., 192000.})
    {
        auto engine = std::make_unique<LinearPhaseEq>(); engine->prepare({rate, 64, 2});
        for (const float f : {20.f, 40.f, 80.f, 100.f, 1500.f}) for (const float q : {1.f, 4.f, 40.f})
        {
            Shape shape{f, q}; engine->rebuildFir(sample, &shape, rate);
            const auto* taps = engine->getFirTaps(); double re = 0, im = 0;
            for (int i = 0; i < LinearPhaseEq::FIR_M; ++i)
            {
                const double phase = 2 * juce::MathConstants<double>::pi * f * i / rate;
                re += taps[i] * std::cos(phase); im -= taps[i] * std::sin(phase);
            }
            std::cout << rate << " Hz, " << f << " Hz, Q=" << q << ": " << 20 * std::log10(std::hypot(re, im)) << " dB" << std::endl;
        }
    }
    return true;
}
