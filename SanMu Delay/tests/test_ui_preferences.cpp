// 由真实处理器和真实编辑器验证偏好隔离；写入、读取可在两个独立进程运行。
#include "../source/PluginEditor.h"
#include <iostream>
#include <stdexcept>
#define CHECK(value, text) do { if (!(value)) throw std::runtime_error(text); } while (false)
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    CHECK(juce::SystemStats::getEnvironmentVariable("SANMU_TEST_PREFERENCES_DIR", {}).isNotEmpty(), "测试必须指定隔离目录");
    SanMuDelayAudioProcessor first;
    auto* sound = first.apvts.getParameter("time_l"); CHECK(sound != nullptr, "声音参数不存在");
    sound->setValueNotifyingHost(.63f);
    const float expectedSound = sound->getValue();
    juce::MemoryBlock oldProject; first.getStateInformation(oldProject);
    const bool readOnly = argc > 1 && juce::String(argv[1]) == "--read";
    if (!readOnly) {
        SanMuDelayAudioProcessorEditor a(first); a.handleCommand("initialize", {});
        const auto result = a.handleCommand("ui", juce::JSON::parse(R"({"theme":"dark","language":"zh","tips":"off","tooltipLanguage":"off","history":false,"visualsEnabled":false,"scale":1.1,"uiScale":1.1,"grRange":24,"levelRange":30,"spectrumRange":120})"));
        CHECK(!result.hasProperty("error"), "偏好写入失败");
    }
    {
        SanMuDelayAudioProcessor restored; restored.setStateInformation(oldProject.getData(), int(oldProject.getSize()));
        SanMuDelayAudioProcessorEditor b(restored); const auto state = b.handleCommand("initialize", {}), ui = state["ui"];
        CHECK(ui["theme"] == "dark" && ui["language"] == "zh", "新实例或旧工程覆盖了个人主题语言");
        CHECK(b.getWidth() == 1210, "窗口缩放未跨实例保存");
        CHECK(ui["tips"] == "off", "悬停提示设置未跨实例保存");
        CHECK(std::abs(restored.apvts.getParameter("time_l")->getValue() - expectedSound) < .0001f, "偏好错误改变声音参数");
    }
    // 直接检查公共持久化规则：陈旧窗口只写变化字段，排除声音和历史快照。
    const auto directory = juce::File(juce::SystemStats::getEnvironmentVariable("SANMU_TEST_PREFERENCES_DIR", {})).getChildFile("merge");
    SanMuUiPreferences one("isolated", directory), two("isolated", directory);
    const auto defaults = juce::JSON::parse(R"({"theme":"light","language":"en"})");
    auto x = one.load(defaults), y = two.load(defaults);
    x.getDynamicObject()->setProperty("theme", "dark"); CHECK(one.save(x), "第一个窗口保存失败");
    y.getDynamicObject()->setProperty("language", "zh"); y.getDynamicObject()->setProperty("snapshot", juce::JSON::parse(R"({"gain":24})"));
    CHECK(two.save(y), "第二个窗口保存失败");
    SanMuUiPreferences three("isolated", directory); const auto merged = three.load(defaults);
    CHECK(merged["theme"] == "dark" && merged["language"] == "zh" && !merged.hasProperty("snapshot"), "陈旧窗口覆盖了新设置或泄露声音状态");
    directory.getChildFile("corrupt.json").replaceWithText("{broken");
    SanMuUiPreferences corrupt("corrupt", directory);
    CHECK(corrupt.load(defaults)["theme"] == "light", "损坏文件应安全回退");
    std::cout << "通过：SanMu Delay 的跨实例、跨进程、旧工程、偏好合并与声音隔离" << std::endl;
}
