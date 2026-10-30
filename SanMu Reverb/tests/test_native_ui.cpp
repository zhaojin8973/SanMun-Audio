#include "../source/PluginEditor.h"
#include <array>
#include <cstdio>
#include <limits>

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    SanMuReverbAudioProcessor processor;
    processor.prepareToPlay(48000, 128);
    auto editor = std::make_unique<SanMuReverbAudioProcessorEditor>(processor);
    juce::DocumentWindow window("SanMu Reverb 原生界面验证", juce::Colour(0xff20252f), juce::DocumentWindow::closeButton);
    window.setContentNonOwned(editor.get(), true);
    window.setVisible(true);
    const auto evaluate = [&](const juce::String& script) {
        juce::var value;
        bool finished = false;
        editor->getBrowser().evaluateJavascript(script, [&](auto result) {
            if (result.getResult()) value = *result.getResult();
            finished = true;
        });
        const auto until = juce::Time::getMillisecondCounterHiRes() + 5000;
        while (!finished && juce::Time::getMillisecondCounterHiRes() < until)
            juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
        return value;
    };
    bool ready = false;
    for (int i = 0; i < 200 && !ready; ++i)
    {
        juce::MessageManager::getInstance()->runDispatchLoopUntil(50);
        ready = static_cast<bool>(evaluate("Boolean(window.__sanmuStudio?.nativeReady)"));
    }
    int failures = 0;
    const auto check = [&](bool ok, const char* label) { std::printf("%s：%s\n", ok ? "通过" : "失败", label); if (!ok) ++failures; std::fflush(stdout); };
    check(ready, "原生 WebView 离线载入完整页面并连接宿主");
    if (ready)
    {
        check(editor->getWidth() == 1100 && editor->getHeight() == 728 && editor->getBrowser().getHeight() == 700, "新实例保留原面板高度并额外显示期限提示条");
        check(evaluate("document.documentElement.dataset.theme").toString() == "light", "新实例默认使用浅色主题");
        check(static_cast<bool>(evaluate(R"JS((() => {
            const panel = document.querySelector('.instrument').getBoundingClientRect();
            const details = document.querySelector('.details').getBoundingClientRect();
            return document.documentElement.scrollHeight <= innerHeight + 1
                && document.documentElement.scrollWidth <= innerWidth + 1
                && details.bottom <= innerHeight && panel.top >= 0
                && innerHeight - panel.bottom >= 0 && innerHeight - panel.bottom <= 24;
        })())JS")), "默认窗口完整显示底部控件且无多余空白或滚动条");
        check(static_cast<int>(evaluate("document.querySelectorAll('[data-knob]').length")) == 15, "原生面板显示 15 个旋钮");
        check(static_cast<bool>(evaluate("Boolean(window.__sanmuStudio.scene?.renderer)")), "原生窗口成功创建三维渲染器");
        evaluate("document.querySelector('#edit-highCut').value='8.5 kHz';document.querySelector('#edit-highCut').dispatchEvent(new Event('change'))");
        juce::MessageManager::getInstance()->runDispatchLoopUntil(250);
        check(std::abs(processor.apvts.getRawParameterValue("high_cut")->load() - 8500) < .01f, "页面输入实际写入宿主参数");
        auto* parameter = processor.apvts.getParameter("decay"); parameter->setValueNotifyingHost(parameter->convertTo0to1(6.25f));
        juce::MessageManager::getInstance()->runDispatchLoopUntil(200);
        check(std::abs(static_cast<double>(evaluate("window.__sanmuStudio.parameters.decay")) - 6.25) < .001, "宿主参数变化同步回页面");
        struct GestureCounter final : juce::AudioProcessorParameter::Listener
        {
            int starts = 0, ends = 0;
            void parameterValueChanged(int, float) override {}
            void parameterGestureChanged(int, bool starting) override { starting ? ++starts : ++ends; }
        } depthGestures;
        auto* depth = processor.apvts.getParameter("mod_depth");
        depth->addListener(&depthGestures);
        // 仅替代操作系统指针捕获，拖动仍经过实际旋钮事件、页面队列和原生桥接。
        evaluate(R"JS((() => {
            const dial = document.querySelector('#knob-depth');
            dial.setPointerCapture = () => {};
            dial.dispatchEvent(new PointerEvent('pointerdown', { bubbles: true, pointerId: 7, button: 0, clientY: 200 }));
        })())JS");
        parameter->setValueNotifyingHost(parameter->convertTo0to1(4.75f));
        processor.apvts.getParameter("bypass")->setValueNotifyingHost(1.0f);
        juce::MessageManager::getInstance()->runDispatchLoopUntil(100);
        evaluate(R"JS((() => {
            const dial = document.querySelector('#knob-depth');
            dial.dispatchEvent(new PointerEvent('pointermove', { bubbles: true, pointerId: 7, clientY: 176 }));
            dial.dispatchEvent(new PointerEvent('pointerup', { bubbles: true, pointerId: 7, clientY: 176 }));
            delete dial.setPointerCapture;
        })())JS");
        juce::MessageManager::getInstance()->runDispatchLoopUntil(250);
        check(std::abs(processor.apvts.getRawParameterValue("decay")->load() - 4.75f) < .001f
            && processor.apvts.getRawParameterValue("bypass")->load() > .5f,
            "拖动一个旋钮时不覆盖其他参数的宿主自动化");
        check(std::abs(processor.apvts.getRawParameterValue("mod_depth")->load() - 48.0f) < .01f,
            "自动化同时到来时当前拖动仍写入正确参数");
        check(std::abs(static_cast<double>(evaluate("window.__sanmuStudio.parameters.decay")) - 4.75) < .001,
            "拖动结束后页面恢复到宿主实际状态");
        depth->removeListener(&depthGestures);
        check(depthGestures.starts == 1 && depthGestures.ends == 1,
            "旋钮拖动向宿主提交一次完整的自动化手势");
        const auto beforeInvalid = juce::JSON::toString(editor->handleCommand("end", {}));
        bool rejected = true;
        for (const auto* invalid : {
            R"({"values":{"decay":2,"unknown":1}})",
            R"({"values":{"decay":2,"depth":101}})",
            R"({"values":{"decay":2},"mode":8})",
            R"({"values":{"decay":2},"mode":1.5})",
            R"({"values":{"decay":2},"bypass":"true"})",
            R"({"values":{"decay":"2"}})", R"({})" })
        {
            const auto result = editor->handleCommand("patch", juce::JSON::parse(invalid));
            rejected &= result.hasProperty("error")
                && juce::JSON::toString(editor->handleCommand("end", {})) == beforeInvalid;
        }
        auto nonFinite = juce::JSON::parse(R"({"values":{"decay":2,"depth":0}})");
        nonFinite["values"].getDynamicObject()->setProperty("depth", std::numeric_limits<double>::quiet_NaN());
        rejected &= editor->handleCommand("patch", nonFinite).hasProperty("error")
            && juce::JSON::toString(editor->handleCommand("end", {})) == beforeInvalid;
        check(rejected, "增量请求拒绝未知参数、越界、错误类型和非有限值，整批不留下部分写入");
        bool allModes = true;
        constexpr std::array<int, 4> expectedCategories { 2, 0, 1, 3 };
        for (int family = 0; family < 4; ++family)
        {
            for (int style = 0; style < 2; ++style)
            {
                evaluate("document.querySelector('[name=family][value=\"" + juce::String(family)
                    + "\"]').click();document.querySelector('[name=style][value=\"" + juce::String(style) + "\"]').click()");
                juce::MessageManager::getInstance()->runDispatchLoopUntil(150);
                allModes &= juce::roundToInt(processor.apvts.getRawParameterValue("category")->load()) == expectedCategories[family]
                    && juce::roundToInt(processor.apvts.getRawParameterValue("mode")->load()) == Params::menuIndexToMode(expectedCategories[family], style)
                    && std::abs(processor.apvts.getRawParameterValue("decay")->load() - 4.75f) < .001f
                    && processor.apvts.getRawParameterValue("bypass")->load() > .5f;
            }
        }
        check(allModes, "四类八模式的页面选择正确映射宿主编号，并保留声音参数与旁通");
        evaluate("document.querySelector('[name=family][value=\"0\"]').click()");
        processor.apvts.getParameter("bypass")->setValueNotifyingHost(0.0f);
        juce::MessageManager::getInstance()->runDispatchLoopUntil(100);
        evaluate("document.querySelector('#bypass').click()");
        juce::MessageManager::getInstance()->runDispatchLoopUntil(200);
        check(processor.apvts.getRawParameterValue("bypass")->load() > .5f, "页面旁通与宿主旁通参数相连");
        evaluate("document.querySelector('#bypass').click();window.__sanmuStudio.setTheme('light')");
        juce::AudioBuffer<float> buffer(2, 128); juce::MidiBuffer midi;
        for (int i = 0; i < 100; ++i)
        {
            buffer.clear(); buffer.setSample(0, 0, .25f); buffer.setSample(1, 0, .2f);
            processor.processBlock(buffer, midi); juce::MessageManager::getInstance()->runDispatchLoopUntil(3);
        }
        check(static_cast<double>(evaluate("window.__sanmuStudio.audio.latest?.dryRms || 0")) > 0,
              "实际原生音频信号驱动页面电平与声场");
        evaluate("window.__visualBefore=JSON.stringify(window.__sanmuStudio.snapshot()); document.querySelector('[name=view][value=wet]').click(); document.querySelector('#visuals-enabled').click()");
        const int pausedFrames = static_cast<int>(evaluate("window.__sanmuStudio.scene.frameCount"));
        for (int i = 0; i < 40; ++i)
        {
            buffer.clear(); buffer.setSample(0, 0, .25f); buffer.setSample(1, 0, .2f);
            processor.processBlock(buffer, midi); juce::MessageManager::getInstance()->runDispatchLoopUntil(5);
        }
        check(static_cast<int>(evaluate("window.__sanmuStudio.scene.frameCount")) == pausedFrames
            && static_cast<bool>(evaluate("window.__sanmuStudio.scene.enabled === false && document.querySelector('#stage canvas').hidden")),
            "关闭声场后停止渲染并隐藏画面");
        check(static_cast<bool>(evaluate("JSON.stringify(window.__sanmuStudio.snapshot())===window.__visualBefore && window.__sanmuStudio.audio.latest.dryRms>0 && Number(document.querySelector('#meter-0').parentElement.getAttribute('aria-valuenow')) > -60")),
            "声场关闭期间声音参数不变，原生音频与电平继续更新");
        juce::MemoryBlock savedState; processor.getStateInformation(savedState);
        window.clearContentComponent(); editor.reset();
        processor.setStateInformation(savedState.getData(), static_cast<int>(savedState.getSize()));
        editor = std::make_unique<SanMuReverbAudioProcessorEditor>(processor);
        window.setContentNonOwned(editor.get(), true);
        bool restored = false;
        for (int i = 0; i < 200 && !restored; ++i)
        {
            juce::MessageManager::getInstance()->runDispatchLoopUntil(50);
            restored = static_cast<bool>(evaluate("Boolean(window.__sanmuStudio?.nativeReady)"));
        }
        check(restored && static_cast<bool>(evaluate("!window.__sanmuStudio.scene.enabled && !document.querySelector('#visuals-enabled').checked && window.__sanmuStudio.scene.view==='wet' && document.querySelector('#input-legend').hidden")),
            "工程状态恢复后保留声场开关、信号视图及图例状态");
        evaluate("document.querySelector('#visuals-enabled').click()");
        juce::MessageManager::getInstance()->runDispatchLoopUntil(100);
        check(static_cast<bool>(evaluate("window.__sanmuStudio.scene.enabled && !document.querySelector('#stage canvas').hidden && window.__sanmuStudio.scene.frameCount>0")),
            "重新开启声场后恢复渲染");
        std::printf("页面状态：%s\n", evaluate("JSON.stringify({ready:window.__sanmuStudio.nativeReady,surface:window.__sanmuStudio.scene.surface,parameters:window.__sanmuStudio.parameters,notification:document.querySelector('#notification').textContent})").toString().toRawUTF8());
    }
    window.clearContentComponent(); editor.reset();
    return failures ? 1 : 0;
}
