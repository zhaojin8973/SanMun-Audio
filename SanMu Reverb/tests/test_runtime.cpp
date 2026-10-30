#include "../source/PluginProcessor.h"
#include "../source/presets/PresetManager.h"
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <new>

namespace
{
thread_local bool auditAllocations = false;
thread_local std::size_t allocations = 0;
}

void* operator new(std::size_t size)
{
    if (auditAllocations) ++allocations;
    if (void* data = std::malloc(size == 0 ? 1 : size)) return data;
    throw std::bad_alloc();
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* data) noexcept { std::free(data); }
void operator delete[](void* data) noexcept { std::free(data); }
void* operator new(std::size_t size, std::align_val_t alignment)
{
    if (auditAllocations) ++allocations;
    void* data = nullptr;
    if (posix_memalign(&data, static_cast<std::size_t>(alignment), size == 0 ? 1 : size) == 0) return data;
    throw std::bad_alloc();
}
void* operator new[](std::size_t size, std::align_val_t alignment) { return ::operator new(size, alignment); }
void operator delete(void* data, std::align_val_t) noexcept { std::free(data); }
void operator delete[](void* data, std::align_val_t) noexcept { std::free(data); }

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    int failures = 0;
    const auto check = [&](bool ok, const char* text) {
        std::printf("%s：%s\n", ok ? "通过" : "失败", text);
        if (!ok) ++failures;
    };
    SanMuReverbAudioProcessor processor;
    const auto enter = [&](const char* id, const char* text, float expected) {
        auto* parameter = processor.apvts.getParameter(id);
        const float actual = parameter->convertFrom0to1(parameter->getValueForText(juce::String::fromUTF8(text)));
        const bool ok = std::abs(actual - expected) < .001f;
        if (!ok) std::printf("输入 %s / %s：期望 %.4f，实际 %.4f\n", id, text, expected, actual);
        check(ok, "实际宿主参数文本解析结果正确");
    };
    enter("high_cut", "8.5 kHz", 8500);
    enter("high_freq", "8.5千赫兹", 8500);
    enter("predelay", ".035秒", 35);
    enter("decay", "2500 ms", 2.5f);
    enter("bass_mult", "1.25×", 1.25f);
    enter("high_shelf", "-12 分贝", -12);
    enter("mix", "200%", 100);
    enter("high_cut", "999999999999999999999999999999999999999 Hz", 20000);
    auto* frequency = processor.apvts.getParameter("high_cut");
    frequency->setValueNotifyingHost(frequency->convertTo0to1(9321));
    for (const auto* text : {"错误", "", "8.5 ms", "8.5kHz垃圾", "1.2.3", "nan", "inf", "1e3"})
        enter("high_cut", text, 9321);
    bool formatting = true;
    for (const auto& id : Params::getAllKnobParamIds())
    {
        auto* parameter = processor.apvts.getParameter(id);
        for (float normalised : {0.0f, .5f, 1.0f})
        {
            const auto text = parameter->getText(normalised, 64);
            formatting &= std::abs(parameter->convertFrom0to1(parameter->getValueForText(text))
                - parameter->convertFrom0to1(normalised)) < .001f;
        }
    }
    check(formatting, "全部旋钮显示文本可往返为相同物理值");

    bool roundtrips = true;
    for (int i = 0; i < PresetManager::getNumFactoryPresets(); ++i)
    {
        PresetManager::applyPreset(PresetManager::getFactoryPreset(i), processor.apvts);
        const auto expected = PresetManager::snapshotCurrent({}, {}, processor.apvts).values;
        juce::MemoryBlock data; processor.getStateInformation(data);
        PresetManager::applyPreset(PresetManager::getFactoryPreset((i + 1) % 23), processor.apvts);
        processor.setStateInformation(data.getData(), static_cast<int>(data.getSize()));
        roundtrips &= PresetManager::snapshotCurrent({}, {}, processor.apvts).values == expected;
    }
    check(roundtrips, "23 个工厂预设的全部 17 项宿主参数保存恢复一致");
    const auto validState = processor.apvts.copyState();
    const auto expected = PresetManager::snapshotCurrent({}, {}, processor.apvts).values;
    const auto restore = [&](juce::ValueTree state) {
        juce::MemoryBlock data; juce::AudioProcessor::copyXmlToBinary(*state.createXml(), data);
        processor.setStateInformation(data.getData(), static_cast<int>(data.getSize()));
    };
    for (int variant = 0; variant < 6; ++variant)
    {
        restore(validState.createCopy());
        auto invalid = validState.createCopy();
        auto decay = invalid.getChildWithProperty("id", "decay");
        if (variant == 0) decay.setProperty("value", "nan", nullptr);
        if (variant == 1) decay.setProperty("value", "错误", nullptr);
        if (variant == 2) decay.setProperty("value", 2000, nullptr);
        if (variant == 3) invalid.removeChild(decay, nullptr);
        if (variant == 4) invalid.getChildWithProperty("id", "mode").setProperty("value", .5, nullptr);
        if (variant == 5) invalid.addChild(decay.createCopy(), -1, nullptr);
        restore(invalid);
        check(PresetManager::snapshotCurrent({}, {}, processor.apvts).values == expected,
              "损坏宿主状态被拒绝且完整保留当前参数");
    }
    restore(validState.createCopy());

    juce::MidiBuffer midi;
    auto* category = processor.apvts.getParameter("category");
    auto* mode = processor.apvts.getParameter("mode");
    const auto process = [&](juce::AudioBuffer<float>& buffer) {
        auditAllocations = true;
        processor.processBlock(buffer, midi);
        auditAllocations = false;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                if (!std::isfinite(buffer.getSample(ch, i)) || std::abs(buffer.getSample(ch, i)) > 25) return false;
        return true;
    };
    bool stable = true;
    int combinations = 0;
    for (int channels : {1, 2})
    {
        processor.setPlayConfigDetails(channels, channels, 48000, 32);
        check(processor.getTotalNumInputChannels() == channels && processor.getTotalNumOutputChannels() == channels,
              "单声道与立体声总线可配置");
        for (double rate : {44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0})
        {
            processor.prepareToPlay(rate, 32);
            for (int size : {1, 16, 32, 64, 127, 512, 2048})
            {
                juce::AudioBuffer<float> buffer(channels, size);
                for (int space = 0; space < 4; ++space)
                    for (int style = 0; style < 2; ++style)
                    {
                        category->setValueNotifyingHost(category->convertTo0to1(static_cast<float>(space)));
                        mode->setValueNotifyingHost(mode->convertTo0to1(static_cast<float>(Params::menuIndexToMode(space, style))));
                        for (int block = 0; block < 4; ++block)
                        {
                            buffer.clear();
                            for (int ch = 0; ch < channels; ++ch) buffer.setSample(ch, 0, .2f);
                            stable &= process(buffer);
                        }
                        ++combinations;
                    }
            }
            juce::AudioBuffer<float> empty(channels, 0); stable &= process(empty);
            processor.releaseResources();
        }
    }
    std::printf("覆盖采样率、块长、声道与风格组合：%d\n", combinations);
    check(stable, "小缓冲、超出准备块长、空块与快速切换均输出有限值");
    check(allocations == 0, "音频处理回调中的 C++ 动态分配为零");

    processor.prepareToPlay(48000, 32);
    juce::AudioBuffer<float> buffer(2, 32);
    buffer.clear(); buffer.setSample(0, 0, std::numeric_limits<float>::quiet_NaN());
    buffer.setSample(1, 0, std::numeric_limits<float>::infinity());
    check(process(buffer), "无效音频采样不会污染输出");
    restore(validState.createCopy());
    buffer.clear(); check(process(buffer) && buffer.getMagnitude(0, 32) <= 0, "恢复状态在音频回调中清理旧尾音");
    bool reopen = true;
    const auto beforeEditors = PresetManager::snapshotCurrent({}, {}, processor.apvts).values;
    for (int i = 0; i < 12; ++i)
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
        reopen &= editor != nullptr && editor->getWidth() > 0 && editor->getHeight() > 0;
    }
    check(reopen && beforeEditors == PresetManager::snapshotCurrent({}, {}, processor.apvts).values,
          "原生编辑器连续关闭重开 12 次保持参数");

    std::array<std::unique_ptr<SanMuReverbAudioProcessor>, 8> instances;
    for (int i = 0; i < 8; ++i)
    {
        instances[static_cast<std::size_t>(i)] = std::make_unique<SanMuReverbAudioProcessor>();
        auto& instance = *instances[static_cast<std::size_t>(i)];
        instance.prepareToPlay(48000, 32);
        PresetManager::applyPreset(PresetManager::getFactoryPreset(i * 3), instance.apvts);
    }
    bool multi = true;
    const auto start = std::chrono::steady_clock::now();
    for (int block = 0; block < 1500; ++block)
        for (auto& instance : instances)
        {
            buffer.clear(); buffer.setSample(0, 0, .1f); buffer.setSample(1, 0, -.1f);
            auditAllocations = true; instance->processBlock(buffer, midi); auditAllocations = false;
            multi &= std::isfinite(buffer.getMagnitude(0, 32)) && buffer.getMagnitude(0, 32) < 25;
        }
    const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    std::printf("8 实例串行处理每实例 1 秒音频：%.3f 秒；C++ 动态分配：%zu\n", elapsed, allocations);
    check(multi && allocations == 0, "8 实例、32 采样缓冲连续运行无异常或 C++ 动态分配");
    std::printf("运行期检查：%d 项失败\n", failures);
    return failures == 0 ? 0 : 1;
}
