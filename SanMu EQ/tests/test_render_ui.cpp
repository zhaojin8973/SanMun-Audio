// 原生桥接契约验证；交互预览与插件使用同一份内嵌资源。
#include "WebEqEditor.h"
#include "params/UpgradeState.h"
#include <iostream>

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    SanMuQ6AudioProcessor processor;
    processor.setRateAndBufferSizeDetails(48000, 512);
    processor.prepareToPlay(48000, 512);
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    auto* web = dynamic_cast<SanMuEqWebEditor*>(editor.get());
    if (web == nullptr) return 1;
    if (argc > 2 && juce::String(argv[1]) == "--response-fixture") {
        juce::Array<juce::var> rows;
        for (int type = 0; type < 5; ++type) for (float q : {.1f, 1.f, 10.f, 40.f}) for (float gain : {-24.f, 0.f, 24.f}) for (int slope = 0; slope < 4; ++slope) {
            EqBand band; band.update(type, true, 2310, gain, q, false, slope, 0);
            juce::var row = new juce::DynamicObject();
            row.getDynamicObject()->setProperty("type", type); row.getDynamicObject()->setProperty("q", q);
            row.getDynamicObject()->setProperty("gain", gain); row.getDynamicObject()->setProperty("slope", slope);
            juce::Array<juce::var> response;
            for (int point = 0; point < 320; ++point) response.add(band.getMagnitudeAtFrequency(20 * std::pow(1000., point/319.), 96000));
            row.getDynamicObject()->setProperty("response", response); rows.add(row);
        }
        return juce::File(argv[2]).replaceWithText(juce::JSON::toString(rows)) ? 0 : 2;
    }
    bool passed = true;
    auto check = [&](bool condition, const char* message) { if (!condition) { passed = false; std::cerr << message << std::endl; } };
    const auto json = [](const char* value) { return juce::JSON::parse(value); };
    auto state = web->handleCommand("initialize", {});
    const auto initialUi = state["ui"].clone();
    web->handleCommand("ui", json(R"({"theme":"dark","language":"en","tips":"off","scale":1.1})"));
    {
        SanMuQ6AudioProcessor fresh;
        // 模拟旧工程带浅色设置：个人偏好必须优先，声音参数保持工程值。
        fresh.apvts.state.setProperty("ui_web_config", R"({"theme":"light","language":"zh"})", nullptr);
        auto* gain = fresh.apvts.getParameter(Params::IDs::bandGain(1)); gain->setValueNotifyingHost(gain->convertTo0to1(3));
        SanMuEqWebEditor other(fresh); const auto restored = other.handleCommand("initialize", {});
        check(restored["ui"]["theme"] == "dark" && restored["ui"]["language"] == "en" && restored["ui"]["tips"] == "off", "新实例未继承本机偏好");
        check(std::abs(double(restored["values"]["band1_gain"]) - 3) < .001, "界面偏好不能改写声音参数");
        check(other.getWidth() == 1210, "新实例未继承窗口缩放");
    }
    web->handleCommand("ui", initialUi);
    web->handleCommand("begin", {});
    check(!web->handleCommand("setMany", json(R"({"values":{"band2_freq":1234,"band2_gain":2.5}})")).hasProperty("error"), "有效参数批次失败");
    check(web->handleCommand("setMany", json(R"({"values":{"band2_freq":1400,"band2_gain":999}})")).hasProperty("error"), "无效批次必须整体拒绝");
    check(float(web->handleCommand("state", {})["values"]["band2_freq"]) == 1234, "无效批次不能部分应用");
    web->handleCommand("end", {});
    web->handleCommand("undo", {});
    check(state["schema"]["band24_freq"].isObject(), "24 段参数定义缺失");
    check(state["frame"]["curves"]["1"].isArray(), "停播频响数据缺失");
    juce::AudioProcessor::TrackProperties track;
    track.name = juce::String::fromUTF8("钢琴 / Piano"); processor.updateTrackProperties(track);
    check(web->handleCommand("initialize", {})["frame"]["trackName"].toString() == *track.name, "宿主轨道名称未传到界面");
    processor.updateTrackProperties({});
    check(web->handleCommand("initialize", {})["frame"]["trackName"].toString().isEmpty(), "宿主未提供名称时不应保留旧名称");
    check(web->handleCommand("ui", json(R"({"spectrumResolution":4})")).hasProperty("error"), "非法分辨率未被拒绝");
    check(web->handleCommand("ui", json(R"({"spectrumTilt":2})")).hasProperty("error"), "非法显示倾斜未被拒绝");
    web->handleCommand("ui", json(R"({"spectrumPre":true,"spectrumRange":120,"spectrumResolution":3,"spectrumSpeed":2,"spectrumTilt":4.5})"));
    state = web->handleCommand("initialize", {});
    check(bool(state["ui"]["spectrumPre"]) && int(state["ui"]["spectrumResolution"]) == 3 && double(state["ui"]["spectrumTilt"]) == 4.5, "分析设置未正确写回");
    web->handleCommand("ui", json(R"({"spectrumPre":false,"spectrumRange":90,"spectrumResolution":2,"spectrumSpeed":1,"spectrumTilt":0})"));
    check(web->handleCommand("set", json(R"({"id":"band1_freq","value":-20})")).hasProperty("error"), "非法频率未被拒绝");
    check(web->handleCommand("set", json(R"({"id":"not_a_parameter","value":1})")).hasProperty("error"), "未知参数未被拒绝");
    check(web->handleCommand("ui", json(R"({"scale":0.2})")).hasProperty("error"), "非法缩放未被拒绝");
    state = web->handleCommand("addBand", json(R"({"freq":3000,"gain":-4})"));
    check(int(state["selected"]) == 7, "新增频段选择错误");
    check(float(state["values"]["band7_freq"]) == 3000, "新增频段频率错误");
    state = web->handleCommand("deleteBand", json(R"({"band":7})"));
    check(float(state["values"]["band7_present"]) == 0, "删除频段失败");
    web->handleCommand("begin", {});
    web->handleCommand("set", json(R"({"id":"band2_dynamic","value":1})"));
    web->handleCommand("set", json(R"({"id":"band2_dyn_auto","value":0})"));
    web->handleCommand("set", json(R"({"id":"band2_freq","value":1000})"));
    web->handleCommand("set", json(R"({"id":"band2_dyn_threshold","value":-30})"));
    web->handleCommand("end", {});
    juce::MidiBuffer midi; juce::AudioBuffer<float> audio(2, 512);
    for (int b = 0; b < 50; ++b) { for (int s = 0; s < 512; ++s) { const float x = .5f * static_cast<float>(std::sin(2 * juce::MathConstants<double>::pi * 1000 * (b * 512 + s) / 48000)); audio.setSample(0, s, x); audio.setSample(1, s, x); } processor.processBlock(audio, midi); }
    state = web->handleCommand("initialize", {});
    check(float(state["frame"]["dynamicReduction"]["2"]) > 4, "实际 DSP 衰减未通过桥接传到界面");
    check(state["frame"]["dynamicCurves"]["2"].isArray(), "实时动态响应曲线缺失");
    web->handleCommand("begin", {}); web->handleCommand("set", json(R"({"id":"band2_dyn_auto","value":1})")); web->handleCommand("end", {});
    for (int b = 0; b < 300; ++b)
    {
        for (int s = 0; s < 512; ++s) { const float x = .2f * std::sin(2 * juce::MathConstants<double>::pi * 1000 * (b * 512 + s) / 48000); audio.setSample(0, s, x); audio.setSample(1, s, x); }
        processor.processBlock(audio, midi);
    }
    state = web->handleCommand("initialize", {});
    check(bool(state["frame"]["dynamicReady"]["2"]), "自动阈值未完成学习");
    check(float(state["frame"]["dynamicDetector"]["2"]) > -30 && float(state["frame"]["dynamicThreshold"]["2"]) < -10, "原生检测电平或自动阈值未传到界面");
    check(float(state["values"]["band2_dyn_threshold"]) == -30, "自动阈值改写了保存的手动阈值");
    web->handleCommand("begin", {}); web->handleCommand("set", json(R"({"id":"band2_dynamic","value":0})")); web->handleCommand("end", {});
    check(web->handleCommand("audition", json(R"({"band":99,"active":true})")).hasProperty("error"), "非法监听频段未拒绝");
    const auto beforeAudition = processor.apvts.copyState().toXmlString();
    web->handleCommand("audition", json(R"({"band":2,"active":true})"));
    check(processor.getEqProcessor().getAuditionBand() == 1 && processor.getEqProcessor().isAnyBandSoloed(), "临时监听未接入声音引擎");
    check(processor.apvts.copyState().toXmlString() == beforeAudition, "临时监听污染可保存参数");
    web->handleCommand("audition", json(R"({"active":false})"));
    check(processor.getEqProcessor().getAuditionBand() == -1 && !processor.getEqProcessor().isAnyBandSoloed(), "松手后监听未解除");
    web->handleCommand("ui", json(R"({"scale":1.25,"theme":"dark"})"));
    check(web->getWidth() == 1375 && web->getHeight() == 966, "窗口缩放比例错误");
    web->handleCommand("ui", json(R"({"scale":1,"theme":"light"})"));
    processor.apvts.state.setProperty(Params::LINEAR_LEGACY_MASK, 1, nullptr);
    web->handleCommand("begin", {}); web->handleCommand("set", json(R"({"id":"eq_mode","value":1})")); web->handleCommand("end", {});
    check(int(web->handleCommand("initialize", {})["linearLegacyMask"]) == 1, "旧工程兼容标记未传入页面");
    web->handleCommand("begin", {}); web->handleCommand("set", json(R"({"id":"band1_dynamic","value":1})")); web->handleCommand("end", {});
    check(processor.isLinearDynamicAllowed(0), "显式开启动态没有解除旧工程保护");
    state = web->handleCommand("undo", {});
    check(!processor.isLinearDynamicAllowed(0) && int(state["linearLegacyMask"]) == 1, "原生撤销没有还原兼容标记");
    web->handleCommand("redo", {}); check(processor.isLinearDynamicAllowed(0), "原生重做没有还原线性动态升级");
    web->handleCommand("begin", {}); web->handleCommand("set", json(R"({"id":"eq_mode","value":0})")); web->handleCommand("end", {});
    std::cout << (passed ? "原生桥接验证通过" : "原生桥接验证失败") << std::endl;
    const bool verifyWeb = argc > 1 && juce::String(argv[1]) == "--verify-web";
    if (verifyWeb || (argc > 1 && juce::String(argv[1]) == "--interactive"))
    {
        struct Window : juce::DocumentWindow
        {
            Window() : DocumentWindow(juce::String::fromUTF8("SanMu EQ 同源界面预览"), juce::Colours::black, closeButton) {}
            void closeButtonPressed() override { juce::MessageManager::getInstance()->stopDispatchLoop(); }
        } window;
        struct WebCheck : juce::Timer
        {
            juce::WebBrowserComponent& browser;
            bool& passed;
            bool started = false, pending = false;
            int attempts = 0;
            WebCheck(juce::WebBrowserComponent& b, bool& p) : browser(b), passed(p) { startTimer(250); }
            void timerCallback() override
            {
                if (++attempts > 80) { passed = false; stopTimer(); std::cerr << "内嵌页面验证超时" << std::endl; juce::MessageManager::getInstance()->stopDispatchLoop(); return; }
                if (pending) return;
                pending = true;
                browser.evaluateJavascript(started ? "window.nativeUiCheck || ''" : "Boolean(window.eqApp?.graph?.buttons.size)", [this](auto result)
                {
                    pending = false;
                    const auto* value = result.getResult();
                    if (!started)
                    {
                        if (value == nullptr || !bool(*value)) return;
                        started = true;
                        browser.evaluateJavascript(juce::String::fromUTF8(R"JS(
                          (async () => {
                            const idle=async()=>{await eqApp.drainPendingSets();await eqApp.queue;await new Promise(r=>setTimeout(r,40));await eqApp.queue;};
                            try {
                              if (!eqApp.schema.band24_freq || document.querySelectorAll('.eq-node').length !== 6) throw Error('页面初始化失败: '+document.querySelectorAll('.eq-node').length+'/'+eqApp.bands().join(','));
                              await eqApp.request('begin', {}, false);
                              await eqApp.request('set', {id: 'band3_freq', value: 1500}, false);
                              await eqApp.request('end');
                              if (eqApp.values.band3_freq !== 1500) throw Error('参数写回失败');
                              await eqApp.command('addBand', {freq: 3000});
                              if (eqApp.selected !== 7 || !eqApp.values.band7_present) throw Error('新增频段失败');
                              await eqApp.command('deleteBand', {band: 7});
                              await eqApp.command('undo');
                              if (!eqApp.values.band7_present) throw Error('撤销失败');
                              await eqApp.command('redo');
                              if (eqApp.values.band7_present) throw Error('重做失败');
                              await eqApp.command('ab', {slot: 'B'});
                              await eqApp.command('ab', {slot: 'A'});
                              if (eqApp.values.band3_freq !== 1500) throw Error('首次快照切换丢失参数');
                              if (document.querySelectorAll('input[name=mode]').length !== 3 || document.querySelector('#latency')) throw Error('模式或底栏未更新');
                              eqApp.select(3);eqApp.chooseMode(2); await idle();
                              if (eqApp.values.eq_mode !== 0 || eqApp.values.character_mode !== 2) throw Error('Discrete 映射失败');
                              eqApp.chooseMode(0); await idle();
                              if (eqApp.values.eq_mode !== 0 || eqApp.values.character_mode !== 0) throw Error('Clean 映射失败');
                              eqApp.toggleLinearPhase(); await idle();
                              if (eqApp.values.eq_mode !== 1 || document.querySelector('#band-dynamic').disabled) throw Error('线性相位设置映射失败: '+eqApp.values.eq_mode+'/'+eqApp.selected+'/'+document.querySelector('#band-dynamic').disabled);
                              await eqApp.chooseLinearQuality(1);
                              if (eqApp.linearQuality !== 1 || !document.querySelector('#linear-quality')) throw Error('线性相位精度设置失败: '+eqApp.linearQuality+'/'+document.querySelector('#linear-quality').value+'/'+eqApp.values.eq_mode);
                              eqApp.toggleLinearPhase(); await idle();
                              eqApp.select(3); eqApp.begin(); eqApp.set('band3_q',1); eqApp.end(); await idle();
                              if (eqApp.knobs[2].normalized() !== .5) throw Error('Q 中点错误');
                              if (!eqApp.schema.band24_dyn_auto || document.querySelector('input[type=checkbox]')) throw Error('自动阈值或按钮资源未更新');
                              eqApp.begin(); eqApp.set('band3_type',3); eqApp.end(); await idle();
                              if (!eqApp.knobs[1].disabled || eqApp.knobs[8].disabled || document.querySelector('#gain-knob').hidden) throw Error('增益禁用或斜率旋钮失败');
                              eqApp.begin(); eqApp.set('band3_type',0); eqApp.set('band3_dynamic',1); eqApp.set('band3_dyn_auto',1); eqApp.end(); await idle();
                              if (!document.querySelector('#threshold-knob input').disabled) throw Error('自动阈值界面未锁定手动调节');
                              const created=eqApp.beginBand(2310,{gain:3,q:2,placement:3});eqApp.end();await idle();
                              if(created!==7||!eqApp.values.band7_present||Math.abs(eqApp.values.band7_gain-3)>.001||eqApp.values.band7_placement!==3) throw Error('候选频点新增事务未写回');
                              eqApp.tools.show(7);
                              if(document.querySelector('#node-tools').hidden||!document.querySelector('.node-gain').value.includes('+3.00')) throw Error('小卡片读数缺失');
                              const input=document.querySelector('.node-frequency');input.focus();input.value='2.4 kHz';input.dispatchEvent(new KeyboardEvent('keydown',{key:'Enter',bubbles:true}));await idle();
                              if(Math.abs(eqApp.values.band7_freq-2400)>1) throw Error('信息卡直接输入未写回宿主');
                              await eqApp.command('undo');
                              if(!document.querySelector('#analyser-settings')||document.querySelector('#spectrum svg')) throw Error('频谱设置资源未更新');
                              await eqApp.command('undo');
                              if(eqApp.values.band7_present) throw Error('候选频点新增事务未一次撤销');
                              eqApp.select(3);eqApp.tools.show(3);eqApp.tools.expand();
                              if(document.querySelector('#band-action-menu').hidden) throw Error('频段扩展菜单未加载');
                              eqApp.tools.close();
                              window.nativeUiCheck = '通过';
                            } catch (e) { window.nativeUiCheck = e.message; }
                          })();
                        )JS"));
                        return;
                    }
                    if (value == nullptr || value->toString().isEmpty()) return;
                    passed = passed && value->toString() == juce::String::fromUTF8("通过");
                    std::cout << "内嵌页面验证：" << value->toString() << std::endl;
                    stopTimer(); juce::MessageManager::getInstance()->stopDispatchLoop();
                });
            }
        };
        window.setUsingNativeTitleBar(true); window.setContentNonOwned(web, true);
        window.centreWithSize(web->getWidth(), web->getHeight()); window.setVisible(true);
        std::unique_ptr<WebCheck> webCheck;
        if (verifyWeb) webCheck = std::make_unique<WebCheck>(*dynamic_cast<juce::WebBrowserComponent*>(web->getChildComponent(0)), passed);
        juce::MessageManager::getInstance()->runDispatchLoop(); window.clearContentComponent();
    }
    // 内部旁通时仍显示真实信号，宿主停止提交音频后才按时间释放。
    web->handleCommand("begin", {}); web->handleCommand("set", json(R"({"id":"global_bypass","value":1})")); web->handleCommand("end", {});
    const auto sequence = processor.getEqProcessor().getMeterSequence();
    for (int b = 0; b < 20; ++b) { audio.clear(); for (int s = 0; s < 512; ++s) { audio.setSample(0, s, .2f); audio.setSample(1, s, .1f); } processor.processBlock(audio, midi); }
    state = web->handleCommand("initialize", {});
    const float peak = float(state["frame"]["meters"][0]);
    check(processor.getEqProcessor().getMeterSequence() == sequence + 20 && peak > -20, "旁通时真实电平被错误停住或归零");
    juce::Thread::sleep(450);
    check(float(web->handleCommand("initialize", {})["frame"]["meters"][0]) < peak - 10, "宿主停回调后电平未自然释放");
    web->handleCommand("audition", json(R"({"band":2,"active":true})"));
    editor.reset();
    check(processor.getEqProcessor().getAuditionBand() == -1, "关闭编辑器后监听未解除");
    processor.releaseResources();
    return passed ? 0 : 2;
}
