// ==============================================================================
// PluginEditor.cpp - SanMu Delay 现代化离线 Web 插件编辑器实现
// ==============================================================================

#include "PluginEditor.h"
#include "presets/PresetManager.h"
#include "SanMuDelayWebAssets.h"
#include <cmath>

namespace
{
juce::var object() { return new juce::DynamicObject(); }
void put(juce::var& value, const juce::Identifier& key, const juce::var& item) { value.getDynamicObject()->setProperty(key, item); }
juce::var failure(const char* message) { auto v = object(); put(v, "error", juce::String::fromUTF8(message)); return v; }
bool number(const juce::var& v) { return (v.isDouble() || v.isInt() || v.isInt64()) && std::isfinite(static_cast<double>(v)); }

const juce::StringArray& editableIds()
{
    static const juce::StringArray ids {
        "engine", "sync_mode", "manual_bpm", "division", "division_r",
        "time_l", "time_r", "time_link", "feedback", "topology",
        "damp_lowcut", "damp_highcut", "mix", "output_gain", "freeze",
        "global_bypass", "duck_enable", "duck_source", "duck_threshold",
        "duck_amount", "duck_release", "mod_rate", "mod_depth",
        "tape_drive", "tape_wow", "bbd_warmth", "diffusion_amount", "theme"
    };
    return ids;
}

std::optional<juce::WebBrowserComponent::Resource> resource(const juce::String& url)
{
    const auto path = url == "/" ? juce::String("index.html") : url.substring(1).upToFirstOccurrenceOf("?", false, false);
    if (path.contains("..")) return {};

    juce::MemoryInputStream input(SanMuDelayWebAssets::sanmudelayui_zip, SanMuDelayWebAssets::sanmudelayui_zipSize, false);
    juce::ZipFile archive(input);
    const auto* entry = archive.getEntry(path);
    if (!entry) return {};

    std::unique_ptr<juce::InputStream> stream(archive.createStreamForEntry(*entry));
    if (!stream) return {};

    juce::MemoryBlock block;
    stream->readIntoMemoryBlock(block);
    const auto* start = static_cast<const std::byte*>(block.getData());
    const auto ext = path.fromLastOccurrenceOf(".", false, false);

    juce::String mime = "application/octet-stream";
    if (ext == "html") mime = "text/html";
    else if (ext == "css") mime = "text/css";
    else if (ext == "js") mime = "text/javascript";
    else if (ext == "json") mime = "application/json";
    else if (ext == "svg") mime = "image/svg+xml";
    else if (ext == "png") mime = "image/png";

    return juce::WebBrowserComponent::Resource {
        std::vector<std::byte>(start, start + block.getSize()),
        mime
    };
}

juce::PropertiesFile::Options prefOptions()
{
    juce::PropertiesFile::Options o;
    o.applicationName = "SanMu Delay";
    o.filenameSuffix = ".settings";
    o.folderName = "SanMu Delay";
    o.osxLibrarySubFolder = "Application Support";
    o.storageFormat = juce::PropertiesFile::storeAsXML;
    return o;
}
} // namespace

SanMuDelayAudioProcessorEditor::SanMuDelayAudioProcessorEditor(SanMuDelayAudioProcessor& p)
    : AudioProcessorEditor(p), audioProcessor(p)
{
    juce::PropertiesFile prefs(prefOptions());
    ui = juce::JSON::parse(p.apvts.state.getProperty("ui_web_config").toString());
    if (!ui.isObject()) ui = object();

    if (!ui.hasProperty("language")) put(ui, "language", prefs.getBoolValue("language_is_english", true) ? "en" : "zh");
    if (!ui.hasProperty("history")) put(ui, "history", p.apvts.state.getProperty("ui_history_visible", true));
    if (!ui.hasProperty("scale")) put(ui, "scale", prefs.getDoubleValue("window_scale", 1.0));
    if (!ui.hasProperty("tips")) put(ui, "tips", prefs.getIntValue("tooltip_mode", 0) == 3 ? "off" : "follow");

    put(ui, "theme", p.apvts.getRawParameterValue(Params::IDs::theme)->load() > 0.5f ? "dark" : "light");
    ui = preferences.load(ui);
    windowScale = number(ui["scale"]) ? juce::jlimit(0.85f, 1.5f, float(ui["scale"])) : 1.0f;

    presetName = p.apvts.state.getProperty("ui_preset_name", "Default Settings").toString();
    presetValues = soundParameters();

    auto options = juce::WebBrowserComponent::Options()
        .withNativeIntegrationEnabled()
        .withInitialisationData("sanmuDelay", "2026.09.13")
        #if JUCE_WINDOWS
        .withBackend(juce::WebBrowserComponent::Options::Backend::webview2)
        #endif
        .withResourceProvider(resource)
        .withNativeFunction("sanmu", [this](const juce::Array<juce::var>& args, auto complete) {
            juce::ScopedValueSetter<bool> active(nativeCallbackActive, true);
            complete(args.size() == 2 ? handleCommand(args[0].toString(), args[1]) : failure("无效请求"));
        });

    browser = std::make_unique<Browser>(options);
    addAndMakeVisible(*browser);

    const float initialScale = windowScale;
    setResizable(true, false);
    setResizeLimits(935, 666, 1650, 1153);
    // 网页按实际内容宽高适配，固定底栏不参与内容比例。
    setSize(juce::roundToInt(1100 * initialScale), juce::roundToInt(750 * initialScale) + 28);
    put(ui, "scale", windowScale);
    audioProcessor.apvts.state.setProperty("ui_web_config", juce::JSON::toString(ui, true), nullptr);

    addAndMakeVisible(betaNotice);
    browser->goToURL(juce::WebBrowserComponent::getResourceProviderRoot());
    startTimerHz(30);
}

SanMuDelayAudioProcessorEditor::~SanMuDelayAudioProcessorEditor()
{
    ready = false;
    cancelPendingUpdate();
    stopTimer();
    flushResizePreferences();
    finishGestures();
    browser.reset();
}

void SanMuDelayAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(ui["theme"].toString() == "dark" ? 0xff101218 : 0xffe9ecef));
}

void SanMuDelayAudioProcessorEditor::resized()
{
    auto content = getLocalBounds();
    // 宿主重挂载或显示比例变化期间可能短暂给出空区域，保持上次有效网页尺寸。
    if (content.getWidth() <= 0 || content.getHeight() <= 28) return;
    betaNotice.setBounds(content.removeFromBottom(28));
    if (browser && browser->getBounds() != content) browser->setBounds(content);
    windowScale = std::min(float(content.getWidth()) / 1100.0f, float(content.getHeight()) / 750.0f);
    if (ready && ui.isObject() && (!number(ui["scale"]) || std::abs(double(ui["scale"]) - windowScale) > .0001))
    {
        put(ui, "scale", windowScale);
        resizeSaveDeadline = juce::Time::getMillisecondCounterHiRes() + 180;
    }
}
void SanMuDelayAudioProcessorEditor::requestWindowScale(double scale)
{
    windowScale = static_cast<float>(scale);
    put(ui, "scale", scale);
    pendingWidth = juce::roundToInt(1100 * scale);
    pendingHeight = juce::roundToInt(750 * scale) + 28;
    if (getWidth() != pendingWidth || getHeight() != pendingHeight)
    {
        if (nativeCallbackActive) triggerAsyncUpdate();
        else handleAsyncUpdate();
    }
}
void SanMuDelayAudioProcessorEditor::handleAsyncUpdate()
{
    if (pendingWidth > 0 && pendingHeight > 28 && (getWidth() != pendingWidth || getHeight() != pendingHeight))
        setSize(pendingWidth, pendingHeight);
}
void SanMuDelayAudioProcessorEditor::flushResizePreferences()
{
    if (resizeSaveDeadline <= 0) return;
    resizeSaveDeadline = 0;
    preferences.save(ui);
    audioProcessor.apvts.state.setProperty("ui_web_config", juce::JSON::toString(ui, true), nullptr);
}

std::vector<float> SanMuDelayAudioProcessorEditor::soundParameters() const
{
    std::vector<float> values;
    for (auto* p : audioProcessor.getParameters())
    {
        if (p != audioProcessor.apvts.getParameter(Params::IDs::theme))
            values.push_back(p->getValue());
    }
    return values;
}

juce::var SanMuDelayAudioProcessorEditor::schema() const
{
    auto result = object();
    for (const auto& id : editableIds())
    {
        auto* p = audioProcessor.apvts.getParameter(id);
        if (!p) continue;
        const auto& range = p->getNormalisableRange();
        auto d = object();
        put(d, "min", range.start);
        put(d, "max", range.end);
        put(d, "step", range.interval);
        put(d, "skew", range.skew);
        put(d, "initial", p->convertFrom0to1(p->getDefaultValue()));
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*>(p))
        {
            juce::Array<juce::var> items;
            for (auto& s : choice->choices)
                items.add(s);
            put(d, "choices", items);
        }
        put(result, id, d);
    }
    return result;
}

juce::var SanMuDelayAudioProcessorEditor::presets() const
{
    juce::Array<juce::var> items;
    for (int i = 0; i < PresetManager::getNumFactoryPresets(); ++i)
    {
        const auto& p = PresetManager::getFactoryPreset(i);
        auto v = object();
        put(v, "id", "factory:" + juce::String(i));
        put(v, "name", p.name);
        put(v, "category", p.category);
        put(v, "user", false);
        items.add(v);
    }
    for (const auto& name : PresetManager::getUserPresetNames())
    {
        auto v = object();
        put(v, "id", "user:" + name);
        put(v, "name", name);
        put(v, "user", true);
        items.add(v);
    }
    return items;
}

juce::var SanMuDelayAudioProcessorEditor::state() const
{
    auto v = object(), values = object();
    for (const auto& id : editableIds())
    {
        if (auto* p = audioProcessor.apvts.getRawParameterValue(id))
            put(values, id, p->load());
    }
    put(v, "values", values);
    auto currentUi = ui.clone();

    put(v, "ui", currentUi);
    put(v, "preset", presetName);
    put(v, "dirty", presetValues != soundParameters());
    put(v, "canUndo", audioProcessor.undoManager.canUndo());
    put(v, "canRedo", audioProcessor.undoManager.canRedo());
    put(v, "bpm", audioProcessor.getDelayProcessor().getLastValidBpm());
    put(v, "ab", audioProcessor.getActiveSnapshot() == SanMuDelayAudioProcessor::ActiveSnapshot::B ? "B" : "A");
    return v;
}

void SanMuDelayAudioProcessorEditor::finishGestures()
{
    for (const auto& id : gestures)
    {
        if (auto* p = audioProcessor.apvts.getParameter(id))
            p->endChangeGesture();
    }
    gestures.clear();
}

bool SanMuDelayAudioProcessorEditor::setParameter(const juce::String& id, float value)
{
    if (!editableIds().contains(id) || !std::isfinite(value)) return false;
    auto* p = audioProcessor.apvts.getParameter(id);
    if (!p) return false;

    const auto& range = p->getNormalisableRange();
    if (value < range.start - 0.0001f || value > range.end + 0.0001f) return false;

    const float before = p->convertFrom0to1(p->getValue());
    value = range.snapToLegalValue(value);
    if (std::abs(before - value) < 0.000001f) return true;

    if (gestures.insert(id).second)
        p->beginChangeGesture();

    p->setValueNotifyingHost(p->convertTo0to1(value));
    return true;
}

void SanMuDelayAudioProcessorEditor::saveUi(const juce::var& data)
{
    if (!data.isObject()) return;
    for (const auto* key : { "language", "tips", "history" })
    {
        if (data.hasProperty(key))
            put(ui, key, data[key]);
    }
    if (data.hasProperty("theme"))
    {
        const auto dark = data["theme"].toString() == "dark";
        if (auto* p = audioProcessor.apvts.getParameter(Params::IDs::theme))
            p->setValueNotifyingHost(dark ? 1.0f : 0.0f);
        put(ui, "theme", dark ? "dark" : "light");
    }
    if (number(data["scale"]))
    {
        requestWindowScale(juce::jlimit(.85, 1.5, double(data["scale"])));
    }
    put(ui, "scale", windowScale);
    audioProcessor.apvts.state.setProperty("ui_web_config", juce::JSON::toString(ui, true), nullptr);
    audioProcessor.apvts.state.setProperty("ui_history_visible", bool(ui["history"]), nullptr);

    preferences.save(ui);
    repaint();
}

juce::var SanMuDelayAudioProcessorEditor::handleCommand(const juce::String& command, const juce::var& data)
{
    if (command == "initialize")
    {
        ready = true;
        auto v = state();
        put(v, "schema", schema());
        put(v, "presets", presets());
        return v;
    }
    if (command == "state") return state();
    if (command == "begin")
    {
        finishGestures();
        audioProcessor.undoManager.beginNewTransaction();
        return true;
    }
    if (command == "end")
    {
        finishGestures();
        return state();
    }
    if (command == "set")
    {
        if (!data.isObject() || !number(data["value"]) || !setParameter(data["id"].toString(), float(data["value"])))
            return failure("参数值超出范围");
        return state();
    }
    if (command == "ui")
    {
        if (!data.isObject() || juce::JSON::toString(data).length() > 4096)
            return failure("界面设置无效");
        saveUi(data);
        return state();
    }
    if (command == "undo" || command == "redo")
    {
        finishGestures();
        if (command == "undo") audioProcessor.undoManager.undo();
        else audioProcessor.undoManager.redo();
        audioProcessor.notifyHostAllParameters();
        return state();
    }
    if (command == "ab")
    {
        finishGestures();
        if (data["slot"].toString() == "A" && audioProcessor.getActiveSnapshot() != SanMuDelayAudioProcessor::ActiveSnapshot::A)
            audioProcessor.clickA();
        if (data["slot"].toString() == "B" && audioProcessor.getActiveSnapshot() != SanMuDelayAudioProcessor::ActiveSnapshot::B)
            audioProcessor.clickB();
        return state();
    }
    if (command == "copyAB")
    {
        finishGestures();
        audioProcessor.copyActiveSnapshotToOther();
        return state();
    }
    if (command == "loadPreset")
    {
        const auto id = data["id"].toString();
        const auto before = audioProcessor.apvts.copyState();
        finishGestures();
        audioProcessor.undoManager.beginNewTransaction();

        if (id.startsWith("factory:"))
        {
            const auto suffix = id.substring(8);
            const int index = suffix.getIntValue();
            if (index < 0 || index >= PresetManager::getNumFactoryPresets())
                return failure("预设不存在");
            const auto& p = PresetManager::getFactoryPreset(index);
            PresetManager::applyPreset(p, audioProcessor.apvts);
            presetName = p.name;
        }
        else if (id.startsWith("user:") && PresetManager::getUserPresetNames().contains(id.substring(5)))
        {
            if (!PresetManager::loadUserPreset(id.substring(5), audioProcessor.apvts))
                return failure("预设读取失败");
            presetName = id.substring(5);
        }
        else
        {
            return failure("预设不存在");
        }

        // 恢复 UI 设置
        for (int i = 0; i < before.getNumProperties(); ++i)
        {
            const auto key = before.getPropertyName(i);
            if (key.toString().startsWith("ui_"))
                audioProcessor.apvts.state.setProperty(key, before.getProperty(key), nullptr);
        }
        audioProcessor.notifyHostAllParameters();
        presetValues = soundParameters();
        audioProcessor.apvts.state.setProperty("ui_preset_name", presetName, nullptr);
        return state();
    }
    if (command == "savePreset")
    {
        const auto name = data["name"].toString().trim();
        if (name.isEmpty() || name.length() > 60 || name.containsAnyOf("/\\:"))
            return failure("请输入不含路径符号的预设名称");
        if (!PresetManager::saveUserPreset(name, audioProcessor.apvts))
            return failure("预设保存失败");
        presetName = name;
        presetValues = soundParameters();
        audioProcessor.apvts.state.setProperty("ui_preset_name", name, nullptr);
        auto v = state();
        put(v, "presets", presets());
        return v;
    }
    return failure("未知请求");
}

void SanMuDelayAudioProcessorEditor::timerCallback()
{
    if (resizeSaveDeadline > 0 && juce::Time::getMillisecondCounterHiRes() >= resizeSaveDeadline) flushResizePreferences();
    if (!ready || !browser) return;
    const double now = juce::Time::getMillisecondCounterHiRes();

    auto& proc = audioProcessor.getDelayProcessor();
    const float outL = proc.getOutputPeakL();
    const float outR = proc.getOutputPeakR();
    const float wetL = proc.getWetPeakL();
    const float wetR = proc.getWetPeakR();
    const float duckGr = proc.getCurrentDuckingGrDb();
    const double bpm = proc.getLastValidBpm();

    const auto currentSoundParameters = soundParameters();
    const bool soundChanged = !hasSentState || currentSoundParameters != lastSentSoundParameters;
    auto packet = soundChanged ? state() : object();
    if (soundChanged)
    {
        lastSentSoundParameters = currentSoundParameters;
        hasSentState = true;
    }
    auto meter = object();
    put(meter, "time", now);
    put(meter, "outputL", outL);
    put(meter, "outputR", outR);
    put(meter, "wetL", wetL);
    put(meter, "wetR", wetR);
    put(meter, "duckGr", duckGr);
    put(meter, "bpm", bpm);
    put(packet, "meter", meter);

    browser->emitEventIfBrowserIsVisible("sanmuFrame", packet);
}
