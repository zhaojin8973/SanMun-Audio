#include "PluginEditor.h"
#include "presets/PresetManager.h"
#include "SanMuCompWebAssets.h"
#include <cmath>

namespace
{
juce::var object() { return new juce::DynamicObject(); }
void put(juce::var& value, const juce::Identifier& key, const juce::var& item) { value.getDynamicObject()->setProperty(key,item); }
juce::var failure(const char* message) { auto v=object();put(v,"error",juce::String::fromUTF8(message));return v; }
bool number(const juce::var& v) { return (v.isDouble()||v.isInt()||v.isInt64()) && std::isfinite(static_cast<double>(v)); }
const juce::StringArray& editableIds()
{
    static const juce::StringArray ids {"mode","global_bypass","vox_comp","vox_gain","vox_gate","vox_mix","vox_smooth",
        "vca_threshold","vca_ratio","vca_attack","vca_release","vca_makeup","vca_sidechain_hp","vca_mix","vca_output_trim","vca_stereo_link",
        "fet_input","fet_output","fet_attack","fet_release","fet_ratio","fet_mix","fet_sidechain_hp","headroom","in_out_link","fet_output_trim","fet_stereo_link",
        "opto_gain","opto_peak_reduction","opto_is_limit","opto_mix","opto_sidechain_hp","opto_output_trim","opto_stereo_link"};
    return ids;
}
std::optional<juce::WebBrowserComponent::Resource> resource(const juce::String& url)
{
    const auto path=url=="/"?juce::String("index.html"):url.substring(1).upToFirstOccurrenceOf("?",false,false);
    if(path.contains(".."))return {};
    juce::MemoryInputStream input(SanMuCompWebAssets::sanmucompui_zip,SanMuCompWebAssets::sanmucompui_zipSize,false);
    juce::ZipFile archive(input);const auto* entry=archive.getEntry(path);if(!entry)return {};
    std::unique_ptr<juce::InputStream> stream(archive.createStreamForEntry(*entry));if(!stream)return {};
    juce::MemoryBlock block;stream->readIntoMemoryBlock(block);const auto* start=static_cast<const std::byte*>(block.getData());
    const auto ext=path.fromLastOccurrenceOf(".",false,false);
    return juce::WebBrowserComponent::Resource{std::vector<std::byte>(start,start+block.getSize()),
        ext=="html"?"text/html":ext=="css"?"text/css":ext=="js"?"text/javascript":ext=="json"?"application/json":"application/octet-stream"};
}
juce::PropertiesFile::Options prefOptions()
{
    juce::PropertiesFile::Options o;o.applicationName="SanMu Comp";o.filenameSuffix=".settings";
    o.folderName="SanMu Comp";o.osxLibrarySubFolder="Application Support";o.storageFormat=juce::PropertiesFile::storeAsXML;return o;
}
}

SanMuCompAudioProcessorEditor::SanMuCompAudioProcessorEditor(SanMuCompAudioProcessor& p):AudioProcessorEditor(p),audioProcessor(p)
{
    juce::PropertiesFile prefs(prefOptions());
    ui=juce::JSON::parse(p.apvts.state.getProperty("ui_web_config").toString());if(!ui.isObject())ui=object();
    if(!ui.hasProperty("language"))put(ui,"language",prefs.getBoolValue("language_is_english",true)?"en":"zh");
    if(!ui.hasProperty("history"))put(ui,"history",p.apvts.state.getProperty("ui_history_visible",true));
    if(!ui.hasProperty("scale"))put(ui,"scale",prefs.getDoubleValue("window_scale",1));
    if(!ui.hasProperty("tips"))put(ui,"tips",prefs.getIntValue("tooltip_mode",0)==3?"off":"follow");
    put(ui,"theme",p.apvts.getRawParameterValue(Params::IDs::THEME)->load()>.5f?"dark":"light");
    ui = preferences.load(ui);
    windowScale=number(ui["scale"])?juce::jlimit(.85f,1.5f,float(ui["scale"])):1.f;
    presetName=p.apvts.state.getProperty("ui_preset_name","Default Settings").toString();presetValues=soundParameters();
    auto options=juce::WebBrowserComponent::Options().withNativeIntegrationEnabled().withInitialisationData("sanmuComp","2026.09.12")
        #if JUCE_WINDOWS
        .withBackend(juce::WebBrowserComponent::Options::Backend::webview2)
        #endif
        .withResourceProvider(resource).withNativeFunction("sanmu",[this](const juce::Array<juce::var>& args,auto complete){
            juce::ScopedValueSetter<bool> active(nativeCallbackActive,true);
            complete(args.size()==2?handleCommand(args[0].toString(),args[1]):failure("无效请求"));});
    browser=std::make_unique<Browser>(options);addAndMakeVisible(*browser);
    const float initialScale = windowScale;
    setResizable(true,false);setResizeLimits(935,666,1650,1153);
    // 网页按实际内容宽高适配，固定底栏不参与内容比例。
    setSize(juce::roundToInt(1100*initialScale),juce::roundToInt(750*initialScale) + 28);
    put(ui,"scale",windowScale);
    audioProcessor.apvts.state.setProperty("ui_web_config",juce::JSON::toString(ui,true),nullptr);
    addAndMakeVisible(betaNotice);
    browser->goToURL(juce::WebBrowserComponent::getResourceProviderRoot());startTimerHz(30);
}
SanMuCompAudioProcessorEditor::~SanMuCompAudioProcessorEditor()
{
    ready=false;cancelPendingUpdate();stopTimer();flushResizePreferences();finishGestures();auto& e=audioProcessor.getFeatureExtractor();
    e.onAnalysisCompleted=nullptr;e.onCaptureCompleted=nullptr;e.cancelListening();
    if(listenCancelled)listenCancelled->store(true);if(listenFuture.valid())listenFuture.wait();browser.reset();
}
void SanMuCompAudioProcessorEditor::paint(juce::Graphics& g)
{ g.fillAll(juce::Colour(ui["theme"].toString()=="dark"?0xff101218:0xffe9ecef)); }
void SanMuCompAudioProcessorEditor::resized()
{
    auto content = getLocalBounds();
    // 宿主重挂载或显示比例变化期间可能短暂给出空区域，保持上次有效网页尺寸。
    if (content.getWidth() <= 0 || content.getHeight() <= 28) return;
    betaNotice.setBounds(content.removeFromBottom(28));
    if (browser && browser->getBounds() != content) browser->setBounds(content);
    windowScale=std::min(float(content.getWidth())/1100.f,float(content.getHeight())/750.f);
    if(ready&&ui.isObject()&&(!number(ui["scale"])||std::abs(double(ui["scale"])-windowScale)>.0001))
    {
        put(ui,"scale",windowScale);
        resizeSaveDeadline=juce::Time::getMillisecondCounterHiRes()+180;
    }
}
void SanMuCompAudioProcessorEditor::requestWindowScale(double scale)
{
    windowScale=static_cast<float>(scale);put(ui,"scale",scale);
    pendingWidth=juce::roundToInt(1100*scale);pendingHeight=juce::roundToInt(750*scale)+28;
    if(getWidth()!=pendingWidth||getHeight()!=pendingHeight)
    {if(nativeCallbackActive)triggerAsyncUpdate();else handleAsyncUpdate();}
}
void SanMuCompAudioProcessorEditor::handleAsyncUpdate()
{
    if(pendingWidth>0&&pendingHeight>28&&(getWidth()!=pendingWidth||getHeight()!=pendingHeight))setSize(pendingWidth,pendingHeight);
}
void SanMuCompAudioProcessorEditor::flushResizePreferences()
{
    if(resizeSaveDeadline<=0)return;resizeSaveDeadline=0;
    preferences.save(ui);audioProcessor.apvts.state.setProperty("ui_web_config",juce::JSON::toString(ui,true),nullptr);
}
std::vector<float> SanMuCompAudioProcessorEditor::soundParameters() const
{
    std::vector<float> values;for(auto* p:audioProcessor.getParameters())
        if(p!=audioProcessor.apvts.getParameter(Params::IDs::THEME))values.push_back(p->getValue());return values;
}
juce::var SanMuCompAudioProcessorEditor::schema() const
{
    auto result=object();
    for(const auto& id:editableIds())
    {
        auto* p=audioProcessor.apvts.getParameter(id);if(!p)continue;const auto& range=p->getNormalisableRange();
        auto d=object();put(d,"min",range.start);put(d,"max",range.end);put(d,"step",range.interval);put(d,"skew",range.skew);
        put(d,"initial",p->convertFrom0to1(p->getDefaultValue()));
        if(auto* choice=dynamic_cast<juce::AudioParameterChoice*>(p)){juce::Array<juce::var> items;for(auto& s:choice->choices)items.add(s);put(d,"choices",items);}
        put(result,id,d);
    }return result;
}
juce::var SanMuCompAudioProcessorEditor::presets() const
{
    juce::Array<juce::var> items;
    for(int i=0;i<PresetManager::getNumFactoryPresets();++i)
    {const auto& p=PresetManager::getFactoryPreset(i);auto v=object();put(v,"id","factory:"+juce::String(i));put(v,"name",p.name);put(v,"mode",p.mode);put(v,"user",false);items.add(v);}
    for(const auto& name:PresetManager::getUserPresetNames())
    {auto v=object();put(v,"id","user:"+name);put(v,"name",name);put(v,"mode",-1);put(v,"user",true);items.add(v);}return items;
}
juce::var SanMuCompAudioProcessorEditor::listenState() const
{
    auto v=object();const auto& e=audioProcessor.getFeatureExtractor();
    put(v,"status",listenStatus=="capturing"&&e.isWaitingForSignal()?juce::String("waiting"):listenStatus);put(v,"progress",e.getProgress());put(v,"seconds",e.getRecordedSeconds());
    put(v,"target",audioProcessor.apvts.state.getProperty("listen_target_gr",4.0));put(v,"hasCapture",bool(listenCapture));
    put(v,"ready",listenResult&&listenResult->ready);put(v,"applied",listenApplied);put(v,"canRestore",listenUndoState.isValid());
    put(v,"message",listenResult?listenResult->message:listenError);
    if(listenResult){put(v,"gr",listenResult->preset.measuredGrDb);put(v,"target",listenResult->preset.requestedGrDb);
        put(v,"level",listenResult->preset.calibratedRmsDb-listenResult->preset.calibrationInputRmsDb);put(v,"mode",listenResult->preset.mode);}return v;
}
juce::var SanMuCompAudioProcessorEditor::state() const
{
    auto v=object(),values=object();for(const auto& id:editableIds())put(values,id,audioProcessor.apvts.getRawParameterValue(id)->load());
    put(v,"values",values);auto currentUi=ui.clone();
    put(v,"ui",currentUi);put(v,"listen",listenState());put(v,"preset",presetName);put(v,"dirty",presetValues!=soundParameters());
    put(v,"canUndo",audioProcessor.undoManager.canUndo());put(v,"canRedo",audioProcessor.undoManager.canRedo());
    put(v,"ab",audioProcessor.getActiveSnapshot()==SanMuCompAudioProcessor::ActiveSnapshot::B?"B":"A");return v;
}
void SanMuCompAudioProcessorEditor::finishGestures()
{for(const auto& id:gestures)audioProcessor.apvts.getParameter(id)->endChangeGesture();gestures.clear();}
bool SanMuCompAudioProcessorEditor::setParameter(const juce::String& id,float value)
{
    if(!editableIds().contains(id)||!std::isfinite(value))return false;
    auto* p=audioProcessor.apvts.getParameter(id);const auto& range=p->getNormalisableRange();
    if(value<range.start-.0001f||value>range.end+.0001f)return false;
    const float before=p->convertFrom0to1(p->getValue());value=range.snapToLegalValue(value);
    if(std::abs(before-value)<.000001f)return true;
    if(gestures.insert(id).second)p->beginChangeGesture();p->setValueNotifyingHost(p->convertTo0to1(value));
    // 联动只响应用户编辑，宿主自动化不会被界面反向改写。
    if((id=="fet_input"||id=="fet_output")&&audioProcessor.apvts.getRawParameterValue("in_out_link")->load()>.5f)
    {
        const juce::String other=id=="fet_input"?"fet_output":"fet_input";auto* q=audioProcessor.apvts.getParameter(other);
        if(gestures.insert(other).second)q->beginChangeGesture();
        const auto target=juce::jlimit(0.f,48.f,q->convertFrom0to1(q->getValue())-(value-before));q->setValueNotifyingHost(q->convertTo0to1(target));
    }return true;
}
void SanMuCompAudioProcessorEditor::saveUi(const juce::var& data)
{
    if(!data.isObject())return;
    for(const auto* key:{"language","tips","history","grRange","levelRange","layers"})if(data.hasProperty(key))put(ui,key,data[key]);
    if(data.hasProperty("theme"))
    {const auto dark=data["theme"].toString()=="dark";audioProcessor.apvts.state.getChildWithProperty("id",Params::IDs::THEME).setProperty("value",dark?1.f:0.f,nullptr);put(ui,"theme",dark?"dark":"light");}
    if(number(data["scale"]))
    {requestWindowScale(juce::jlimit(.85,1.5,double(data["scale"])));}
    put(ui,"scale",windowScale);audioProcessor.apvts.state.setProperty("ui_web_config",juce::JSON::toString(ui,true),nullptr);
    audioProcessor.apvts.state.setProperty("ui_history_visible",bool(ui["history"]),nullptr);
    preferences.save(ui);repaint();
}
void SanMuCompAudioProcessorEditor::restoreWithPreferences(const juce::ValueTree& source)
{
    auto copy=source.createCopy();const auto current=audioProcessor.apvts.copyState();
    copy.getChildWithProperty("id","theme").setProperty("value",current.getChildWithProperty("id","theme").getProperty("value"),nullptr);
    for(int i=0;i<current.getNumProperties();++i){const auto key=current.getPropertyName(i);if(key.toString().startsWith("ui_")||key.toString()=="listen_target_gr")copy.setProperty(key,current.getProperty(key),nullptr);}
    audioProcessor.apvts.replaceState(copy);audioProcessor.notifyHostAllParameters();
}
juce::var SanMuCompAudioProcessorEditor::handleCommand(const juce::String& command,const juce::var& data)
{
    if(command=="initialize"){ready=true;auto v=state();put(v,"schema",schema());put(v,"presets",presets());return v;}
    if(command=="state")return state();
    if(command=="begin"){finishGestures();audioProcessor.undoManager.beginNewTransaction();return true;}
    if(command=="end"){finishGestures();return state();}
    if(command=="set")
    {if(!data.isObject()||!number(data["value"])||!setParameter(data["id"].toString(),float(data["value"])))return failure("参数值超出范围");return state();}
    if(command=="ui")
    {if(!data.isObject()||juce::JSON::toString(data).length()>4096)return failure("界面设置无效");saveUi(data);return state();}
    if(command=="undo"||command=="redo")
    {finishGestures();if(command=="undo")audioProcessor.undoManager.undo();else audioProcessor.undoManager.redo();audioProcessor.notifyHostAllParameters();return state();}
    if(command=="ab")
    {
        finishGestures();if(data["slot"].toString()=="A"&&audioProcessor.getActiveSnapshot()!=SanMuCompAudioProcessor::ActiveSnapshot::A)audioProcessor.clickA();
        if(data["slot"].toString()=="B"&&audioProcessor.getActiveSnapshot()!=SanMuCompAudioProcessor::ActiveSnapshot::B)audioProcessor.clickB();return state();
    }
    if(command=="copyAB"){finishGestures();audioProcessor.copyActiveSnapshotToOther();return state();}
    if(command=="loadPreset")
    {
        const auto id=data["id"].toString();const auto before=audioProcessor.apvts.copyState();finishGestures();audioProcessor.undoManager.beginNewTransaction();
        if(id.startsWith("factory:"))
        {const auto suffix=id.substring(8);if(!suffix.containsOnly("0123456789")||suffix.isEmpty())return failure("预设不存在");const int index=suffix.getIntValue();
            if(index<0||index>=PresetManager::getNumFactoryPresets())return failure("预设不存在");const auto& p=PresetManager::getFactoryPreset(index);PresetManager::applyPreset(p,audioProcessor.apvts);presetName=p.name;}
        else if(id.startsWith("user:")&&PresetManager::getUserPresetNames().contains(id.substring(5)))
        {if(!PresetManager::loadUserPreset(id.substring(5),audioProcessor.apvts))return failure("预设读取失败");presetName=id.substring(5);}
        else return failure("预设不存在");
        // 预设只改声音，沿用当前外观和助理目标。
        for(int i=0;i<before.getNumProperties();++i){const auto key=before.getPropertyName(i);if(key.toString().startsWith("ui_")||key.toString()=="listen_target_gr")audioProcessor.apvts.state.setProperty(key,before.getProperty(key),nullptr);}
        audioProcessor.apvts.state.getChildWithProperty("id","theme").setProperty("value",before.getChildWithProperty("id","theme").getProperty("value"),nullptr);
        audioProcessor.notifyHostAllParameters();presetValues=soundParameters();audioProcessor.apvts.state.setProperty("ui_preset_name",presetName,nullptr);return state();
    }
    if(command=="savePreset")
    {
        const auto name=data["name"].toString().trim();if(name.isEmpty()||name.length()>60||name.containsAnyOf("/\\:"))return failure("请输入不含路径符号的预设名称");
        if(PresetManager::getUserPresetNames().contains(name)&&!bool(data["overwrite"]))return failure("同名预设已存在，请更换名称");
        if(!PresetManager::saveUserPreset(name,audioProcessor.apvts))return failure("预设保存失败");presetName=name;presetValues=soundParameters();audioProcessor.apvts.state.setProperty("ui_preset_name",name,nullptr);
        auto v=state();put(v,"presets",presets());return v;
    }
    if(command=="listenStart")
    {
        if(listenFuture.valid()||audioProcessor.getFeatureExtractor().isListening())return failure("正在测算，请先结束当前任务");
        if(!number(data["target"])||double(data["target"])<.5||double(data["target"])>24)return failure("目标须在 0.5 至 24 dB 之间");
        const int mode=int(audioProcessor.apvts.getRawParameterValue("mode")->load());listenOptions={float(data["target"]),mode};
        audioProcessor.apvts.state.setProperty("listen_target_gr",double(data["target"]),nullptr);startListen(bool(data["reuse"]));return state();
    }
    if(command=="listenStop")
    {auto& e=audioProcessor.getFeatureExtractor();if(e.isListening()){if(e.isWaitingForSignal()||e.getRecordedSeconds()<1.0)return failure("请先播放至少 1 秒有效素材");e.stopListeningAndCompute();}return state();}
    if(command=="listenCancel")
    {audioProcessor.getFeatureExtractor().cancelListening();if(listenCancelled)listenCancelled->store(true);listenStatus=listenFuture.valid()?"cancelling":"idle";return state();}
    if(command=="listenApply")
    {if(!listenResult||!listenResult->ready||!listenCapture||std::abs(listenCapture->sampleRate-audioProcessor.getSampleRate())>.01)return failure("请在当前采样率下重新测算");finishGestures();applyListen();return state();}
    if(command=="listenRestore")
    {if(listenUndoState.isValid()){finishGestures();audioProcessor.undoManager.beginNewTransaction();restoreWithPreferences(listenUndoState);listenUndoState={};listenApplied=false;listenStatus="ready";}return state();}
    return failure("未知请求");
}
void SanMuCompAudioProcessorEditor::startListen(bool reuse)
{
    listenResult.reset();listenApplied=false;listenError={};listenStartParameters=soundParameters();
    if(reuse&&listenCapture){analyseListen();return;}
    listenCapture.reset();listenStatus="capturing";auto& e=audioProcessor.getFeatureExtractor();e.onAnalysisCompleted=nullptr;
    e.onCaptureCompleted=[safe=juce::Component::SafePointer<SanMuCompAudioProcessorEditor>(this)](ai::ListenCapture capture)
    {if(safe){safe->listenCapture=std::make_shared<const ai::ListenCapture>(std::move(capture));safe->analyseListen();}};
    e.startListening(Params::listenDurationSeconds,true);
}
void SanMuCompAudioProcessorEditor::analyseListen()
{
    if(!listenCapture||listenFuture.valid())return;listenCancelled=std::make_shared<std::atomic<bool>>(false);listenStatus="analysing";
    const bool smooth=audioProcessor.apvts.getRawParameterValue("vox_smooth")->load()>=.5f;
    listenFuture=std::async(std::launch::async,[capture=listenCapture,cancelled=listenCancelled,smooth,options=listenOptions]
    {return ai::ListenAssistant::analyse(*capture,smooth,ai::AcousticSourceType::Unknown,cancelled.get(),options);});
}
void SanMuCompAudioProcessorEditor::pollListen()
{
    if(listenApplied&&listenAppliedParameters!=soundParameters()){listenApplied=false;listenStatus="ready";}
    if(!listenFuture.valid()||listenFuture.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return;
    try
    {
        auto r=listenFuture.get();if(listenCancelled->load()){listenResult.reset();listenStatus="idle";return;}
        listenResult=std::make_unique<ai::ListenResult>(std::move(r));audioProcessor.getFeatureExtractor().acceptAnalysis(listenResult->features);
        listenStatus=listenResult->ready?"ready":"review";
        if(listenResult->ready&&gestures.empty()&&listenStartParameters==soundParameters()&&listenCapture&&std::abs(listenCapture->sampleRate-audioProcessor.getSampleRate())<.01)applyListen();
    }catch(...){listenStatus="error";listenError=juce::String::fromUTF8("测算未完成，原参数已保留。");}
}
void SanMuCompAudioProcessorEditor::applyListen()
{
    if(!listenResult||!listenResult->ready)return;listenUndoState=audioProcessor.apvts.copyState();
    audioProcessor.applyPresetRecommendation(listenResult->preset);listenApplied=true;listenAppliedParameters=soundParameters();listenStatus="applied";
    presetName="ANALYZE / "+ai::ListenAssistant::modeName(listenResult->preset.mode);
}
void SanMuCompAudioProcessorEditor::timerCallback()
{
    if(resizeSaveDeadline>0&&juce::Time::getMillisecondCounterHiRes()>=resizeSaveDeadline)flushResizePreferences();
    pollListen();if(!ready)return;const double now=juce::Time::getMillisecondCounterHiRes();
    const auto peaks=audioProcessor.getCompressor().consumeMeterPeaks();const auto timeout=audioProcessor.getCompressor().getMeterIdleTimeoutMs();
    for(size_t i=0;i<3;++i){if(std::isfinite(peaks[i])){meterValues[i]=peaks[i];meterTimes[i]=now;}else if(now-meterTimes[i]>=timeout)meterValues[i]=i==2?0.f:-100.f;}
    auto packet=state();auto meter=object();put(meter,"time",now);put(meter,"input",meterValues[0]);put(meter,"output",meterValues[1]);put(meter,"gr",meterValues[2]);put(packet,"meter",meter);
    browser->emitEventIfBrowserIsVisible("sanmuFrame",packet);
}
