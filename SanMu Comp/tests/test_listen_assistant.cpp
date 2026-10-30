#include "ai/ListenAssistant.h"
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <iostream>
#include <stdexcept>
#include <thread>

static void require(bool value, const char* message)
{ if (!value) throw std::runtime_error(message); }

static ai::ListenCapture drum(double sr, double duration = 2.0, float gain = 1)
{
    ai::ListenCapture c; c.sampleRate = sr;
    c.mid.resize(size_t(sr * duration)); c.side.resize(c.mid.size());
    juce::Random random(7341);
    for (size_t n = 0; n < c.mid.size(); ++n)
    {
        const double t = double(n) / sr, age = std::fmod(t + .25, .5);
        // 长尾与持续底噪刻意提高占空比，检验拍头不会被旧门限吞掉。
        const double envelope = std::exp(-age / .16);
        c.mid[n] = gain * float(.24 * envelope * (random.nextFloat() * 2 - 1)
                   + .2 * std::exp(-age / .08) * std::sin(2 * juce::MathConstants<double>::pi * 180 * t));
    }
    return c;
}

static void features()
{
    for (double sr : {44100.,48000.,96000.,192000.})
    {
        const auto c = drum(sr);
        const auto original = ai::AcousticFeatureExtractor::analyseCapture(c);
        require(original.sourceType == ai::AcousticSourceType::SnareDrum, "长尾噪声瞬态没有识别为打击乐");
        require(original.percussiveOnsetCount >= 2, "未检测到连续拍头");
        for (float scale : {.001f,.25f,2.f})
        {
            auto scaled = c; for (auto& x : scaled.mid) x *= scale;
            auto f = ai::AcousticFeatureExtractor::analyseCapture(scaled);
            require(f.sourceType == original.sourceType, "改变输入音量导致声源类别改变");
            require(std::abs(f.noiseBandFlatness-original.noiseBandFlatness)<.002f, "谱形受绝对音量影响");
        }
        auto reversed = c; reversed.side = c.mid; std::fill(reversed.mid.begin(),reversed.mid.end(),0);
        require(ai::AcousticFeatureExtractor::analyseCapture(reversed).sourceType == original.sourceType,
                "左右反相导致打击声源丢失");
    }
    auto silence = drum(48000);std::fill(silence.mid.begin(),silence.mid.end(),0);
    require(ai::AcousticFeatureExtractor::analyseCapture(silence).sourceType == ai::AcousticSourceType::Unknown,
            "静音被当作可识别声源");
    require(!ai::ListenAssistant::analyse(silence,false).ready, "静音生成了可应用方案");
    require(!ai::ListenAssistant::analyse(drum(48000,.3),false).ready, "过短捕获未经拦截");
    std::cout << "相对电平、跨采样率、反相、静音与短片段：通过\n";
}

static void recommendations()
{
    const auto c = drum(48000);
    auto r = ai::ListenAssistant::analyse(c,false);
    require(r.ready && r.preset.mode == Params::ModeFET, "明确打击乐未得到合理起点");
    require(std::abs(r.preset.measuredGrDb-r.preset.requestedGrDb)<.6f, "真实引擎深度未校准");
    require(std::abs(r.preset.calibratedRmsDb-r.preset.calibrationInputRmsDb)<.4f, "有效电平未匹配");
    require(r.candidates.size()==4, "没有比较四种独立引擎");
    // 手动信息更新偏好而不是固定绑定；同一录音无需重新采集。
    auto corrected = ai::ListenAssistant::analyse(c,true,ai::AcousticSourceType::LeadVocal);
    require(corrected.ready && corrected.features.sourceType == ai::AcousticSourceType::LeadVocal, "无法纠正类别");
    for (auto& candidate : corrected.candidates) require(candidate.voxSmooth && candidate.voxGate == -80, "改变了平滑选择或擅自关闭尾音");
    auto uncertain = ai::ListenAssistant::analyse(c,false,ai::AcousticSourceType::AmbiguousInstrument);
    require(uncertain.ready && uncertain.preset.mode == Params::ModeVCA, "低置信度素材没有回退到 VCA 通用起点");
    require(uncertain.candidates.size()==1, "歧义类别仍被其他引擎评分覆盖");
    auto uncertainTarget = ai::ListenAssistant::analyse(c,false,ai::AcousticSourceType::BackingVocals,nullptr,{6,-1});
    require(uncertainTarget.ready && uncertainTarget.preset.mode == Params::ModeVCA
            && uncertainTarget.candidates.size()==1 && uncertainTarget.preset.requestedGrDb==6,
            "人工深度加歧义回退没有保持目标及 VCA");
    auto fixed = ai::ListenAssistant::analyse(c,false,ai::AcousticSourceType::AmbiguousInstrument,nullptr,{6,Params::ModeOpto});
    require(fixed.ready && fixed.preset.mode == Params::ModeOpto && fixed.candidates.size()==1,
            "歧义回退覆盖了用户固定引擎");
    const auto unreachableFallback = ai::ListenAssistant::analyse(drum(48000,2,.0001f),false,
        ai::AcousticSourceType::AmbiguousInstrument,nullptr,{24,-1});
    require(!unreachableFallback.ready && unreachableFallback.preset.mode == Params::ModeVCA
            && unreachableFallback.candidates.size()==1 && unreachableFallback.preset.requestedGrDb==24,
            "通用回退不可达时换了引擎或降低了人工目标");
    std::atomic<bool> cancelled {true};
    require(!ai::ListenAssistant::analyse(c,false,ai::AcousticSourceType::Unknown,&cancelled).ready,
            "取消后返回可应用结果");
    std::cout << "候选比较、实际 GR、配平、类别纠正、取消：通过\n";
}

static void requestedTargets()
{
    const auto c = drum(48000);
    for (int mode = 0; mode < 4; ++mode)
    for (float target : {2.0f, 6.0f, 10.0f})
    {
        const auto r = ai::ListenAssistant::analyse(c, false, ai::AcousticSourceType::Unknown, nullptr, {target, mode});
        require(r.candidates.size() == 1 && r.preset.mode == mode, "固定引擎时仍尝试或切换了其他模式");
        require(r.features.sourceType == ai::AcousticSourceType::Unknown && r.features.inferredSource.empty(),
                "手动目标仍执行了声源分类");
        require(std::abs(r.preset.requestedGrDb-target)<.001f,"用户目标被偷偷降低");
        std::cout << "固定模式=" << mode << " 目标=" << target << " 实测=" << r.preset.measuredGrDb
                  << " 可应用=" << r.ready << '\n';
        require(r.ready && std::abs(r.preset.measuredGrDb-target)<.5f,"正常素材未达到手动目标");
    }
    auto autoMode = ai::ListenAssistant::analyse(c, false, ai::AcousticSourceType::Unknown, nullptr, {6,-1});
    require(autoMode.ready && autoMode.candidates.size()==4, "手动深度加自动选型没有比较候选");
    for (const auto& candidate : autoMode.candidates)
        require(std::abs(candidate.requestedGrDb-6)<.001f,"自动选型改掉了手动目标");
    auto quiet = c; for(auto& x:quiet.mid) x *= .0001f;
    const auto unreachable = ai::ListenAssistant::analyse(quiet,false,ai::AcousticSourceType::Unknown,nullptr,{24,Params::ModeFET});
    require(!unreachable.ready && unreachable.candidates.size()==1 && unreachable.preset.mode==Params::ModeFET,
            "无法达到目标时偷偷换模式或应用");
    require(std::abs(unreachable.preset.requestedGrDb-24)<.001f,"失败时下调了用户目标");
    for (float invalid : {-1.f,.1f,25.f,std::numeric_limits<float>::quiet_NaN()})
        require(!ai::ListenAssistant::analyse(c,false,ai::AcousticSourceType::Unknown,nullptr,{invalid,2}).ready,"非法目标未拦截");
    auto silence = c; std::fill(silence.mid.begin(), silence.mid.end(), 0);
    require(!ai::ListenAssistant::analyse(silence,false,ai::AcousticSourceType::Unknown,nullptr,{6,Params::ModeVCA}).ready,
            "停用识别后静音绕过了有效音频检查");
    auto manualVca = ai::ListenAssistant::analyse(c,false,ai::AcousticSourceType::LeadVocal,nullptr,{6,Params::ModeVCA});
    require(manualVca.ready && manualVca.features.sourceType == ai::AcousticSourceType::Unknown
            && manualVca.preset.vcaRatio == 2 && !manualVca.message.contains(juce::String::fromUTF8("声源")),
            "分类信息仍干扰手动目标参数或公开结果");
    std::cout << "手动深度、分类停用、固定引擎、静音与不可达目标：通过\n";
}

static juce::var request(SanMuCompAudioProcessorEditor& e,const juce::String& command,float target=4,bool reuse=false)
{
    juce::var data=new juce::DynamicObject();data.getDynamicObject()->setProperty("target",target);
    data.getDynamicObject()->setProperty("reuse",reuse);return e.handleCommand(command,data);
}
static std::vector<float> values(SanMuCompAudioProcessor& p)
{std::vector<float> v;for(auto* param:p.getParameters())v.push_back(param->getValue());return v;}
static void set(SanMuCompAudioProcessor& p,const char* id,float v)
{auto* param=p.apvts.getParameter(id);param->setValueNotifyingHost(param->convertTo0to1(v));}
static void pump(){juce::Thread::sleep(20);juce::Timer::callPendingTimersSynchronously();}
static juce::String status(SanMuCompAudioProcessorEditor& e){return e.handleCommand("state")["listen"]["status"].toString();}
static void interface()
{
    for(int scenario=0;scenario<7;++scenario)
    {
        SanMuCompAudioProcessor p;p.setRateAndBufferSizeDetails(48000,256);p.prepareToPlay(48000,256);
        const int mode=scenario==4?Params::ModeFET:scenario==5?Params::ModeOpto:scenario==6?Params::ModeVOX:Params::ModeVCA;
        set(p,Params::IDs::MODE,float(mode));auto before=values(p);
        auto editor=std::make_unique<SanMuCompAudioProcessorEditor>(p);
        for(float invalid:{0.f,25.f,std::numeric_limits<float>::quiet_NaN()})
            require(request(*editor,"listenStart",invalid).hasProperty("error")&&!p.getFeatureExtractor().isListening(),"非法目标启动了捕获");
        request(*editor,"listenStart",6);require(p.getFeatureExtractor().isListening(),"合法目标未采集");
        const auto capture=drum(48000,2);auto& extractor=p.getFeatureExtractor();
        for(size_t n=0;n<capture.mid.size();n+=256)
        {
            const int count=int(std::min<size_t>(256,capture.mid.size()-n));juce::AudioBuffer<float>b(2,count);
            for(int i=0;i<count;++i){b.setSample(0,i,capture.mid[n+size_t(i)]);b.setSample(1,i,capture.mid[n+size_t(i)]);}extractor.processBlock(b);
        }
        request(*editor,"listenStop");require(status(*editor)=="analysing","提前结束未启动后台测算");require(values(p)==before,"分析前参数被覆盖");
        if(scenario==1){set(p,Params::IDs::VCA_MAKEUP,3);before=values(p);}
        if(scenario==2)request(*editor,"listenCancel");
        if(scenario==3){const auto start=juce::Time::getMillisecondCounterHiRes();editor.reset();require(juce::Time::getMillisecondCounterHiRes()-start<1500,"关闭未及时取消后台任务");require(values(p)==before,"关闭后结果修改参数");continue;}
        const auto start=juce::Time::getMillisecondCounterHiRes();
        while((status(*editor)=="analysing"||status(*editor)=="cancelling")&&juce::Time::getMillisecondCounterHiRes()-start<30000)pump();
        require(status(*editor)!="analysing","分析超时");
        if(scenario==0||scenario>=4)
        {
            require(status(*editor)=="applied","合理测算结果未应用");
            require(int(p.apvts.getRawParameterValue("mode")->load())==mode,"测算切换了用户指定模式");
            require(extractor.getLatestFeatures().sourceType==ai::AcousticSourceType::Unknown,"公开路径启用了分类");
            require(p.undoManager.undo(),"推荐没有撤销事务");require(values(p)==before,"一次撤销未恢复完整声音参数");
        }else require(values(p)==before,"取消或编辑后被后台结果覆盖");
        if(scenario==2)
        {
            require(bool(editor->handleCommand("state")["listen"]["hasCapture"]),"取消后录音不能复用");
            request(*editor,"listenStart",2,true);require(status(*editor)=="analysing"&&!extractor.isListening(),"复用错误地重新采集");
            const auto began=juce::Time::getMillisecondCounterHiRes();while(status(*editor)=="analysing"&&juce::Time::getMillisecondCounterHiRes()-began<30000)pump();
            require(status(*editor)=="applied","复用测算未完成");
        }
        juce::MemoryBlock saved;p.getStateInformation(saved);editor.reset();SanMuCompAudioProcessor restored;restored.setStateInformation(saved.getData(),int(saved.getSize()));
        require(float(restored.apvts.state.getProperty("listen_target_gr"))==(scenario==2?2:6),"目标未随工程保存");
    }
    std::cout<<"真实捕获、四模式目标、自动应用、手动改动保护、取消、关闭、复用、撤销、工程恢复：通过\n";
}
int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    try{features();recommendations();requestedTargets();interface();}
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
