#include "PluginEditor.h"
#include "presets/PresetManager.h"
#include "params/StateCompatibility.h"
#include "SanMuWebAssets.h"
#include <cmath>

namespace
{
const std::array<std::pair<const char*, const char*>, 15> parameterMap {{
    {"decay", "decay"}, {"predelay", "predelay"}, {"mix", "mix"},
    {"highHz", "high_freq"}, {"highDb", "high_shelf"}, {"bassHz", "bass_freq"}, {"bassMult", "bass_mult"},
    {"size", "size"}, {"attack", "attack"}, {"early", "early_diff"}, {"late", "late_diff"},
    {"rate", "mod_rate"}, {"depth", "mod_depth"}, {"lowCut", "low_cut"}, {"highCut", "high_cut"}
}};
int visibleMode(int category, int mode)
{
    constexpr std::array<int, 4> family {1, 2, 0, 3};
    return family[static_cast<size_t>(juce::jlimit(0, 3, category))] * 2 + Params::modeToMenuIndex(category, mode);
}
juce::var failure(const char* message)
{
    auto* result = new juce::DynamicObject(); result->setProperty("error", juce::String::fromUTF8(message)); return result;
}
juce::var presetToVar(const ReverbPreset& preset, const juce::String& id, bool user)
{
    auto* result = new juce::DynamicObject(); auto* values = new juce::DynamicObject();
    for (const auto& [web, native] : parameterMap)
        values->setProperty(web, std::round(preset.values.at(native) * 10000.0) / 10000.0);
    result->setProperty("values", values); result->setProperty("name", preset.name);
    result->setProperty("mode", visibleMode(juce::roundToInt(preset.values.at("category")), juce::roundToInt(preset.values.at("mode"))));
    result->setProperty("id", id); result->setProperty("user", user); return result;
}
std::optional<juce::WebBrowserComponent::Resource> resource(const juce::String& url)
{
    // 每次请求独立读取内嵌压缩包，多个插件实例不会共享可变流位置。
    juce::MemoryInputStream input(SanMuWebAssets::sanmuui_zip, SanMuWebAssets::sanmuui_zipSize, false);
    juce::ZipFile archive(input);
    const auto path = url == "/" ? juce::String("studio.html") : url.substring(1).upToFirstOccurrenceOf("?", false, false);
    if (path.contains("..")) return {};
    const auto* entry = archive.getEntry(path); if (entry == nullptr) return {};
    std::unique_ptr<juce::InputStream> stream(archive.createStreamForEntry(*entry)); if (!stream) return {};
    juce::MemoryBlock block; stream->readIntoMemoryBlock(block);
    const auto* start = static_cast<const std::byte*>(block.getData());
    const auto ext = path.fromLastOccurrenceOf(".", false, false);
    const juce::String mime = ext == "js" ? "text/javascript" : ext == "css" ? "text/css" : ext == "html" ? "text/html" : "application/octet-stream";
    return juce::WebBrowserComponent::Resource { std::vector<std::byte>(start, start + block.getSize()), mime };
}
}

SanMuReverbAudioProcessorEditor::SanMuReverbAudioProcessorEditor(SanMuReverbAudioProcessor& processor)
    : AudioProcessorEditor(processor), processorRef(processor)
{
    auto options = juce::WebBrowserComponent::Options().withNativeIntegrationEnabled()
        .withInitialisationData("sanmu", "2026.09.10")
        #if JUCE_WINDOWS
        .withBackend(juce::WebBrowserComponent::Options::Backend::webview2)
        #endif
        .withResourceProvider(resource)
        .withNativeFunction("sanmu", [this](const juce::Array<juce::var>& arguments, auto complete) {
            juce::ScopedValueSetter<bool> active(nativeCallbackActive, true);
            complete(arguments.size() == 2 ? handleCommand(arguments[0].toString(), arguments[1]) : failure("无效插件请求"));
        });
    browser = std::make_unique<Browser>(options);
    addAndMakeVisible(*browser);
    setResizable(true, false); setResizeLimits(720, 648, 1920, 1468);
    const auto saved = preferences.load(juce::JSON::parse(processorRef.getEditorState()));
    processorRef.setEditorState(juce::JSON::toString(saved));
    constexpr int defaultWidth = 1100;
    constexpr int defaultHeight = 700;
    const int width = saved.hasProperty("width") ? static_cast<int>(saved["width"]) : defaultWidth;
    const int savedHeight = saved.hasProperty("height") ? static_cast<int>(saved["height"]) : defaultHeight;
    // 仅迁移旧版默认尺寸，用户在新版手动调整的窗口大小仍正常恢复。
    const bool legacyDefault = !saved.hasProperty("layoutVersion") && width == 1100 && savedHeight == 850;
    const int height = legacyDefault ? defaultHeight : savedHeight;
    setSize(juce::jlimit(720, 1920, width), juce::jlimit(620, 1440, height) + 28);
    VisualMeter::Frame old; while (processorRef.getReverb().visualMeter.pop(old)) {}
    processorRef.getReverb().visualMeter.enabled.store(true, std::memory_order_relaxed);
    addAndMakeVisible(betaNotice);
    browser->goToURL(juce::WebBrowserComponent::getResourceProviderRoot());
    startTimerHz(30);
}
SanMuReverbAudioProcessorEditor::~SanMuReverbAudioProcessorEditor()
{
    ready = false; cancelPendingUpdate(); stopTimer(); flushResizePreferences(); finishGesture();
    processorRef.getReverb().visualMeter.enabled.store(false, std::memory_order_relaxed);
    browser.reset();
}
void SanMuReverbAudioProcessorEditor::paint(juce::Graphics& g) { g.fillAll(juce::Colour(0xffe9ecef)); }
void SanMuReverbAudioProcessorEditor::resized()
{
    auto content = getLocalBounds();
    // 宿主重挂载或显示比例变化期间可能短暂给出空区域，保持上次有效网页尺寸。
    if (content.getWidth() <= 0 || content.getHeight() <= 28) return;
    betaNotice.setBounds(content.removeFromBottom(28));
    if (browser && browser->getBounds() != content) browser->setBounds(content);
    if (ready) resizeSaveDeadline = juce::Time::getMillisecondCounterHiRes() + 180;
}
void SanMuReverbAudioProcessorEditor::requestWindowScale(double scale)
{
    pendingWidth = juce::roundToInt(1100 * scale);
    pendingHeight = juce::roundToInt(700 * scale) + 28;
    if (getWidth() != pendingWidth || getHeight() != pendingHeight)
    {
        if (nativeCallbackActive) triggerAsyncUpdate();
        else handleAsyncUpdate();
    }
}
void SanMuReverbAudioProcessorEditor::handleAsyncUpdate()
{
    if (pendingWidth > 0 && pendingHeight > 28 && (getWidth() != pendingWidth || getHeight() != pendingHeight))
        setSize(pendingWidth, pendingHeight);
}
void SanMuReverbAudioProcessorEditor::flushResizePreferences()
{
    if (resizeSaveDeadline <= 0) return;
    resizeSaveDeadline = 0;
    auto state = juce::JSON::parse(processorRef.getEditorState());
    if (!state.isObject()) state = new juce::DynamicObject();
    state.getDynamicObject()->setProperty("width", getWidth());
    state.getDynamicObject()->setProperty("height", getHeight() - 28);
    state.getDynamicObject()->setProperty("layoutVersion", 2);
    preferences.save(state);
    processorRef.setEditorState(juce::JSON::toString(state));
}

juce::var SanMuReverbAudioProcessorEditor::state() const
{
    auto* result = new juce::DynamicObject(); auto* values = new juce::DynamicObject();
    for (const auto& [web, native] : parameterMap)
        values->setProperty(web, std::round(processorRef.apvts.getRawParameterValue(native)->load() * 10000.0) / 10000.0);
    result->setProperty("values", values);
    result->setProperty("mode", visibleMode(juce::roundToInt(processorRef.apvts.getRawParameterValue("category")->load()),
        juce::roundToInt(processorRef.apvts.getRawParameterValue("mode")->load())));
    result->setProperty("bypass", processorRef.apvts.getRawParameterValue("bypass")->load() > .5f);
    result->setProperty("sampleRate", processorRef.getSampleRate() > 0 ? processorRef.getSampleRate() : 48000);
    result->setProperty("channels", processorRef.getTotalNumInputChannels());
    return result;
}
juce::var SanMuReverbAudioProcessorEditor::presets() const
{
    juce::Array<juce::var> result;
    for (int i = 0; i < PresetManager::getNumFactoryPresets(); ++i)
        result.add(presetToVar(PresetManager::getFactoryPreset(i), "native-factory-" + juce::String(i), false));
    // 仅读取预设文件，不将枚举操作应用到当前实例。
    for (const auto& file : PresetManager::getUserPresetFiles())
    {
        const auto xml = juce::XmlDocument::parse(file); if (!xml) continue;
        auto tree = juce::ValueTree::fromXml(*xml);
        if (!state_compatibility::isValid(tree, processorRef.apvts)) continue;
        state_compatibility::migrate(tree);
        ReverbPreset preset; preset.name = file.getFileNameWithoutExtension();
        for (const auto& [web, native] : parameterMap)
        { juce::ignoreUnused(web); preset.values[native] = static_cast<float>(tree.getChildWithProperty("id", native)["value"]); }
        for (const auto* id : {"category", "mode"}) preset.values[id] = static_cast<float>(tree.getChildWithProperty("id", id)["value"]);
        result.add(presetToVar(preset, "native-user-" + file.getFileName(), true));
    }
    return result;
}
void SanMuReverbAudioProcessorEditor::setParameter(const juce::String& id, float value)
{
    auto* parameter = processorRef.apvts.getParameter(id);
    const auto current = processorRef.apvts.getRawParameterValue(id)->load();
    if (std::abs(current - value) < .00001f) return;
    if (gestures.insert(id).second) parameter->beginChangeGesture();
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
void SanMuReverbAudioProcessorEditor::finishGesture()
{
    for (const auto& id : gestures) processorRef.apvts.getParameter(id)->endChangeGesture();
    gestures.clear(); transaction = false;
}
juce::var SanMuReverbAudioProcessorEditor::handleCommand(const juce::String& command, const juce::var& data)
{
    if (command == "initialize")
    {
        ready = true; auto result = state();
        result.getDynamicObject()->setProperty("presets", presets());
        result.getDynamicObject()->setProperty("ui", juce::JSON::parse(processorRef.getEditorState()));
        result.getDynamicObject()->setProperty("build", "2026.09.10"); return result;
    }
    if (command == "resize")
    {
        const auto scale = data["scale"];
        const bool numeric = scale.isDouble() || scale.isInt() || scale.isInt64();
        if (!data.isObject() || !numeric || !std::isfinite(double(scale)) || double(scale) < .85 || double(scale) > 1.5)
            return failure("缩放无效");
        requestWindowScale(double(scale));
        auto* result = new juce::DynamicObject();
        result->setProperty("width", pendingWidth);
        result->setProperty("height", pendingHeight - 28);
        result->setProperty("scale", scale);
        return result;
    }
    if (command == "begin") { transaction = true; return true; }
    if (command == "end") { finishGesture(); return state(); }
    if (command == "patch")
    {
        if (!data.isObject() || !data["values"].isObject()) return failure("参数请求不完整");
        // 先验证整批增量，再写入；未知参数或异常值不得留下部分修改。
        const auto& values = data["values"].getDynamicObject()->getProperties();
        for (const auto& entry : values)
        {
            const auto mapping = std::find_if(parameterMap.begin(), parameterMap.end(),
                [&](const auto& item) { return entry.name.toString() == item.first; });
            if (mapping == parameterMap.end()) return failure("未知参数");
            const auto& value = entry.value;
            const double number = static_cast<double>(value);
            const auto range = processorRef.apvts.getParameter(mapping->second)->getNormalisableRange();
            if (!(value.isDouble() || value.isInt() || value.isInt64()) || !std::isfinite(number)
                || number < range.start - .0001 || number > range.end + .0001) return failure("参数超出有效范围");
        }
        if (data.hasProperty("mode") && (!data["mode"].isInt()
            || static_cast<int>(data["mode"]) < 0 || static_cast<int>(data["mode"]) > 7)) return failure("空间类型无效");
        if (data.hasProperty("bypass") && !data["bypass"].isBool()) return failure("旁通状态无效");
        if (data.hasProperty("mode"))
        {
            const int requested = static_cast<int>(data["mode"]);
            constexpr std::array<int, 4> categories {2, 0, 1, 3};
            const int category = categories[static_cast<size_t>(requested / 2)];
            setParameter("category", static_cast<float>(category));
            setParameter("mode", static_cast<float>(Params::menuIndexToMode(category, requested % 2)));
        }
        for (const auto& [web, native] : parameterMap)
            if (data["values"].hasProperty(web)) setParameter(native, static_cast<float>(data["values"][web]));
        if (data.hasProperty("bypass")) setParameter("bypass", static_cast<bool>(data["bypass"]) ? 1.0f : 0.0f);
        if (!transaction) finishGesture();
        return state();
    }
    if (command == "apply")
    {
        if (!data.isObject() || !data["values"].isObject() || !data["mode"].isInt() || !data["bypass"].isBool())
            return failure("参数请求不完整");
        for (const auto& [web, native] : parameterMap)
        {
            const auto value = data["values"][web];
            const double number = static_cast<double>(value);
            const auto range = processorRef.apvts.getParameter(native)->getNormalisableRange();
            if (!(value.isDouble() || value.isInt() || value.isInt64()) || !std::isfinite(number)
                || number < range.start - .0001 || number > range.end + .0001) return failure("参数超出有效范围");
        }
        const int requested = static_cast<int>(data["mode"]);
        if (requested < 0 || requested > 7) return failure("空间类型无效");
        if (static_cast<int>(state()["mode"]) != requested)
        {
            constexpr std::array<int, 4> categories {2, 0, 1, 3};
            const int category = categories[static_cast<size_t>(requested / 2)];
            setParameter("category", static_cast<float>(category));
            setParameter("mode", static_cast<float>(Params::menuIndexToMode(category, requested % 2)));
        }
        for (const auto& [web, native] : parameterMap) setParameter(native, static_cast<float>(data["values"][web]));
        setParameter("bypass", static_cast<bool>(data["bypass"]) ? 1.0f : 0.0f);
        if (!transaction) finishGesture();
        return state();
    }
    if (command == "savePreset")
    {
        const auto name = data["name"].toString().trim();
        if (name.isEmpty() || name.length() > 60) return failure("请输入 1 至 60 个字符的预设名称");
        const auto file = PresetManager::saveUserPreset(name, processorRef.apvts);
        if (!file.existsAsFile()) return failure("预设保存失败，请检查目录权限");
        auto* result = new juce::DynamicObject(); result->setProperty("presets", presets());
        result->setProperty("id", "native-user-" + file.getFileName());
        result->setProperty("name", file.getFileNameWithoutExtension()); return result;
    }
    if (command == "ui")
    {
        if (!data.isObject()) return failure("界面状态无效");
        auto copy = data.clone(); copy.getDynamicObject()->setProperty("width", getWidth());
        copy.getDynamicObject()->setProperty("height", getHeight() - 28);
        copy.getDynamicObject()->setProperty("layoutVersion", 2);
        const auto json = juce::JSON::toString(copy, true);
        if (json.length() > 524288) return failure("界面状态过大");
        if (!preferences.save(copy)) return failure("界面偏好保存失败，请检查用户目录权限");
        processorRef.setEditorState(json); return true;
    }
    return failure("未知插件请求");
}
void SanMuReverbAudioProcessorEditor::timerCallback()
{
    if (resizeSaveDeadline > 0 && juce::Time::getMillisecondCounterHiRes() >= resizeSaveDeadline) flushResizePreferences();
    if (!ready) return;
    auto packet = state(); juce::Array<juce::var> frames;
    VisualMeter::Frame frame;
    while (processorRef.getReverb().visualMeter.pop(frame))
    {
        auto* item = new juce::DynamicObject();
        const auto array = [](const auto& values) { juce::Array<juce::var> a; for (const auto value : values) a.add(value); return juce::var(a); };
        item->setProperty("dry", array(frame.dry)); item->setProperty("wet", array(frame.wet)); item->setProperty("peaks", array(frame.peaks));
        item->setProperty("duration", frame.duration); item->setProperty("dryRms", frame.dryRms); item->setProperty("wetRms", frame.wetRms);
        item->setProperty("peak", frame.peak); item->setProperty("pan", frame.pan); item->setProperty("wetPan", frame.wetPan);
        item->setProperty("correlation", frame.correlation); frames.add(item);
    }
    packet.getDynamicObject()->setProperty("frames", frames);
    browser->emitEventIfBrowserIsVisible("sanmuFrame", packet);
}
