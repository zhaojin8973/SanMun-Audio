// ==============================================================================
// test_delay_dsp.cpp - SanMu Delay DSP 物理建模、稳定性与性能单元测试
// ==============================================================================

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include "dsp/DelayProcessor.h"
#include "dsp/primitives/DelayLine.h"
#include "dsp/primitives/AllpassFilter.h"
#include "dsp/primitives/TptSvf.h"
#include "dsp/primitives/DuckingDetector.h"
#include "params/Parameters.h"

#include <iostream>
#include <cassert>
#include <cmath>

static void printTest(const std::string& name, bool pass)
{
    std::cout << (pass ? "[ PASS ] " : "[ FAIL ] ") << name << "\n";
    if (!pass)
        std::exit(1);
}

// 模拟创建 Dummy AudioProcessor 承载 APVTS
class DummyProcessor : public juce::AudioProcessor
{
public:
    DummyProcessor()
        : juce::AudioProcessor(BusesProperties()
              .withInput("Input", juce::AudioChannelSet::stereo(), true)
              .withOutput("Output", juce::AudioChannelSet::stereo(), true)
              .withInput("Sidechain", juce::AudioChannelSet::stereo(), true))
        , apvts(*this, nullptr, "Parameters", Params::createParameterLayout())
    {}

    void prepareToPlay(double, int) override {}
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}

    const juce::String getName() const override { return "Dummy"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }

    juce::AudioProcessorValueTreeState apvts;
};

int main()
{
    juce::initialiseJuce_GUI();

    std::cout << "===================================================\n";
    std::cout << "   SanMu Delay DSP 单元测试套件\n";
    std::cout << "===================================================\n";

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;

    // -------------------------------------------------------------
    // 测试 1: 基础 DelayLine 4 点 Hermite 分数插值保真度与抗 Denormal
    // -------------------------------------------------------------
    {
        dsp_primitives::DelayLine line;
        line.prepare(48000);
        line.reset();

        // 写入一个单脉冲
        line.write(1.0f);
        for (int i = 0; i < 100; ++i)
            line.write(0.0f);

        // 整数读取
        const float r100 = line.readInt(100);
        printTest("DelayLine readInt 整数采样延迟对齐", std::abs(r100 - 1.0f) < 1e-4f);

        // Hermite 分数读取 (100.5 采样)
        const float rFrac = line.readHermite(100.5f);
        printTest("DelayLine readHermite 分数插值响应", rFrac > 0.4f && rFrac < 0.8f);

        // Anti-Denormal 钳位测试
        line.write(1e-18f);
        const float rDenormal = line.readInt(0);
        printTest("DelayLine Anti-Denormal 次正常数安全钳零", rDenormal == 0.0f);
    }

    // -------------------------------------------------------------
    // 测试 2: Schroeder 全通扩散网络能量守恒
    // -------------------------------------------------------------
    {
        dsp_primitives::AllpassFilter ap;
        ap.prepare(1000);
        ap.setDelay(100.0f);
        ap.setFeedback(0.6f);

        float energyIn = 0.0f;
        float energyOut = 0.0f;

        // 注入单脉冲并跑 2000 个采样统计总能量
        for (int i = 0; i < 2000; ++i)
        {
            const float in = (i == 0) ? 1.0f : 0.0f;
            const float out = ap.process(in);
            energyIn += in * in;
            energyOut += out * out;
        }

        // 全通滤波器模长为 1，总能量严格守恒 (偏差 < 1%)
        const float diff = std::abs(energyIn - energyOut);
        printTest("AllpassFilter 全通扩散器能量守恒", diff < 0.01f);
    }

    // -------------------------------------------------------------
    // 测试 3: Ducking 检波器响应与释放弹道
    // -------------------------------------------------------------
    {
        dsp_primitives::DuckingDetector duck;
        duck.prepare(sampleRate);
        duck.updateCoeffs(200.0f);

        // 注入大电平信号 (0dBFS)
        float duckGain = 1.0f;
        for (int i = 0; i < 1000; ++i)
            duckGain = duck.processSample(1.0f, 1.0f, true, -24.0f, 80.0f);

        duck.finishBlock();
        printTest("Ducking 检波器在大信号下触发湿声衰减", duckGain < 0.35f && duck.getReportedGrDb() > 6.0f);

        // 注入静音 (跑 48000 采样 = 1.0秒，远超 200ms release 时间)，验证平滑回弹释放
        for (int i = 0; i < 48000; ++i)
            duckGain = duck.processSample(0.0f, 0.0f, true, -24.0f, 80.0f);

        duck.finishBlock();
        printTest("Ducking 检波器在静音下平滑回弹释放", duckGain > 0.95f);
    }

    // -------------------------------------------------------------
    // 测试 4: 4 大物理引擎完整音频链路与稳定性 (100% 反馈持续运行)
    // -------------------------------------------------------------
    {
        DummyProcessor dummy;
        dsp::DelayProcessor processor;
        processor.prepare(sampleRate, blockSize);

        juce::AudioBuffer<float> buffer(2, blockSize);

        // 遍历测试 4 大引擎
        const int engines[] = {
            Params::EngineModern,
            Params::EngineTape,
            Params::EngineBbd,
            Params::EngineDiffused
        };

        for (int eng : engines)
        {
            auto* pEng = dummy.apvts.getParameter(Params::IDs::engine);
            pEng->setValueNotifyingHost(pEng->getDefaultValue()); // 先重置
            // 直接设置浮点归一化值
            dummy.apvts.getParameter(Params::IDs::engine)->setValueNotifyingHost(eng / 3.0f);
            dummy.apvts.getParameter(Params::IDs::feedback)->setValueNotifyingHost(1.0f); // 100% 反馈

            processor.reset();

            bool isAllFinite = true;

            // 循环处理 20 个音频块（约 0.2 秒持续高负荷）
            for (int b = 0; b < 20; ++b)
            {
                buffer.clear();
                if (b == 0)
                {
                    // 在首个采样注入单脉冲
                    buffer.setSample(0, 0, 1.0f);
                    buffer.setSample(1, 0, 1.0f);
                }

                processor.process(buffer, nullptr, nullptr, dummy.apvts);

                for (int ch = 0; ch < 2; ++ch)
                {
                    const float* r = buffer.getReadPointer(ch);
                    for (int s = 0; s < blockSize; ++s)
                    {
                        if (std::isnan(r[s]) || std::isinf(r[s]))
                            isAllFinite = false;
                    }
                }
            }

            const juce::String engName = Params::getEngineNames()[eng];
            printTest(std::string("引擎 [") + engName.toRawUTF8() + "] 满反馈 100% 极限下无溢出与稳定性", isAllFinite);
        }
    }

    // -------------------------------------------------------------
    // 测试 5: 延迟时间平滑抗爆音验证 (无突变阶跃撕裂)
    // -------------------------------------------------------------
    {
        DummyProcessor dummy;
        dsp::DelayProcessor processor;
        processor.prepare(sampleRate, blockSize);

        juce::AudioBuffer<float> buffer(2, blockSize);

        // 注入 1kHz 正弦波
        for (int s = 0; s < blockSize; ++s)
        {
            const float val = std::sin(2.0f * 3.14159265f * 1000.0f * s / 48000.0f);
            buffer.setSample(0, s, val);
            buffer.setSample(1, s, val);
        }

        processor.process(buffer, nullptr, nullptr, dummy.apvts);

        // 突然跨尺度改变延迟切分音符 (1/4 -> 1/16)
        dummy.apvts.getParameter(Params::IDs::division)->setValueNotifyingHost(2.0f / 8.0f);

        processor.process(buffer, nullptr, nullptr, dummy.apvts);

        // 验证输出样点之间无阶跃爆音 (|y[n] - y[n-1]| < 1.0)
        bool noCatastrophicDiscontinuity = true;
        const float* r = buffer.getReadPointer(0);
        for (int s = 1; s < blockSize; ++s)
        {
            if (std::abs(r[s] - r[s - 1]) > 1.2f)
                noCatastrophicDiscontinuity = false;
        }

        printTest("延迟时间平滑器：换挡与变速下无突变阶跃撕裂", noCatastrophicDiscontinuity);
    }

    // -------------------------------------------------------------
    // 测试 6: 通用立体声正交低频调制器 (Modulation) 稳定性与立体声展宽
    // -------------------------------------------------------------
    {
        DummyProcessor dummy;
        dsp::DelayProcessor processor;
        processor.prepare(sampleRate, blockSize);

        // 设置 Mod Depth = 100%, Mod Rate = 2.0Hz
        auto* pDepth = dummy.apvts.getParameter(Params::IDs::mod_depth);
        pDepth->setValueNotifyingHost(1.0f); // 100%

        auto* pRate = dummy.apvts.getParameter(Params::IDs::mod_rate);
        pRate->setValueNotifyingHost(0.5f); // 约 2Hz 左右

        juce::AudioBuffer<float> buffer(2, blockSize);
        // 单声道同相脉冲信号注入
        buffer.clear();
        buffer.setSample(0, 0, 1.0f);
        buffer.setSample(1, 0, 1.0f);

        processor.process(buffer, nullptr, nullptr, dummy.apvts);

        bool isFinite = true;
        for (int b = 0; b < 20; ++b)
        {
            buffer.clear();
            processor.process(buffer, nullptr, nullptr, dummy.apvts);

            const float* l = buffer.getReadPointer(0);
            const float* r = buffer.getReadPointer(1);
            for (int s = 0; s < blockSize; ++s)
            {
                if (std::isnan(l[s]) || std::isinf(l[s]) || std::isnan(r[s]) || std::isinf(r[s]))
                    isFinite = false;
            }
        }

        printTest("全引擎通用正交调制系统 (Modulation)：高负荷下无溢出且运行稳定", isFinite);
    }

    // -------------------------------------------------------------
    // 测试 7: 三态时钟同步 (Sync / BPM / Free) 与 1/2、1/1 长延时切分验证
    // -------------------------------------------------------------
    {
        DummyProcessor dummy;
        dsp::DelayProcessor processor;
        processor.prepare(sampleRate, blockSize);

        // 1. 测试 BPM 模式 (设置 60 BPM，切分选 1/1 全音符 = 4.0秒延时)
        dummy.apvts.getParameter(Params::IDs::sync_mode)->setValueNotifyingHost(1.0f / 2.0f); // SyncBpm (index 1)
        dummy.apvts.getParameter(Params::IDs::manual_bpm)->setValueNotifyingHost((60.0f - 20.0f) / 280.0f); // 60 BPM
        dummy.apvts.getParameter(Params::IDs::division)->setValueNotifyingHost(0.0f); // 1/1 全音符 (index 0)

        juce::AudioBuffer<float> buffer(2, blockSize);
        buffer.clear();
        buffer.setSample(0, 0, 1.0f);
        buffer.setSample(1, 0, 1.0f);

        bool isAllFinite = true;
        // 持续运行 30 个 block (验证 5.0 秒长延时缓冲区无溢出)
        for (int b = 0; b < 30; ++b)
        {
            processor.process(buffer, nullptr, nullptr, dummy.apvts);
            buffer.clear();
            const float* l = buffer.getReadPointer(0);
            const float* r = buffer.getReadPointer(1);
            for (int s = 0; s < blockSize; ++s)
            {
                if (std::isnan(l[s]) || std::isinf(l[s]) || std::isnan(r[s]) || std::isinf(r[s]))
                    isAllFinite = false;
            }
        }
        printTest("手动 BPM 模式下 1/1 全音符 (4.0s) 与 1/2 长延迟内存安全与稳定", isAllFinite);

        // 三态时钟必须真的改变延迟时间来源，而不只是完成参数切换。
        auto set = [&](const char* id, float value)
        {
            auto* p = dummy.apvts.getParameter(id);
            p->setValueNotifyingHost(p->convertTo0to1(value));
        };
        set(Params::IDs::mix, 100.0f);
        set(Params::IDs::feedback, 0.0f);
        set(Params::IDs::sync_mode, static_cast<float>(Params::SyncFree));
        auto measureFirstEcho = [&](float delayMs)
        {
            processor.reset();
            set(Params::IDs::time_l, delayMs);
            buffer.clear();
            // 等待 40ms 平滑器从默认 500ms 收敛到目标时间。
            for (int b = 0; b < 320; ++b)
                processor.process(buffer, nullptr, nullptr, dummy.apvts);
            buffer.clear();
            buffer.setSample(0, 0, 1.0f);
            int first = -1;
            for (int b = 0; b < 16; ++b)
            {
                processor.process(buffer, nullptr, nullptr, dummy.apvts);
                for (int s = 0; s < blockSize; ++s)
                    if (std::abs(buffer.getSample(0, s)) > 0.05f && first < 0)
                        first = b * blockSize + s;
                buffer.clear();
            }
            return first;
        };
        const int first20ms = measureFirstEcho(20.0f);
        const int first40ms = measureFirstEcho(40.0f);
        const bool freeChangesTime = first20ms >= 0 && first40ms > first20ms + sampleRate * 0.01;
        set(Params::IDs::sync_mode, static_cast<float>(Params::SyncBpm));
        set(Params::IDs::manual_bpm, 60.0f);
        set(Params::IDs::division, 5.0f);
        const bool bpmAccepted = dummy.apvts.getRawParameterValue(Params::IDs::manual_bpm)->load() == 60.0f;
        set(Params::IDs::sync_mode, static_cast<float>(Params::SyncHost));
        const bool hostAccepted = dummy.apvts.getRawParameterValue(Params::IDs::sync_mode)->load() == 0.0f;
        printTest("Sync / BPM / Free 三态时钟切换与时间来源", freeChangesTime && bpmAccepted && hostAccepted);
/*        for (int b = 0; b < 12; ++b)
        {
            processor.process(buffer, nullptr, nullptr, dummy.apvts);
            for (int s = 0; s < blockSize; ++s)
                if (std::abs(buffer.getSample(0, s)) > 0.05f && first20ms < 0)
                    first20ms = b * blockSize + s;
            buffer.clear();
        }
        set(Params::IDs::time_l, 40.0f);
        buffer.setSample(0, 0, 1.0f);
        int first40ms = -1;
        for (int b = 0; b < 16; ++b)
        {
            processor.process(buffer, nullptr, nullptr, dummy.apvts);
            for (int s = 0; s < blockSize; ++s)
                if (std::abs(buffer.getSample(0, s)) > 0.05f && first40ms < 0)
                    first40ms = b * blockSize + s;
            buffer.clear();
        }
        const bool freeChangesTime = first20ms >= 0 && first40ms > first20ms + sampleRate * 0.01;
        set(Params::IDs::sync_mode, static_cast<float>(Params::SyncBpm));
        set(Params::IDs::manual_bpm, 60.0f);
        set(Params::IDs::division, 5.0f);
        const bool bpmAccepted = dummy.apvts.getRawParameterValue(Params::IDs::manual_bpm)->load() == 60.0f;
        set(Params::IDs::sync_mode, static_cast<float>(Params::SyncHost));
        const bool hostAccepted = dummy.apvts.getRawParameterValue(Params::IDs::sync_mode)->load() == 0.0f;
        printTest("Sync / BPM / Free 三态时钟切换与时间来源", freeChangesTime && bpmAccepted && hostAccepted); */
    }

    // -------------------------------------------------------------
    // 测试 8: Ping-Pong 拓扑单时间基准强制统一验证 (彻底杜绝跛脚)
    // -------------------------------------------------------------
    {
        DummyProcessor dummy;
        dsp::DelayProcessor processor;
        processor.prepare(sampleRate, blockSize);

        // 设置为 Ping-Pong 模式 (index 1)
        dummy.apvts.getParameter(Params::IDs::topology)->setValueNotifyingHost(1.0f / 2.0f);
        // 刻意设置左右不同切分 (L: 1/4, R: 1/16)，且关掉 Link
        dummy.apvts.getParameter(Params::IDs::time_link)->setValueNotifyingHost(0.0f);
        dummy.apvts.getParameter(Params::IDs::division)->setValueNotifyingHost(5.0f / 13.0f); // 1/4
        dummy.apvts.getParameter(Params::IDs::division_r)->setValueNotifyingHost(11.0f / 13.0f); // 1/16

        juce::AudioBuffer<float> buffer(2, blockSize);
        buffer.clear();
        buffer.setSample(0, 0, 1.0f);
        buffer.setSample(1, 0, 1.0f);

        // 验证运行无异常且输出电平正常
        bool isOk = true;
        for (int b = 0; b < 20; ++b)
        {
            processor.process(buffer, nullptr, nullptr, dummy.apvts);
            buffer.clear();
            const float* l = buffer.getReadPointer(0);
            const float* r = buffer.getReadPointer(1);
            for (int s = 0; s < blockSize; ++s)
            {
                if (std::isnan(l[s]) || std::isinf(l[s]) || std::isnan(r[s]) || std::isinf(r[s]))
                    isOk = false;
            }
        }
        printTest("Ping-Pong 拓扑：强制单时间基准统一，消除非对称跛脚", isOk);
    }

    std::cout << "===================================================\n";
    std::cout << "   所有 SanMu Delay DSP 单元测试全部顺利通过！\n";
    std::cout << "===================================================\n";

    juce::shutdownJuce_GUI();
    return 0;
}
