#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ui/InputLevelMeter.h"
#include "presets/PresetManager.h"
#include <iostream>
#include <stdexcept>
#include <thread>
#include <limits>
#include <type_traits>
#include <fstream>

struct InputLevelMeterTestAccess
{
    static std::array<float, 3> tick(InputLevelMeter& meter, double nowMs)
    {
        meter.stopTimer();
        meter.updateDisplay(nowMs);
        const auto index = size_t((meter.writeIndex - 1 + meter.kHistorySize) % meter.kHistorySize);
        return {meter.inputHistory[index], meter.outputHistory[index], meter.grHistory[index]};
    }
};

static void check(bool ok, const char* message)
{
    if (!ok) throw std::runtime_error(message);
}

static void set(SanMuCompAudioProcessor& p, const char* id, float value)
{
    auto* parameter = p.apvts.getParameter(id);
    check(parameter != nullptr, "参数不存在");
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

static float get(SanMuCompAudioProcessor& p, const char* id)
{
    return p.apvts.getRawParameterValue(id)->load();
}

static void stateAndRouting()
{
    SanMuCompAudioProcessor p;
    set(p, Params::IDs::FET_MIX, 31);
    set(p, Params::IDs::OPTO_MIX, 72);
    set(p, Params::IDs::VCA_MIX, 43);
    set(p, Params::IDs::VOX_MIX, 54);
    p.clickB();
    p.clickA();
    check(get(p, Params::IDs::FET_MIX) == 31, "首次切 B 丢失 A");

    auto preset = PresetManager::getDefaultPresetForMode(Params::ModeOpto);
    PresetManager::applyPreset(preset, p.apvts);
    check(get(p, Params::IDs::FET_MIX) == 31 && get(p, Params::IDs::VCA_MIX) == 43
          && get(p, Params::IDs::VOX_MIX) == 54, "工厂预设串改其他模式");
    ai::CompressorPresetRecommendation recommendation;
    recommendation.mode = Params::ModeFET;
    recommendation.fetMix = 23;
    p.applyPresetRecommendation(recommendation);
    check(get(p, Params::IDs::VCA_MIX) == 43 && get(p, Params::IDs::VOX_MIX) == 54,
          "推荐串改其他模式");

    auto old = p.apvts.copyState();
    old.removeChild(old.getChildWithProperty("id", Params::IDs::FET_MIX), nullptr);
    old.removeChild(old.getChildWithProperty("id", Params::IDs::OPTO_MIX), nullptr);
    old.getChildWithProperty("id", Params::IDs::MIX).setProperty("value", 27.0f, nullptr);
    juce::MemoryBlock state;
    juce::AudioProcessor::copyXmlToBinary(*old.createXml(), state);
    p.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    check(get(p, Params::IDs::FET_MIX) == 27 && get(p, Params::IDs::OPTO_MIX) == 27,
          "旧通用混合比例未迁移");
    check(get(p, Params::IDs::VCA_MIX) == 43 && get(p, Params::IDs::VOX_MIX) == 54,
          "迁移覆盖已有独立参数");
    p.setRateAndBufferSizeDetails(48000, 257);
    p.prepareToPlay(48000, 257);
    juce::AudioBuffer<float> block(2, 257);
    juce::MidiBuffer midi;
    for (int mode = 0; mode < 4; ++mode)
    {
        set(p, Params::IDs::MODE, static_cast<float>(mode));
        block.clear();
        p.processBlock(block, midi);
        check(p.getLatencySamples() == 0, "模式切换改变零延迟上报");
    }
    check(p.getCompressor().getFetEngine().mixPercent.load() == 27
          && p.getCompressor().getVcaEngine().mixPercent.load() == 43
          && p.getCompressor().getOptoEngine().mixPercent.load() == 27, "模式路由串写");
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    std::thread automation([&] { for (int i = 0; i < 30; ++i) set(p, Params::IDs::MODE, float(i % 4)); });
    automation.join();
    editor.reset();
    juce::Timer::callPendingTimersSynchronously();
    std::cout << "状态迁移、预设隔离、推荐隔离、参数回调关闭：通过\n";
}

static void bypassAndMeters()
{
    for (double sr : {44100.0, 48000.0, 96000.0, 192000.0})
    for (int channels : {1, 2})
    for (int mode = 0; mode < 4; ++mode)
    for (bool hostBypass : {false, true})
    for (bool smooth : {false, true})
    {
        const int delay = mode == Params::ModeVOX ? dsp_engines::VoxEngine::getLatencySamples(sr, smooth) : 0;
        const int blockSize = std::max(769, delay + 257);
        SanMuCompAudioProcessor p;
        p.setPlayConfigDetails(channels, channels, sr, blockSize);
        set(p, Params::IDs::MODE, float(mode));
        set(p, Params::IDs::VOX_SMOOTH, smooth ? 1.f : 0.f);
        set(p, Params::IDs::GLOBAL_BYPASS, hostBypass ? 0.0f : 1.0f);
        p.prepareToPlay(sr, blockSize);
        juce::AudioBuffer<float> block(channels, blockSize);
        juce::MidiBuffer midi;
        block.clear();
        for (int c = 0; c < channels; ++c) block.setSample(c, 0, 0.75f);
        if (hostBypass) p.processBlockBypassed(block, midi); else p.processBlock(block, midi);
        check(p.getLatencySamples() == delay, "旁通宿主延迟错误");
        for (int c = 0; c < channels; ++c)
            for (int n = 0; n < block.getNumSamples(); ++n)
                check(block.getSample(c, n) == (n == delay ? 0.75f : 0.0f), "旁通音频延迟不匹配");
        block.clear();
        p.processBlockBypassed(block, midi);
        auto peaks = p.getCompressor().consumeMeterPeaks();
        check(peaks[0] > -3.0f && peaks[1] > -3.0f, "刷新间峰值被静音块覆盖");
    }
    std::cout << "单声道/立体声、四模式双旁通、峰值保持：通过\n";
    SanMuCompAudioProcessor expanded;
    expanded.setPlayConfigDetails(1, 2, 48000, 257);
    set(expanded, Params::IDs::MODE, float(Params::ModeFET));
    expanded.prepareToPlay(48000, 257);
    juce::AudioBuffer<float> block(2, 257);
    juce::MidiBuffer midi;
    block.clear();
    block.setSample(0, 0, 0.5f);
    block.setSample(0, 1, std::numeric_limits<float>::quiet_NaN());
    block.setSample(0, 2, std::numeric_limits<float>::infinity());
    expanded.processBlockBypassed(block, midi);
    check(block.getSample(1, 0) == 0.5f, "单声道扩展缺少右声道");
    for (int c = 0; c < 2; ++c)
        for (int n = 0; n < 257; ++n)
            check(std::isfinite(block.getSample(c, n)), "异常浮点未隔离");
}

static void meterCadence(const juce::File& evidence = {})
{
    int gaps = 0;
    int frames = 0;
    for (double sr : {44100.0, 48000.0, 96000.0, 192000.0})
    for (int blockSize : {64, 256, 512, 1024, 2048, 4096})
    for (int channels : {1, 2})
    for (int mode = 0; mode < 4; ++mode)
    {
        SanMuCompAudioProcessor p;
        p.setPlayConfigDetails(channels, channels, sr, blockSize);
        set(p, Params::IDs::MODE, float(mode));
        p.prepareToPlay(sr, blockSize);
        InputLevelMeter meter(p.getCompressor());
        meter.setSize(1280, 360);
        juce::AudioBuffer<float> block(channels, blockSize);
        juce::MidiBuffer midi;
        const bool capture = evidence != juce::File() && sr == 48000 && blockSize == 1024
                             && channels == 2 && mode == Params::ModeVCA;
        std::ofstream csv;
        if (capture)
        {
            evidence.createDirectory();
            csv.open(evidence.getChildFile("meter-cadence.csv").getFullPathName().toStdString());
            csv << "time_ms,input_db,output_db,gr_db\n";
        }
        int64_t samples = 0;
        for (int frame = 0; frame < (capture ? 180 : 30); ++frame)
        {
            const double now = frame * (1000.0 / 60.0);
            while (samples * 1000.0 / sr <= now)
            {
                for (int n = 0; n < blockSize; ++n)
                    for (int c = 0; c < channels; ++c)
                        block.setSample(c, n, .25f * std::sin(float(2.0 * juce::MathConstants<double>::pi * 1000 * (samples + n) / sr)));
                p.processBlock(block, midi);
                samples += blockSize;
            }
            const auto value = InputLevelMeterTestAccess::tick(meter, now);
            if (value[0] < -90 || value[1] < -90) ++gaps;
            ++frames;
            if (capture) csv << now << ',' << value[0] << ',' << value[1] << ',' << value[2] << '\n';
        }
        if (capture)
        {
            auto stream = evidence.getChildFile("meter-cadence.png").createOutputStream();
            juce::PNGImageFormat().writeImageToStream(meter.createComponentSnapshot(meter.getLocalBounds()), *stream);
        }
    }
    std::cout << "持续正弦、四模式、单/双声道、四采样率、六块大小：" << frames << " 帧，假静音 " << gaps << " 帧\n";
    check(gaps == 0, "持续声音被曲线错误绘制成静音");

    SanMuCompAudioProcessor p;
    p.setPlayConfigDetails(2, 2, 48000, 1024);
    set(p, Params::IDs::MODE, float(Params::ModeVCA));
    set(p, Params::IDs::VCA_THRESHOLD, -40);
    set(p, Params::IDs::VCA_RATIO, 10);
    p.prepareToPlay(48000, 1024);
    InputLevelMeter meter(p.getCompressor());
    juce::MidiBuffer midi;
    juce::AudioBuffer<float> block(2, 1024);
    for (int b = 0; b < 40; ++b)
    {
        for (int c = 0; c < 2; ++c)
            for (int n = 0; n < 1024; ++n) block.setSample(c, n, .5f);
        p.processBlock(block, midi);
    }
    const auto sounding = InputLevelMeterTestAccess::tick(meter, 1000);
    check(sounding[2] > 6, "电平回归信号未产生足够压缩");
    check(InputLevelMeterTestAccess::tick(meter, 1016) == sounding, "块间输入、输出或压缩量未保持");
    juce::AudioBuffer<float> empty(2, 0);
    p.processBlock(empty, midi);
    check(InputLevelMeterTestAccess::tick(meter, 1033) == sounding, "空音频块被当作真实静音");
    block.clear();
    p.processBlockBypassed(block, midi);
    const auto silent = InputLevelMeterTestAccess::tick(meter, 1050);
    check(silent[0] <= -100 && silent[1] <= -100 && silent[2] == 0, "真实静音或旁通零压缩被错误保持");
    for (int c = 0; c < 2; ++c)
        for (int n = 0; n < 1024; ++n) block.setSample(c, n, .5f);
    p.processBlock(block, midi);
    InputLevelMeterTestAccess::tick(meter, 1100);
    const auto idle = InputLevelMeterTestAccess::tick(meter, 1500);
    check(idle[0] <= -100 && idle[1] <= -100 && idle[2] == 0, "宿主停调后电平永久冻结");
    // 即使宿主的实际块超过准备时大小，也不能在下一块到来前误报静音。
    juce::AudioBuffer<float> large(2, 16384);
    for (int c = 0; c < 2; ++c)
        for (int n = 0; n < large.getNumSamples(); ++n) large.setSample(c, n, .25f);
    p.processBlockBypassed(large, midi);
    const auto largeValue = InputLevelMeterTestAccess::tick(meter, 2000);
    check(InputLevelMeterTestAccess::tick(meter, 2300) == largeValue, "超大块刷新间隔未纳入停调判定");
    const auto largeIdle = InputLevelMeterTestAccess::tick(meter, 3000);
    check(largeIdle[0] <= -100 && largeIdle[1] <= -100, "超大块停调后电平不回落");
    p.releaseResources();
    const auto reset = InputLevelMeterTestAccess::tick(meter, 3100);
    check(reset[0] <= -100 && reset[1] <= -100 && reset[2] == 0, "重置未清除显示值");
    std::cout << "块间保持、真实静音、旁通、空块、停调、超大块及重置：通过\n";
}

static void featuresAndCalibration()
{
    ai::AcousticFeatureExtractor extractor;
    extractor.prepare(48000, 257);
    juce::AudioBuffer<float> signal(2, 48000);
    for (int n = 0; n < signal.getNumSamples(); ++n)
    {
        const float x = 0.25f * std::sin(float(2.0 * juce::MathConstants<double>::pi * 1000 * n / 48000));
        signal.setSample(0, n, x);
        signal.setSample(1, n, -x);
    }
    extractor.startListening(1);
    extractor.processBlock(signal);
    extractor.computeFeatures();
    auto feature = extractor.getLatestFeatures();
    check(std::abs(feature.peakDb + 12.0412f) < 0.02f, "反相信号峰值错误");
    check(std::abs(feature.rmsDb + 15.0515f) < 0.05f, "反相信号能量错误");
    check(feature.midEnergyPct > 99 && !feature.isHighConfidence, "反相信号频谱或置信声明错误");
    for (int mode = 0; mode < 4; ++mode)
    {
        ai::CompressorPresetRecommendation rec;
        rec.mode = mode;
        extractor.calibrateRecommendationWithVirtualDsp(rec);
        std::cout << "模式 " << mode << " 校准输出 " << rec.calibratedPeakDb
                  << "，目标 " << rec.calibrationTargetDb << '\n';
        check(std::isfinite(rec.calibratedPeakDb), "校准非有限输出");
        check(rec.calibrationWithinRange, "可达有效电平校准未收敛");
    }
    ai::CompressorPresetRecommendation smoothRecommendation;
    smoothRecommendation.mode = Params::ModeVOX; smoothRecommendation.voxSmooth = true;
    extractor.calibrateRecommendationWithVirtualDsp(smoothRecommendation);
    check(smoothRecommendation.calibrationWithinRange, "LEV 平滑推荐没有按实际延迟对齐电平校准");
    int callbacks = 0;
    extractor.onAnalysisCompleted = [&](const auto&) { ++callbacks; };
    extractor.startListening();
    for (int second = 0; second < 4; ++second) extractor.processBlock(signal);
    check(extractor.isListening() && std::abs(extractor.getRecordedSeconds() - 4.0) < 0.001,
          "默认采集时长不足五秒");
    extractor.processBlock(signal);
    check(!extractor.isListening() && extractor.getRecordedSeconds() == 5.0, "五秒未结束");
    extractor.computeFeatures();
    check(callbacks == 1, "完成回调次数错误");
    extractor.startListening();
    extractor.stopListeningAndCompute();
    check(callbacks == 2 && extractor.getLatestFeatures().peakDb < -100, "空采集未结束");
    {
        auto pending = std::make_unique<ai::AcousticFeatureExtractor>();
        pending->prepare(48000, 512);
        pending->startListening(1);
        pending->processBlock(signal);
        pending->onAnalysisCompleted = [&](const auto&) { ++callbacks; };
    }
    juce::Thread::sleep(40);
    juce::Timer::callPendingTimersSynchronously();
    check(callbacks == 2, "销毁后仍触发分析回调");
    std::cout << "反相立体声、五秒采集、四模式校准、空采集：通过\n";
}

static void limiterBoundary()
{
    for (double sr : {44100.0, 48000.0, 96000.0, 192000.0})
    {
        dsp_engines::VoxEngine engine;
        engine.setGain(-6.0f);
        engine.setGate(-80.0f);
        engine.setComp(-36.0f);
        engine.prepare({sr, 512, 2});
        juce::AudioBuffer<float> block(2, 4096);
        block.clear();
        block.setSample(0, 1024, 4.0f);
        block.setSample(1, 1024, -4.0f);
        engine.process(block);
        const float peak = block.getMagnitude(0, block.getNumSamples());
        std::cout << "采样率 " << sr << " 限幅脉冲峰值 " << peak << '\n';
        check(peak <= std::pow(10.0f, -6.0f / 20.0f) + 1e-5f, "限幅器超过采样峰值天花板");
    }
}

static void feedbackCurves()
{
    for (double sr : {44100.0, 96000.0, 192000.0})
    {
        float previousFet = -1.0f, previousOpto = -1.0f;
        for (float inputDb : {-36.0f, -24.0f, -12.0f, 0.0f})
        {
            dsp_engines::FetEngine fet;
            dsp_engines::OptoEngine opto, limit;
            fet.prepare({sr, 257, 1}); opto.prepare({sr, 257, 1}); limit.prepare({sr, 257, 1});
            fet.fetDrive = 18; fet.makeupDb = 24; fet.sidechainHpHz = 0; fet.headroomDb = 16;
            opto.optoPeakReduction = 60; opto.makeupDb = 40; opto.sidechainHpHz = 0;
            limit.optoPeakReduction = 60; limit.makeupDb = 40; limit.sidechainHpHz = 0; limit.isLimit = true;
            // 直流排除过零纹波，用于核对反馈平衡方程；音色与频率另由正弦测试覆盖。
            juce::AudioBuffer<float> block(1, 257);
            const float amplitude = std::pow(10.0f, inputDb / 20.0f);
            for (int offset = 0; offset < int(sr * 2); offset += 257)
            {
                for (auto* engine : {static_cast<dsp_engines::CompressorEngine*>(&fet),
                                     static_cast<dsp_engines::CompressorEngine*>(&opto),
                                     static_cast<dsp_engines::CompressorEngine*>(&limit)})
                {
                    for (int n = 0; n < 257; ++n) block.setSample(0, n, amplitude);
                    engine->process(block);
                    check(std::isfinite(block.getMagnitude(0, 257)), "反馈输出非有限");
                }
            }
            const float f = fet.getCurrentGrDb(), o = opto.getCurrentGrDb(), l = limit.getCurrentGrDb();
            check(f >= previousFet - 0.1f && o >= previousOpto - 0.1f, "反馈压缩量不单调");
            check(l >= o - 0.1f && l < 60, "限制模式强度或稳定性错误");
            if (inputDb >= -12)
            {
                const float theoretical = (inputDb + 20.0f - 0.5f) * 0.75f;
                check(std::abs(f - theoretical) < 1.0f, "FET 反馈四比一静态曲线错误");
            }
            std::cout << "反馈曲线 " << sr << " Hz，输入 " << inputDb << " dB，衰减 "
                      << f << " / " << o << " / " << l << '\n';
            previousFet = f; previousOpto = o;
        }
    }
}

template <typename Engine>
static void partitionCheck(bool smooth = false)
{
    Engine whole, split;
    if constexpr (std::is_same_v<Engine, dsp_engines::VoxEngine>)
        for (auto* engine : {&whole, &split})
        {
            engine->setComp(-24); engine->setGate(-40); engine->setGain(-12); engine->setMix(63);
            engine->setSmooth(smooth);
        }
    whole.prepare({96000, 512, 2});
    split.prepare({96000, 512, 2});
    juce::AudioBuffer<float> a(2, 96000), b(2, 96000);
    for (int n = 0; n < 96000; ++n)
        for (int c = 0; c < 2; ++c)
            a.setSample(c, n, 0.6f * std::sin(float(n * (c + 1) * 0.03)));
    b.makeCopyOf(a);
    whole.process(a);
    for (int offset = 0; offset < 96000; offset += 257)
    {
        juce::AudioBuffer<float> part(b.getArrayOfWritePointers(), 2, offset, std::min(257, 96000 - offset));
        split.process(part);
    }
    for (int c = 0; c < 2; ++c)
        for (int n = 0; n < 96000; ++n)
            check(std::isfinite(a.getSample(c, n)) && std::abs(a.getSample(c, n) - b.getSample(c, n)) < 1e-6f,
                  "引擎分块结果不一致");
}

static void smoothStateAndUi(const juce::File& screenshots = {})
{
    SanMuCompAudioProcessor p;
    set(p, Params::IDs::MODE, Params::ModeVOX);
    check(get(p, Params::IDs::VOX_SMOOTH) == 0, "平滑模式默认值不是关闭");
    set(p, Params::IDs::VOX_SMOOTH, 1);
    juce::MemoryBlock saved;
    p.getStateInformation(saved);
    set(p, Params::IDs::VOX_SMOOTH, 0);
    p.setStateInformation(saved.getData(), int(saved.getSize()));
    check(get(p, Params::IDs::VOX_SMOOTH) == 1, "工程未保存平滑开关");
    auto preset = PresetManager::snapshotFromApvts(p.apvts);
    check(preset.voxSmooth, "预设快照遗漏平滑开关");
    set(p, Params::IDs::VOX_SMOOTH, 0);
    PresetManager::applyPreset(preset,p.apvts);
    check(get(p, Params::IDs::VOX_SMOOTH) == 1, "预设恢复遗漏平滑开关");
    PresetManager::applyPreset(PresetManager::getDefaultPresetForMode(Params::ModeFET),p.apvts);
    check(get(p, Params::IDs::VOX_SMOOTH) == 1, "其他模式预设覆盖 LEV 平滑状态");
    p.clickB(); p.clickA();
    check(get(p, Params::IDs::VOX_SMOOTH) == 1, "A/B 切换丢失平滑状态");
    auto legacy=p.apvts.copyState();
    legacy.removeChild(legacy.getChildWithProperty("id",Params::IDs::VOX_SMOOTH),nullptr);
    juce::MemoryBlock old;
    juce::AudioProcessor::copyXmlToBinary(*legacy.createXml(),old);
    p.setStateInformation(old.getData(),int(old.getSize()));
    check(get(p, Params::IDs::VOX_SMOOTH) == 0, "旧工程继承了当前平滑状态");

    p.setPlayConfigDetails(2,2,48000,257); p.prepareToPlay(48000,257);
    juce::AudioBuffer<float> b(2,257); juce::MidiBuffer midi;
    for(int mode:{Params::ModeVOX,Params::ModeFET,Params::ModeVOX})
    for(bool smooth:{true,false})
    {
        set(p,Params::IDs::MODE,float(mode));set(p,Params::IDs::VOX_SMOOTH,smooth?1.f:0.f);
        b.clear();p.processBlock(b,midi);
        check(p.getLatencySamples()==(mode==Params::ModeVOX&&smooth?576:0),"开关或模式切换未更新宿主延迟");
    }
    set(p,Params::IDs::MODE,Params::ModeVOX);
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    auto* web=dynamic_cast<SanMuCompAudioProcessorEditor*>(editor.get());
    check(web!=nullptr,"编辑器参数桥接缺失");
    for(bool enabled:{false,true})
    {
        juce::var data=new juce::DynamicObject();data.getDynamicObject()->setProperty("id","vox_smooth");
        data.getDynamicObject()->setProperty("value",enabled?1:0);
        web->handleCommand("begin");web->handleCommand("set",data);web->handleCommand("end");
        check(get(p,Params::IDs::VOX_SMOOTH)==(enabled?1.f:0.f),"平滑开关没有写入参数");
    }
    set(p,Params::IDs::VOX_SMOOTH,0);
    check(float(web->handleCommand("state")["values"]["vox_smooth"])==0,"宿主自动化没有同步界面状态");
    juce::ignoreUnused(screenshots);
    std::cout << "平滑开关：工程兼容、预设隔离、A/B、宿主延迟与网页桥接通过\n";
}

static void listeningAudit(bool verify)
{
    constexpr double sr = 48000;
    constexpr int blockSize = 256;
    juce::MidiBuffer midi;
    for (float inputDb : {-36.f, -24.f, -18.f, -12.f})
    for (float comp : {0.f, -12.f, -24.f, -36.f})
    {
        SanMuCompAudioProcessor p;
        p.setRateAndBufferSizeDetails(sr, blockSize); p.prepareToPlay(sr, blockSize);
        set(p, Params::IDs::MODE, Params::ModeVOX); set(p, Params::IDs::VOX_COMP, comp);
        set(p, Params::IDs::VOX_GAIN, -24); set(p, Params::IDs::VOX_GATE, -80);
        juce::AudioBuffer<float> b(2, blockSize);
        for (int offset = 0; offset < int(sr); offset += blockSize)
        {
            for (int n = 0; n < blockSize; ++n)
                for (int c = 0; c < 2; ++c)
                    b.setSample(c, n, std::pow(10.f, inputDb / 20) * float(std::sin(2 * juce::MathConstants<double>::pi * 1000 * (n + offset) / sr)));
            p.processBlock(b, midi);
        }
        const auto meters = p.getCompressor().consumeMeterPeaks();
        std::cout << "LEV 输入=" << inputDb << " COMP=" << comp << " 表头GR=" << meters[2] << '\n';
        if (verify && inputDb >= -24 && comp == -36)
            check(meters[2] > 8, "LEV COMP 或 GR 表头链路失效");
    }
    for (int material = 0; material < 3; ++material)
    for (int mode = 0; mode < 4; ++mode)
    {
        ai::AcousticFeatureExtractor extractor; extractor.prepare(sr, blockSize); extractor.startListening(6);
        juce::AudioBuffer<float> input(2, int(sr * 6));
        for (int n = 0; n < input.getNumSamples(); ++n)
        {
            const double t = n / sr;
            const double envelope = material == 0 ? 0.15 : material == 1 ?
                0.35 * std::exp(-std::fmod(t, 0.5) / 0.04) + 0.02 : (t < 3 ? 0.025 : 0.25);
            const float value = float(envelope * (std::sin(2 * juce::MathConstants<double>::pi * 170 * t)
                                                   + 0.3 * std::sin(2 * juce::MathConstants<double>::pi * 2300 * t)));
            input.setSample(0, n, value); input.setSample(1, n, -value);
        }
        for (int offset = 0; offset < input.getNumSamples(); offset += blockSize)
        {
            const int count = std::min(blockSize, input.getNumSamples() - offset);
            float* pointers[] = {input.getWritePointer(0, offset), input.getWritePointer(1, offset)};
            juce::AudioBuffer<float> b(pointers, 2, count); extractor.processBlock(b);
        }
        extractor.computeFeatures();
        ai::CompressorPresetRecommendation rec;
        rec.mode = mode; rec.voxComp = -30; rec.voxGate = -80; rec.fetInput = 24;
        extractor.calibrateRecommendationWithVirtualDsp(rec);
        SanMuCompAudioProcessor p;
        p.setRateAndBufferSizeDetails(sr, blockSize); p.prepareToPlay(sr, blockSize); p.applyPresetRecommendation(rec);
        double inputEnergy = 0, outputEnergy = 0, peak = 0;
        juce::AudioBuffer<float> b(2, blockSize);
        for (int offset = 0; offset < input.getNumSamples(); offset += blockSize)
        {
            const int count = std::min(blockSize, input.getNumSamples() - offset);
            for (int c = 0; c < 2; ++c) b.copyFrom(c, 0, input, c, offset, count);
            juce::AudioBuffer<float> view(b.getArrayOfWritePointers(), 2, count); p.processBlock(view, midi);
            for (int n = 0; n < count; ++n)
            {
                const double x = input.getSample(0, offset + n), y = b.getSample(0, n);
                inputEnergy += x * x; outputEnergy += y * y; peak = std::max(peak, std::abs(y));
            }
        }
        const double difference = 10 * std::log10(outputEnergy / inputEnergy);
        std::cout << "LISTEN 素材=" << material << " 模式=" << mode << " 有效电平差=" << difference
                  << " 峰值=" << 20 * std::log10(peak) << " 校准标记=" << rec.calibrationWithinRange << '\n';
        if (verify) check(difference > -0.7 && difference < 0.9, "LISTEN 应用后电平没有对齐旁通");
    }
}

static void calibrationAcrossRates()
{
    for (double sr : {44100., 96000., 192000.})
    {
        ai::AcousticFeatureExtractor extractor;
        extractor.prepare(sr, 256); extractor.startListening(1);
        juce::AudioBuffer<float> input(2, int(sr));
        for (int n = 0; n < input.getNumSamples(); ++n)
        {
            const float x = float(0.2 * std::sin(2 * juce::MathConstants<double>::pi * 1000 * n / sr));
            input.setSample(0, n, x); input.setSample(1, n, -x);
        }
        extractor.processBlock(input); extractor.computeFeatures();
        const auto feature = extractor.getLatestFeatures();
        check(feature.midEnergyPct > 99 && !feature.isHighConfidence, "跨采样率特征或概率声明错误");
        for (int mode : {Params::ModeVOX, Params::ModeFET})
        {
            ai::CompressorPresetRecommendation rec; rec.mode = mode; rec.voxGate = -80;
            rec.voxSmooth = mode == Params::ModeVOX;
            extractor.calibrateRecommendationWithVirtualDsp(rec);
            check(rec.calibrationWithinRange && std::abs(rec.calibratedRmsDb - rec.calibrationInputRmsDb) < 0.4,
                  "跨采样率有效电平校准失败");
        }
        ai::CompressorPresetRecommendation unreachable; unreachable.mode = Params::ModeVOX;
        unreachable.voxGate = 0; unreachable.voxComp = 0;
        extractor.calibrateRecommendationWithVirtualDsp(unreachable);
        check(!unreachable.calibrationWithinRange, "增益不足的方案被错误标为成功");
    }
}

static void trimSignalFlow(bool full)
{
    // 不对称双声道素材，包含强弱变化；以未加 Trim 的完整处理器输出为参照。
    const char* trims[] = {"vca_output_trim", "fet_output_trim", "opto_output_trim"};
    const char* mixes[] = {"vca_mix", "fet_mix", "opto_mix"};
    int cases = 0; double worstRelativeError = 0, worstGrError = 0;
    for (int mode : {1, 2, 3}) for (double rate : {44100.0, 96000.0})
    for (int size : {64, 257}) for (float mix : {0.f, 37.f, 100.f})
    for (float trim : {-18.f, 6.f, 18.f})
    {
        if (!full && (mode != 1 || rate > 45000 || size != 64 || mix < 99 || trim < 5 || trim > 7)) continue;
        SanMuCompAudioProcessor reference, adjusted;
        for (auto* p : {&reference, &adjusted})
        {
            set(*p,"mode",float(mode));set(*p,"global_bypass",0);
            set(*p,"vca_threshold",-30);set(*p,"vca_ratio",4);set(*p,"vca_attack",3);
            set(*p,"fet_input",25);set(*p,"fet_output",20);set(*p,"opto_peak_reduction",60);set(*p,"opto_gain",40);
            for(int k=0;k<3;++k){set(*p,trims[k],0);set(*p,mixes[k],mix);}
            p->setRateAndBufferSizeDetails(rate,size);p->prepareToPlay(rate,size);
        }
        for(int k=0;k<3;++k) set(adjusted,trims[k],k==mode-1?trim:-11.f);
        // 旧全局 Trim 也不能污染当前模式。
        set(adjusted,"output_trim",13);
        const double gain=std::pow(10.0,trim/20.0);
        double error=0, energy=0, grError=0, maxGr=0;
        juce::AudioBuffer<float> a(2,size),b(2,size);juce::MidiBuffer midi;
        const auto fill=[&](int offset){for(int c=0;c<2;++c)for(int n=0;n<size;++n){const double t=(offset+n)/rate;const double envelope=std::fmod(t,.13)<.04?1:.35;const float value=float((c==0?.20:.09)*envelope*std::sin(2*juce::MathConstants<double>::pi*(c==0?997:613)*t));a.setSample(c,n,value);b.setSample(c,n,value);}};
        for(int offset=0;offset<int(rate*.5);offset+=size)
        {
            fill(offset);reference.processBlock(a,midi);adjusted.processBlock(b,midi);
            if(offset<int(rate*.2))continue; // 包含 Opto 参数平滑的收敛期。
            for(int c=0;c<2;++c)for(int n=0;n<size;++n){const double expected=a.getSample(c,n)*gain,delta=b.getSample(c,n)-expected;check(std::isfinite(delta),"Trim 产生无效输出");error+=delta*delta;energy+=expected*expected;}
            grError=std::max(grError,std::abs(double(reference.getCompressor().getCurrentGainReductionDb()-adjusted.getCompressor().getCurrentGainReductionDb())));
            maxGr=std::max(maxGr,double(reference.getCompressor().getCurrentGainReductionDb()));
        }
        const double relative=std::sqrt(error/std::max(energy,1e-20));
        check(relative<1e-5,"Trim 不是混合后的单次总增益，或模式路由串用");
        check(grError<1e-5&&maxGr>.1,"Trim 改变 GR，或素材没有触发压缩");
        for(bool host : {false,true})
        {
            set(adjusted,"global_bypass",host?0.f:1.f);fill(0);
            if(host)adjusted.processBlockBypassed(b,midi);else adjusted.processBlock(b,midi);
            for(int c=0;c<2;++c)for(int n=0;n<size;++n)check(std::abs(b.getSample(c,n)-a.getSample(c,n))<1e-7f,"旁通仍施加 Trim");
        }
        worstRelativeError=std::max(worstRelativeError,relative);worstGrError=std::max(worstGrError,grError);++cases;
    }
    std::cout<<"Trim 信号流："<<cases<<" 组通过；最大相对波形误差 "<<worstRelativeError<<"，最大 GR 差 "<<worstGrError<<" dB\n";
}

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    try
    {
        if (argc == 3 && std::string(argv[1]) == "--meter-audit") { meterCadence(juce::File(argv[2])); return 0; }
        if (argc == 3 && std::string(argv[1]) == "--capture-smooth-ui") { smoothStateAndUi(juce::File(argv[2])); return 0; }
        if (argc == 2 && std::string(argv[1]) == "--listening-audit") { listeningAudit(false); return 0; }
        if (argc == 2 && std::string(argv[1]) == "--trim-baseline") { trimSignalFlow(false); return 0; }
        if (argc == 2 && std::string(argv[1]) == "--trim-audit") { trimSignalFlow(true); return 0; }
        trimSignalFlow(true);
        stateAndRouting();
        smoothStateAndUi();
        listeningAudit(true);
        calibrationAcrossRates();
        bypassAndMeters();
        meterCadence();
        featuresAndCalibration();
        limiterBoundary();
        feedbackCurves();
        partitionCheck<dsp_engines::FetEngine>();
        partitionCheck<dsp_engines::OptoEngine>();
        partitionCheck<dsp_engines::VcaEngine>();
        partitionCheck<dsp_engines::VoxEngine>();
        partitionCheck<dsp_engines::VoxEngine>(true);
        std::cout << "插件集成回归全部通过\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "失败：" << error.what() << '\n';
        return 1;
    }
}
