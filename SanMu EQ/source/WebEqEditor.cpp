#include "WebEqEditor.h"
#include "SanMuEqWebAssets.h"
#include "presets/PresetManager.h"
#include <cmath>

namespace
{
juce::var object() { return new juce::DynamicObject(); }
void put(juce::var& value, const juce::Identifier& key, const juce::var& item) { value.getDynamicObject()->setProperty(key, item); }
bool number(const juce::var& v) { return (v.isDouble() || v.isInt() || v.isInt64()) && std::isfinite(static_cast<double>(v)); }
juce::var failure(const char* text) { auto v = object(); put(v, "error", juce::String::fromUTF8(text)); return v; }
bool editable(const juce::String& id) { return id != Params::IDs::THEME && id != Params::IDs::GAIN_RANGE; }
std::optional<juce::WebBrowserComponent::Resource> resource(const juce::String& url)
{
    const auto path = url == "/" ? juce::String("index.html") : url.substring(1).upToFirstOccurrenceOf("?", false, false);
    if (path.contains("..")) return {};
    juce::MemoryInputStream input(SanMuEqWebAssets::sanmuequi_zip, SanMuEqWebAssets::sanmuequi_zipSize, false);
    juce::ZipFile archive(input); const auto* entry = archive.getEntry(path); if (entry == nullptr) return {};
    std::unique_ptr<juce::InputStream> stream(archive.createStreamForEntry(*entry)); if (!stream) return {};
    juce::MemoryBlock block; stream->readIntoMemoryBlock(block); const auto* start = static_cast<const std::byte*>(block.getData());
    const auto ext = path.fromLastOccurrenceOf(".", false, false);
    return juce::WebBrowserComponent::Resource { std::vector<std::byte>(start, start + block.getSize()),
        ext == "html" ? "text/html" : ext == "css" ? "text/css" : ext == "js" ? "text/javascript" : "application/octet-stream" };
}
}

SanMuEqWebEditor::SanMuEqWebEditor(SanMuQ6AudioProcessor& p) : AudioProcessorEditor(p), audioProcessor(p)
{
    ui = juce::JSON::parse(p.apvts.state.getProperty("ui_web_config").toString()); if (!ui.isObject()) ui = object();
    if (!ui.hasProperty("language")) put(ui, "language", "zh");
    if (!ui.hasProperty("scale")) put(ui, "scale", 1.0);
    if (!ui.hasProperty("tips") || ui["tips"].toString() == "on") put(ui, "tips", "follow");
    if (!ui.hasProperty("spectrum")) put(ui, "spectrum", true);
    if (!ui.hasProperty("range")) put(ui, "range", 12);
    if (!ui.hasProperty("outputLink")) put(ui, "outputLink", true);
    for (const auto* key : {"spectrumPost", "spectrumGrab"}) if (!ui[key].isBool()) put(ui, key, true);
    if (!ui["spectrumPre"].isBool()) put(ui, "spectrumPre", false);
    const auto option = [&](const char* key, std::initializer_list<double> choices, double fallback)
    { const auto v = ui[key]; if (!number(v) || std::find(choices.begin(), choices.end(), double(v)) == choices.end()) put(ui, key, fallback); };
    option("spectrumRange", {60, 90, 120}, 90); option("spectrumResolution", {0, 1, 2, 3}, 2);
    option("spectrumSpeed", {0, 1, 2}, 1); option("spectrumTilt", {0, 1.5, 3, 4.5, 6}, 0);
    audioProcessor.getEqProcessor().setSpectrumResolution(int(ui["spectrumResolution"]));
    put(ui, "theme", p.apvts.getRawParameterValue(Params::IDs::THEME)->load() > .5f ? "dark" : "light");
    ui = preferences.load(ui);
    presetName = p.apvts.state.getProperty("ui_preset_name", "Default Settings").toString(); presetValues = soundValues();
    auto options = juce::WebBrowserComponent::Options().withNativeIntegrationEnabled().withInitialisationData("sanmuEq", "2026.09.14")
    #if JUCE_WINDOWS
        .withBackend(juce::WebBrowserComponent::Options::Backend::webview2)
    #endif
        .withResourceProvider(resource).withNativeFunction("sanmu", [this](const juce::Array<juce::var>& args, auto complete)
        { juce::ScopedValueSetter<bool> active(nativeCallbackActive, true); complete(args.size() == 2 ? handleCommand(args[0].toString(), args[1]) : failure("无效请求")); });
    browser = std::make_unique<Browser>(options); addAndMakeVisible(*browser);
    const float scale = number(ui["scale"]) ? juce::jlimit(.85f, 1.5f, float(ui["scale"])) : 1.f;
    setResizable(true, false); setResizeLimits(935, 666, 1650, 1153); // 网页按实际内容宽高适配，固定底栏不参与内容比例。
    setSize(juce::roundToInt(1100 * scale), juce::roundToInt(750 * scale) + 28);
    put(ui, "scale", std::min(double(getWidth()) / 1100.0, double(getHeight() - 28) / 750.0));
    saveUi();
    addAndMakeVisible(betaNotice);
    browser->goToURL(juce::WebBrowserComponent::getResourceProviderRoot()); startTimerHz(30);
}
SanMuEqWebEditor::~SanMuEqWebEditor() { ready = false; cancelPendingUpdate(); stopTimer(); flushResizePreferences(); audioProcessor.getEqProcessor().setAuditionBand(-1); finishGestures(); browser.reset(); }
void SanMuEqWebEditor::paint(juce::Graphics& g) { g.fillAll(juce::Colour(ui["theme"].toString() == "dark" ? 0xff101218 : 0xffe9ecef)); }
void SanMuEqWebEditor::resized()
{
    auto content = getLocalBounds();
    // 宿主重挂载或显示比例变化期间可能短暂给出空区域，保持上次有效网页尺寸。
    if (content.getWidth() <= 0 || content.getHeight() <= 28) return;
    betaNotice.setBounds(content.removeFromBottom(28));
    if (browser && browser->getBounds() != content) browser->setBounds(content);
    if (ready && ui.isObject())
    {
        const double scale = std::min(double(content.getWidth()) / 1100.0, double(content.getHeight()) / 750.0);
        if (!number(ui["scale"]) || std::abs(double(ui["scale"]) - scale) > .0001)
        {
            put(ui, "scale", scale);
            resizeSaveDeadline = juce::Time::getMillisecondCounterHiRes() + 180;
        }
    }
}
void SanMuEqWebEditor::requestWindowScale(double scale)
{
    put(ui, "scale", scale);
    pendingWidth = juce::roundToInt(1100 * scale);
    pendingHeight = juce::roundToInt(750 * scale) + 28;
    if (getWidth() != pendingWidth || getHeight() != pendingHeight)
    {
        if (nativeCallbackActive) triggerAsyncUpdate();
        else handleAsyncUpdate();
    }
}
void SanMuEqWebEditor::handleAsyncUpdate()
{
    if (pendingWidth > 0 && pendingHeight > 28 && (getWidth() != pendingWidth || getHeight() != pendingHeight))
        setSize(pendingWidth, pendingHeight);
}
void SanMuEqWebEditor::flushResizePreferences()
{
    if (resizeSaveDeadline <= 0) return;
    resizeSaveDeadline = 0;
    preferences.save(ui);
    saveUi();
}
void SanMuEqWebEditor::saveUi() { audioProcessor.apvts.state.setProperty("ui_web_config", juce::JSON::toString(ui, true), nullptr); }
std::vector<float> SanMuEqWebEditor::soundValues() const
{
    std::vector<float> result; for (auto* p : audioProcessor.getParameters())
        if (auto* param = dynamic_cast<juce::RangedAudioParameter*>(p); param != nullptr && editable(param->paramID)) result.push_back(p->getValue());
    result.push_back(static_cast<float>(audioProcessor.getLinearLegacyMask()));
    result.push_back(static_cast<float>(audioProcessor.getLinearQuality()));
    return result;
}
juce::var SanMuEqWebEditor::schema() const
{
    auto result = object(); for (auto* parameter : audioProcessor.getParameters())
    {
        auto* p = dynamic_cast<juce::RangedAudioParameter*>(parameter); if (p == nullptr || !editable(p->paramID)) continue;
        const auto& range = p->getNormalisableRange(); auto d = object();
        put(d, "min", range.start); put(d, "max", range.end); put(d, "step", range.interval); put(d, "skew", range.skew);
        put(d, "initial", p->convertFrom0to1(p->getDefaultValue()));
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*>(p)) { juce::Array<juce::var> choices; for (const auto& s : choice->choices) choices.add(s); put(d, "choices", choices); }
        put(result, p->paramID, d);
    } return result;
}
juce::var SanMuEqWebEditor::presets() const
{
    juce::Array<juce::var> result;
    for (int i = 0; i < PresetManager::getNumFactoryPresets(); ++i) { auto p = object(); put(p, "id", "factory:" + juce::String(i)); put(p, "name", PresetManager::getFactoryPreset(i).name); result.add(p); }
    for (const auto& name : PresetManager::getUserPresetNames()) { auto p = object(); put(p, "id", "user:" + name); put(p, "name", name); result.add(p); }
    return result;
}
juce::var SanMuEqWebEditor::state() const
{
    auto result = object(), values = object();
    for (auto* parameter : audioProcessor.getParameters()) if (auto* p = dynamic_cast<juce::RangedAudioParameter*>(parameter); p != nullptr && editable(p->paramID)) put(values, p->paramID, p->convertFrom0to1(p->getValue()));
    auto config = ui.clone();
    put(result, "values", values); put(result, "ui", config); put(result, "preset", presetName); put(result, "dirty", presetValues != soundValues());
    put(result, "linearLegacyMask", audioProcessor.getLinearLegacyMask());
    put(result, "linearQuality", audioProcessor.getLinearQuality());
    put(result, "canUndo", audioProcessor.undoManager.canUndo()); put(result, "canRedo", audioProcessor.undoManager.canRedo());
    put(result, "ab", audioProcessor.getActiveSnapshot() == SanMuQ6AudioProcessor::ActiveSnapshot::B ? "B" : "A"); return result;
}
bool SanMuEqWebEditor::setParameter(const juce::String& id, const juce::var& value)
{
    auto* p = audioProcessor.apvts.getParameter(id); if (p == nullptr || !editable(id) || !number(value)) return false;
    const auto& range = p->getNormalisableRange(); const float v = float(value); if (v < range.start || v > range.end) return false;
    if (v >= .5f && audioProcessor.apvts.getRawParameterValue(Params::IDs::EQ_MODE)->load() >= .5f)
        for (int i = 0; i < Params::MAX_BANDS; ++i)
            if (id == Params::IDs::bandDynamic(i + 1)) audioProcessor.enableLinearDynamicBand(i);
    if (gestures.insert(id).second) p->beginChangeGesture();
    p->setValueNotifyingHost(p->convertTo0to1(range.snapToLegalValue(v))); return true;
}
void SanMuEqWebEditor::finishGestures()
{
    // APVTS 定时同步参数；事务结束时立即同步，避免快速增删被合并或漏记。
    audioProcessor.apvts.copyState();
    for (const auto& id : gestures) if (auto* p = audioProcessor.apvts.getParameter(id)) p->endChangeGesture();
    gestures.clear();
}
juce::var SanMuEqWebEditor::frame(bool includeCurves)
{
    auto result = object(), curves = object(), dynamicCurves = object(), rangeCurves = object(); auto& eq = audioProcessor.getEqProcessor(); juce::var reductions = object(), detectors = object(), thresholds = object(), thresholdReady = object();
    const auto read = [this](const juce::String& id) { return audioProcessor.apvts.getRawParameterValue(id)->load(); };
    const double rate = audioProcessor.getSampleRate() > 0 ? audioProcessor.getSampleRate() : 48000.0;
    for (int i = 0; i < Params::MAX_BANDS; ++i)
    {
        const int n = i + 1; if (read(Params::IDs::bandPresent(n)) < .5f) continue;
        if (includeCurves) {
        auto& b = displayBands[static_cast<size_t>(i)];
        b.update(int(read(Params::IDs::bandType(n))), read(Params::IDs::bandEnabled(n)) > .5f, read(Params::IDs::bandFreq(n)), read(Params::IDs::bandGain(n)), read(Params::IDs::bandQ(n)), read(Params::IDs::bandSolo(n)) > .5f, int(read(Params::IDs::bandSlope(n))), int(read(Params::IDs::bandPlacement(n))), true);
        juce::Array<juce::var> curve; curve.ensureStorageAllocated(320);
        for (int point = 0; point < 320; ++point) curve.add(b.getMagnitudeAtFrequency(20.0 * std::pow(1000.0, point / 319.0), rate * EqProcessor::OVERSAMPLING_FACTOR));
        put(curves, juce::String(n), curve);
        }
        const bool dynamic = read(Params::IDs::bandDynamic(n)) > .5f
            && (read(Params::IDs::EQ_MODE) < .5f || audioProcessor.isLinearDynamicAllowed(i))
            && read(Params::IDs::bandEnabled(n)) > .5f && read(Params::IDs::bandType(n)) < 3 && !eq.isAnyBandSoloed() && !eq.isGlobalBypassed();
        const float reduction = dynamic ? eq.getAppliedDynamicReduction(i) : 0;
        put(reductions, juce::String(n), reduction);
        const auto& detector = eq.getBand(i).dynamicProcessor;
        put(detectors, juce::String(n), dynamic ? detector.getDetectorDb() : -120.f);
        put(thresholds, juce::String(n), dynamic ? detector.getThresholdDb() : read(Params::IDs::bandDynThreshold(n)));
        put(thresholdReady, juce::String(n), dynamic && detector.isThresholdReady());
        if (dynamic && includeCurves)
        {
            const int type = int(read(Params::IDs::bandType(n)));
            const auto filterType = type == 1 ? TptSvf::Type::LowShelf : type == 2 ? TptSvf::Type::HighShelf : TptSvf::Type::Bell;
            const float q = read(Params::IDs::bandQ(n)) * (type == 0 ? 1.f : .70710678f);
            juce::Array<juce::var> live, limit;
            for (int point = 0; point < 320; ++point)
            {
                const double f = 20 * std::pow(1000.0, point / 319.0);
                live.add(TptSvf::magnitudeDb(filterType, read(Params::IDs::bandFreq(n)), q, -reduction, f));
                limit.add(TptSvf::magnitudeDb(filterType, read(Params::IDs::bandFreq(n)), q, (read(Params::IDs::bandDynUpward(n)) > .5f ? 1 : -1) * read(Params::IDs::bandDynRange(n)), f));
            }
            put(dynamicCurves, juce::String(n), live); put(rangeCurves, juce::String(n), limit);
        }
    }
    put(result, "curves", curves); put(result, "dynamicCurves", dynamicCurves); put(result, "rangeCurves", rangeCurves); put(result, "dynamicReduction", reductions); juce::Array<juce::var> spectrum, inputSpectrum;
    put(result, "dynamicDetector", detectors); put(result, "dynamicThreshold", thresholds); put(result, "dynamicReady", thresholdReady);
    juce::Array<juce::var> spectrumPeak;
    if (bool(ui["spectrum"]) && eq.isSpectrumReady())
        for (int i = 0; i < EqProcessor::SPECTRUM_POINTS; ++i) { spectrum.add(eq.getSpectrumPoint(i)); inputSpectrum.add(eq.getInputSpectrumPoint(i)); spectrumPeak.add(eq.getSpectrumPeakPoint(i)); }
    put(result, "spectrum", spectrum); put(result, "inputSpectrum", inputSpectrum); put(result, "spectrumPeak", spectrumPeak);
    const double now = juce::Time::getMillisecondCounterHiRes();
    const auto sequence = eq.getMeterSequence();
    if (sequence != lastMeterSequence) { lastMeterSequence = sequence; lastMeterUpdate = now; }
    // 宿主停播或绕过音频回调时，旧电平按墙钟时间释放。
    const float decay = static_cast<float>(std::max(0.0, now - lastMeterUpdate - 100) * .04);
    put(result, "meters", juce::Array<juce::var> {std::max(-100.f, eq.getLeftPeakDb() - decay), std::max(-100.f, eq.getRightPeakDb() - decay)});
    put(result, "trackName", audioProcessor.getTrackName());
    juce::Array<juce::var> precisionLatencies; for (int quality = 0; quality < 3; ++quality) precisionLatencies.add(PrecisionLinearEq::latencyFor(rate, quality));
    put(result, "linearLatencies", precisionLatencies);
    put(result, "sampleRate", rate); put(result, "latency", audioProcessor.getLatencySamples()); return result;
}
void SanMuEqWebEditor::timerCallback()
{
    if (resizeSaveDeadline > 0 && juce::Time::getMillisecondCounterHiRes() >= resizeSaveDeadline) flushResizePreferences();
    // 浏览器崩溃或漏发松手时，心跳到期自动解除监听。
    if (auditionDeadline > 0 && juce::Time::getMillisecondCounterHiRes() > auditionDeadline)
    { audioProcessor.getEqProcessor().setAuditionBand(-1); auditionDeadline = 0; }
    if (!ready || !browser->isVisible()) return;
    auto data = frame(false); if (++frameCount % 3 == 0) put(data, "state", state());
    browser->emitEventIfBrowserIsVisible("sanmuFrame", data);
}
juce::var SanMuEqWebEditor::handleCommand(const juce::String& command, const juce::var& data)
{
    if (command == "initialize") { ready = true; auto result = state(); put(result, "schema", schema()); put(result, "presets", presets()); put(result, "frame", frame()); return result; }
    if (command == "state") return state();
    if (command == "linearQuality")
    {
        if (!number(data["quality"]) || double(data["quality"]) != std::floor(double(data["quality"])) || int(data["quality"]) < 0 || int(data["quality"]) > 2) return failure("线性相位精度档位无效");
        finishGestures(); audioProcessor.undoManager.beginNewTransaction(); audioProcessor.setLinearQuality(int(data["quality"]));
        return state();
    }
    if (command == "audition")
    {
        if (!data["active"].isBool()) return failure("监听状态无效");
        int index = -1;
        if (bool(data["active"]))
        {
            if (!number(data["band"]) || double(data["band"]) != std::floor(double(data["band"]))) return failure("监听频段无效");
            index = int(data["band"]) - 1;
            if (index < 0 || index >= Params::MAX_BANDS || audioProcessor.apvts.getRawParameterValue(Params::IDs::bandPresent(index + 1))->load() < .5f) return failure("监听频段不存在");
        }
        audioProcessor.getEqProcessor().setAuditionBand(index);
        auditionDeadline = index >= 0 ? juce::Time::getMillisecondCounterHiRes() + 1000 : 0;
        return true;
    }
    if (command == "begin") { finishGestures(); audioProcessor.undoManager.beginNewTransaction(); return true; }
    if (command == "end") { finishGestures(); auto result = state(); put(result, "frame", frame()); return result; }
    if (command == "setMany")
    {
        const auto values = data["values"]; if (!values.isObject()) return failure("参数批次无效");
        for (const auto& item : values.getDynamicObject()->getProperties()) {
            const auto id = item.name.toString(); const auto* p = audioProcessor.apvts.getParameter(id);
            if (!p || !editable(id) || !number(item.value) || double(item.value) < p->getNormalisableRange().start || double(item.value) > p->getNormalisableRange().end) return failure("参数批次超出范围");
        }
        for (const auto& item : values.getDynamicObject()->getProperties()) setParameter(item.name.toString(), item.value);
        return true;
    }
    if (command == "set") return data.isObject() && setParameter(data["id"].toString(), data["value"]) ? juce::var(true) : failure("参数无效或超出范围");
    if (command == "ui")
    {
        if (!data.isObject()) return failure("界面设置无效");
        if (data.hasProperty("theme")) { const auto theme = data["theme"].toString(); if (theme != "light" && theme != "dark") return failure("主题无效"); audioProcessor.apvts.getParameter(Params::IDs::THEME)->setValueNotifyingHost(theme == "dark" ? 1.f : 0.f); put(ui, "theme", theme); }
        for (const auto* key : {"language", "tips"}) if (data.hasProperty(key)) { const auto value = data[key].toString(); if ((juce::String(key) == "language" && value != "en" && value != "zh") || (juce::String(key) == "tips" && value != "follow" && value != "zh" && value != "en" && value != "off")) return failure("界面选项无效"); put(ui, key, value); }
        for (const auto* key : {"spectrum", "outputLink", "spectrumPre", "spectrumPost", "spectrumGrab"}) if (data[key].isBool()) put(ui, key, data[key]);
        const auto setOption = [&](const char* key, std::initializer_list<double> choices)
        { if (!data.hasProperty(key)) return true; if (!number(data[key]) || std::find(choices.begin(), choices.end(), double(data[key])) == choices.end()) return false; put(ui, key, data[key]); return true; };
        if (!setOption("spectrumRange", {60, 90, 120}) || !setOption("spectrumResolution", {0, 1, 2, 3})
            || !setOption("spectrumSpeed", {0, 1, 2}) || !setOption("spectrumTilt", {0, 1.5, 3, 4.5, 6})) return failure("频谱设置无效");
        audioProcessor.getEqProcessor().setSpectrumResolution(int(ui["spectrumResolution"]));
        if (data.hasProperty("range")) { const auto value = data["range"]; if (!(value.toString() == "auto" || (number(value) && (int(value) == 12 || int(value) == 24 || int(value) == 30)))) return failure("量程无效"); put(ui, "range", value); }
        if (data.hasProperty("scale")) { if (!number(data["scale"]) || double(data["scale"]) < .85 || double(data["scale"]) > 1.5) return failure("缩放无效"); requestWindowScale(double(data["scale"])); }
        if (!preferences.save(ui)) return failure("界面偏好保存失败，请检查用户目录权限");
        saveUi(); repaint(); return state();
    }
    if (command == "undo" || command == "redo") { finishGestures(); if (command == "undo") audioProcessor.undoManager.undo(); else audioProcessor.undoManager.redo(); return state(); }
    if (command == "ab")
    {
        const auto slot = data["slot"].toString(); if (slot != "A" && slot != "B") return failure("快照无效");
        finishGestures(); const float theme = audioProcessor.apvts.getParameter(Params::IDs::THEME)->getValue();
        if (audioProcessor.getActiveSnapshot() == SanMuQ6AudioProcessor::ActiveSnapshot::None)
            audioProcessor.switchToSnapshot(SanMuQ6AudioProcessor::ActiveSnapshot::A);
        audioProcessor.switchToSnapshot(slot == "A" ? SanMuQ6AudioProcessor::ActiveSnapshot::A : SanMuQ6AudioProcessor::ActiveSnapshot::B);
        audioProcessor.apvts.getParameter(Params::IDs::THEME)->setValueNotifyingHost(theme); saveUi();
        auto result = state(); put(result, "frame", frame()); return result;
    }
    if (command == "copyAB") { finishGestures(); audioProcessor.copyActiveToOtherSnapshot(); return state(); }
    if (command == "addBand" || command == "deleteBand")
    {
        int n = -1; if (command == "addBand") { for (int i = 1; i <= Params::MAX_BANDS; ++i) if (audioProcessor.apvts.getRawParameterValue(Params::IDs::bandPresent(i))->load() < .5f) { n = i; break; } }
        else if (number(data["band"]) && double(data["band"]) == std::floor(double(data["band"]))) n = int(data["band"]);
        if (n < 1 || n > Params::MAX_BANDS) return failure("频段不存在或已达到上限");
        const float freq = data.hasProperty("freq") && number(data["freq"]) ? float(data["freq"]) : 1000.f;
        const float gain = data.hasProperty("gain") && number(data["gain"]) ? float(data["gain"]) : 0.f;
        if (freq < 20 || freq > 20000 || gain < Params::Ranges::GAIN_MIN || gain > Params::Ranges::GAIN_MAX) return failure("频段参数无效");
        finishGestures(); audioProcessor.undoManager.beginNewTransaction();
        if (command == "addBand") { setParameter(Params::IDs::bandDynamic(n), 0); setParameter(Params::IDs::bandDynAuto(n), 1); setParameter(Params::IDs::bandDynUpward(n), 0); setParameter(Params::IDs::bandDynRange(n), 6); setParameter(Params::IDs::bandDynThreshold(n), -24); }
        if (command == "addBand") { setParameter(Params::IDs::bandType(n), 0); setParameter(Params::IDs::bandFreq(n), freq); setParameter(Params::IDs::bandGain(n), gain); setParameter(Params::IDs::bandQ(n), 1.f); setParameter(Params::IDs::bandSlope(n), 0); setParameter(Params::IDs::bandPlacement(n), 0); }
        setParameter(Params::IDs::bandSolo(n), 0); setParameter(Params::IDs::bandEnabled(n), command == "addBand" ? 1 : 0); setParameter(Params::IDs::bandPresent(n), command == "addBand" ? 1 : 0); finishGestures();
        auto result = state(); put(result, "selected", command == "addBand" ? n : -1); put(result, "frame", frame()); return result;
    }
    if (command == "loadPreset")
    {
        finishGestures(); const auto id = data["id"].toString(); const float theme = audioProcessor.apvts.getParameter(Params::IDs::THEME)->getValue();
        audioProcessor.undoManager.beginNewTransaction();
        if (id.startsWith("factory:")) { const auto suffix = id.substring(8); if (suffix.isEmpty() || !suffix.containsOnly("0123456789")) return failure("预设不存在"); const int index = suffix.getIntValue(); if (index < 0 || index >= PresetManager::getNumFactoryPresets()) return failure("预设不存在"); const auto& p = PresetManager::getFactoryPreset(index); PresetManager::applyPreset(p, audioProcessor.apvts); presetName = p.name; }
        else if (id.startsWith("user:") && PresetManager::getUserPresetNames().contains(id.substring(5))) { if (!PresetManager::loadUserPreset(id.substring(5), audioProcessor.apvts)) return failure("预设读取失败"); presetName = id.substring(5); }
        else return failure("预设不存在");
        audioProcessor.apvts.getParameter(Params::IDs::THEME)->setValueNotifyingHost(theme); saveUi(); audioProcessor.apvts.state.setProperty("ui_preset_name", presetName, nullptr); presetValues = soundValues(); auto result = state(); put(result, "frame", frame()); return result;
    }
    if (command == "savePreset")
    {
        const auto name = data["name"].toString().trim(); if (name.isEmpty() || name.length() > 60 || name.containsAnyOf("/\\:")) return failure("预设名称无效");
        if (PresetManager::getUserPresetNames().contains(name)) return failure("同名预设已存在");
        if (!PresetManager::saveUserPreset(name, audioProcessor.apvts)) return failure("保存失败");
        presetName = name; presetValues = soundValues(); audioProcessor.apvts.state.setProperty("ui_preset_name", name, nullptr); auto result = state(); put(result, "presets", presets()); return result;
    }
    return failure("未知请求");
}
