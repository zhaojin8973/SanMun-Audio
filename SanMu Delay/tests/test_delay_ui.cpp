// ==============================================================================
// test_delay_ui.cpp - SanMu Delay 现代化 UI 桥接与参数双向集成测试
// ==============================================================================

#define JucePlugin_Name "SanMu Delay"
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../source/PluginProcessor.h"
#include "../source/PluginEditor.h"
#include <iostream>
#include <stdexcept>
#include <functional>

// Release 构建也必须执行断言。
#define REQUIRE(condition) do { if (!(condition)) throw std::runtime_error("检查失败: " #condition); } while (false)

static juce::var data(const char* key, const juce::var& value)
{
    juce::var v = new juce::DynamicObject();
    v.getDynamicObject()->setProperty(key, value);
    return v;
}

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;

    std::cout << "===================================================\n";
    std::cout << "   SanMu Delay UI 现代化架构集成测试\n";
    std::cout << "===================================================\n";

    // 宿主发现阶段：单声道/立体声插入都可用，左右声道数不匹配仍拒绝。
    {
        SanMuDelayAudioProcessor monoProcessor;
        auto mono = monoProcessor.getBusesLayout();
        mono.inputBuses.set(0, juce::AudioChannelSet::mono());
        mono.outputBuses.set(0, juce::AudioChannelSet::mono());
        mono.inputBuses.set(1, juce::AudioChannelSet::disabled());
        REQUIRE(monoProcessor.checkBusesLayoutSupported(mono));
        REQUIRE(monoProcessor.setBusesLayout(mono));
        REQUIRE(monoProcessor.getTotalNumInputChannels() == 1);
        REQUIRE(monoProcessor.getTotalNumOutputChannels() == 1);
        auto monoSidechain = mono;
        monoSidechain.inputBuses.set(1, juce::AudioChannelSet::mono());
        REQUIRE(monoProcessor.checkBusesLayoutSupported(monoSidechain));
        auto mismatch = mono;
        mismatch.outputBuses.set(0, juce::AudioChannelSet::stereo());
        REQUIRE(!monoProcessor.checkBusesLayoutSupported(mismatch));
        auto stereo = mismatch;
        stereo.inputBuses.set(0, juce::AudioChannelSet::stereo());
        REQUIRE(monoProcessor.checkBusesLayoutSupported(stereo));

        monoProcessor.prepareToPlay(48000.0, 512);
        auto* mix = monoProcessor.apvts.getParameter(Params::IDs::mix);
        mix->setValueNotifyingHost(mix->convertTo0to1(0.0f));
        juce::AudioBuffer<float> monoAudio(1, 512);
        juce::MidiBuffer monoMidi;
        for (int i = 0; i < 512; ++i) monoAudio.setSample(0, i, i % 2 ? -0.2f : 0.2f);
        monoProcessor.processBlock(monoAudio, monoMidi);
        for (int i = 0; i < 512; ++i)
            REQUIRE(std::abs(monoAudio.getSample(0, i) - (i % 2 ? -0.2f : 0.2f)) < 1.0e-4f);
        // 100% 湿声的单声道脉冲必须在延迟后出现，且不产生无穷或非数值。
        const auto setMono = [&](const char* id, float value) {
            auto* parameter = monoProcessor.apvts.getParameter(id);
            parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
        };
        setMono(Params::IDs::sync_mode, static_cast<float>(Params::SyncFree));
        setMono(Params::IDs::time_l, 10.0f);
        setMono(Params::IDs::time_r, 10.0f);
        setMono(Params::IDs::feedback, 0.0f);
        setMono(Params::IDs::mix, 100.0f);
        monoProcessor.getDelayProcessor().reset();
        double wetEnergy = 0.0;
        for (int block = 0; block < 8; ++block)
        {
            monoAudio.clear();
            if (block == 0) monoAudio.setSample(0, 0, 0.5f);
            monoProcessor.processBlock(monoAudio, monoMidi);
            for (int i = 0; i < 512; ++i)
            {
                const auto sample = monoAudio.getSample(0, i);
                REQUIRE(std::isfinite(sample));
                if (block > 0) wetEnergy += sample * sample;
            }
        }
        REQUIRE(wetEnergy > 1.0e-7);
        monoProcessor.releaseResources();
        std::cout << "[ PASS ] 单声道主总线协商、侧链、干声与湿声处理，立体声保持可用\n";
    }

    SanMuDelayAudioProcessor processor;
    processor.prepareToPlay(48000.0, 512);

    // 新实例默认进入手动 BPM，宿主同步由时间编辑器主动选择。
    REQUIRE(std::abs(processor.apvts.getRawParameterValue(Params::IDs::sync_mode)->load()
                     - static_cast<float>(Params::SyncBpm)) < 0.01f);

    auto editor = std::unique_ptr<SanMuDelayAudioProcessorEditor>(
        dynamic_cast<SanMuDelayAudioProcessorEditor*>(processor.createEditor()));
    REQUIRE(editor != nullptr);

    // 默认槽位也能复制当前声音，不能只在首次切换 A/B 后生效。
    {
        auto* feedback = processor.apvts.getParameter(Params::IDs::feedback);
        feedback->setValueNotifyingHost(feedback->convertTo0to1(53.0f));
        editor->handleCommand("copyAB");
        editor->handleCommand("ab", data("slot", "B"));
        REQUIRE(std::abs(processor.apvts.getRawParameterValue(Params::IDs::feedback)->load() - 53.0f) < 0.1f);
        editor->handleCommand("ab", data("slot", "A"));
    }

    // 1. 测试 initialize 命令返回完整的 schema 和 presets
    {
        auto initData = editor->handleCommand("initialize");
        REQUIRE(initData.hasProperty("schema"));
        REQUIRE(initData.hasProperty("presets"));
        REQUIRE(initData.hasProperty("values"));
        REQUIRE(initData.hasProperty("ui"));

        auto schema = initData["schema"];
        REQUIRE(schema.hasProperty("engine"));
        REQUIRE(schema.hasProperty("sync_mode"));
        REQUIRE(schema.hasProperty("manual_bpm"));
        REQUIRE(schema.hasProperty("time_l"));
        REQUIRE(schema.hasProperty("time_r"));
        REQUIRE(schema.hasProperty("feedback"));
        REQUIRE(schema.hasProperty("mix"));
        REQUIRE(schema.hasProperty("output_gain"));

        auto presets = initData["presets"];
        REQUIRE(presets.isArray());
        REQUIRE(presets.size() >= 11);
        juce::StringArray categories;
        for (int i = 0; i < presets.size(); ++i)
        {
            REQUIRE(presets[i].hasProperty("category"));
            categories.addIfNotAlreadyThere(presets[i]["category"].toString());
        }
        REQUIRE(categories.contains("Modern"));
        REQUIRE(categories.contains("Tape"));
        REQUIRE(categories.contains("BBD"));
        REQUIRE(categories.contains("Diffused"));

        std::cout << "[ PASS ] 原生 initialize 协议握手、Schema 与出厂预设加载成功\n";
    }

    // 2. 测试参数双向同步与边界值校验
    {
        // 设置有效参数
        auto setParam = data("id", "feedback");
        setParam.getDynamicObject()->setProperty("value", 62.5);
        auto res = editor->handleCommand("set", setParam);
        REQUIRE(!res.hasProperty("error"));
        REQUIRE(std::abs(processor.apvts.getRawParameterValue(Params::IDs::feedback)->load() - 62.5f) < 0.1f);

        // 设置非法参数（超出边界）
        auto invalidParam = data("id", "feedback");
        invalidParam.getDynamicObject()->setProperty("value", 150.0);
        auto errRes = editor->handleCommand("set", invalidParam);
        REQUIRE(errRes.hasProperty("error"));

        std::cout << "[ PASS ] 原生参数设定与越界安全拦截通过\n";
    }

    // 3. 测试 Free 毫秒模式设置
    {
        auto setSync = data("id", "sync_mode");
        setSync.getDynamicObject()->setProperty("value", 2.0); // SyncFree
        editor->handleCommand("set", setSync);

        auto setTime = data("id", "time_l");
        setTime.getDynamicObject()->setProperty("value", 375.5);
        editor->handleCommand("set", setTime);

        REQUIRE(std::abs(processor.apvts.getRawParameterValue(Params::IDs::time_l)->load() - 375.5f) < 0.2f);
        std::cout << "[ PASS ] Free 模式纯毫秒精确设置通过\n";
    }

    // 4. 测试 A/B 对比切换
    {
        editor->handleCommand("ab", data("slot", "A"));
        REQUIRE(processor.getActiveSnapshot() == SanMuDelayAudioProcessor::ActiveSnapshot::A);

        editor->handleCommand("ab", data("slot", "B"));
        REQUIRE(processor.getActiveSnapshot() == SanMuDelayAudioProcessor::ActiveSnapshot::B);

        processor.copyActiveSnapshotToOther();
        std::cout << "[ PASS ] A/B 对比与跨槽复制通过\n";
    }

    // 5. 测试外观与首选项持久化（不污染声音撤销栈）
    {
        processor.undoManager.clearUndoHistory();
        auto uiData = data("theme", "dark");
        uiData.getDynamicObject()->setProperty("language", "zh");
        uiData.getDynamicObject()->setProperty("scale", 1.15);
        editor->handleCommand("ui", uiData);

        REQUIRE(!processor.undoManager.canUndo()); // 外观操作不进入声音撤销历史
        auto curState = editor->handleCommand("state");
        REQUIRE(curState["ui"]["theme"].toString() == "dark");
        REQUIRE(curState["ui"]["language"].toString() == "zh");

        std::cout << "[ PASS ] UI 首选项保存与声音撤销栈隔离通过\n";
    }

    // 6. 真实离线网页与原生参数之间的端到端桥接。
    editor->addToDesktop(0);
    editor->setVisible(true);
    const auto waitFor = [](const std::function<bool()>& done) {
        const auto limit = juce::Time::getMillisecondCounter() + 15000;
        while (!done() && juce::Time::getMillisecondCounter() < limit)
            juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
        REQUIRE(done());
    };
    bool loaded = false;
    const auto loadDeadline = juce::Time::getMillisecondCounter() + 15000;
    while (!loaded && juce::Time::getMillisecondCounter() < loadDeadline)
    {
        bool returned = false;
        editor->getBrowser().evaluateJavascript("Boolean(window.sanmuApp?.visualizer?.key)", [&](auto result) {
            if (auto* value = result.getResult()) loaded = bool(*value);
            returned = true;
        });
        waitFor([&] { return returned; });
        if (!loaded) juce::MessageManager::getInstance()->runDispatchLoopUntil(100);
    }
    REQUIRE(loaded);
    // WebKit 实际布局必须包含完整路由、时钟按钮及 Ducking 内的电平条。
    bool layoutReturned = false, layoutVisible = false;
    editor->getBrowser().evaluateJavascript(R"JS((() => {
        const bounds = document.querySelector('.instrument').getBoundingClientRect();
        const inside = (a,b) => a.left >= b.left-1 && a.top >= b.top-1 && a.right <= b.right+1 && a.bottom <= b.bottom+1;
        const selectors = ['.mode-routing','.echo-precision','.duck-meter','.duck-meter-track','#duck-meter-value'];
        return document.querySelectorAll('input[name="sync_mode"]').length === 3
            && !document.querySelector('#echo-live,.meters,.precision-note,#time-box-l-container')
            && selectors.every(selector => { const r = document.querySelector(selector).getBoundingClientRect(); return r.width > 0 && r.height > 0 && inside(r,bounds); })
            && inside(document.querySelector('.duck-meter').getBoundingClientRect(), document.querySelector('.duck-group').getBoundingClientRect());
    })())JS", [&](auto result) {
        if (auto* value = result.getResult()) layoutVisible = bool(*value);
        layoutReturned = true;
    });
    waitFor([&] { return layoutReturned; });
    REQUIRE(layoutVisible);

    // 原生音频输入经过真实避让检波后，网页小表应收到大于 6 dB 的读数。
    const auto setActual = [&](const char* id, float value) {
        auto* parameter = processor.apvts.getParameter(id);
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
    };
    setActual(Params::IDs::duck_enable, 1.0f);
    setActual(Params::IDs::duck_source, 0.0f);
    setActual(Params::IDs::duck_threshold, -24.0f);
    setActual(Params::IDs::duck_amount, 80.0f);
    juce::AudioBuffer<float> audio(std::max(2, processor.getTotalNumInputChannels()), 512);
    juce::MidiBuffer midi;
    for (int block = 0; block < 96; ++block) {
        audio.clear();
        for (int ch = 0; ch < 2; ++ch)
            for (int sample = 0; sample < 512; ++sample) audio.setSample(ch, sample, 0.5f);
        processor.processBlock(audio, midi);
    }
    REQUIRE(processor.getDelayProcessor().getCurrentDuckingGrDb() > 6.0f);
    bool duckVisible = false;
    const auto duckDeadline = juce::Time::getMillisecondCounter() + 5000;
    while (!duckVisible && juce::Time::getMillisecondCounter() < duckDeadline) {
        bool returned = false;
        editor->getBrowser().evaluateJavascript("parseFloat(document.querySelector('#duck-meter-value').textContent) > 6 && parseFloat(document.querySelector('#duck-meter-fill').style.width) > 25", [&](auto result) {
            if (auto* value = result.getResult()) duckVisible = bool(*value);
            returned = true;
        });
        waitFor([&] { return returned; });
        juce::MessageManager::getInstance()->runDispatchLoopUntil(40);
    }
    REQUIRE(duckVisible);
    std::cout << "[ PASS ] WebKit 路由及时钟控件可见；真实音频避让量回显于 Ducking 小表\n";
    editor->getBrowser().evaluateJavascript(R"JS((async () => {
        const app = window.sanmuApp;
        app.begin(); app.set('sync_mode',2); app.set('time_link',0); app.set('topology',0); app.set('time_l',250); app.end();
        await app.commandQueue;
        document.querySelector('#echo-time-l').dispatchEvent(new KeyboardEvent('keydown',{key:'End',bubbles:true}));
        await app.commandQueue;
        document.querySelector('#echo-feedback-control').dispatchEvent(new KeyboardEvent('keydown',{key:'Home',bubbles:true}));
    })())JS");
    waitFor([&] {
        return processor.apvts.getRawParameterValue(Params::IDs::time_l)->load() > 1999.9f
            && processor.apvts.getRawParameterValue(Params::IDs::feedback)->load() < 0.01f;
    });
    // 从宿主侧改变引擎，网页必须更新对应专属旋钮。
    auto* engine = processor.apvts.getParameter(Params::IDs::engine);
    engine->setValueNotifyingHost(engine->convertTo0to1(1.0f));
    bool reflected = false;
    const auto deadline = juce::Time::getMillisecondCounter() + 15000;
    while (!reflected && juce::Time::getMillisecondCounter() < deadline)
    {
        bool returned = false;
        editor->getBrowser().evaluateJavascript("window.sanmuApp?.values.engine === 1 && !!document.querySelector('#edit-tape_drive')", [&](auto result) {
            if (auto* value = result.getResult()) reflected = bool(*value);
            returned = true;
        });
        waitFor([&] { return returned; });
        juce::MessageManager::getInstance()->runDispatchLoopUntil(40);
    }
    REQUIRE(reflected);
    editor->setVisible(false);
    editor->removeFromDesktop();
    std::cout << "[ PASS ] 网页节点与反馈键盘操作写入原生参数；宿主引擎变更回显专属控件\n";

    std::cout << "===================================================\n";
    std::cout << "   所有 SanMu Delay UI 架构测试全部顺利通过！\n";
    std::cout << "===================================================\n";

    return 0;
}
