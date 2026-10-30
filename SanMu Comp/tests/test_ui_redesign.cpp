#include "PluginEditor.h"
#include <iostream>
#include <stdexcept>
struct GestureProbe final : juce::AudioProcessorParameter::Listener
{
    int begins=0,ends=0;
    void parameterValueChanged(int,float) override {}
    void parameterGestureChanged(int,bool start) override {if(start)++begins;else ++ends;}
};
static juce::var data(const char* key,const juce::var& value)
{juce::var v=new juce::DynamicObject();v.getDynamicObject()->setProperty(key,value);return v;}
static void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(int argc,char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    try
    {
        SanMuCompAudioProcessor p;p.setRateAndBufferSizeDetails(48000,256);p.prepareToPlay(48000,256);
        auto e=std::make_unique<SanMuCompAudioProcessorEditor>(p);
        if(argc==3&&juce::String(argv[1])=="--schema")
        {juce::File(argv[2]).replaceWithText(juce::JSON::toString(e->handleCommand("initialize")));return 0;}
        juce::DocumentWindow window(juce::String::fromUTF8("SanMu Comp 界面验证"),juce::Colour(0xffe9ecef),juce::DocumentWindow::closeButton);
        window.setContentNonOwned(e.get(),true);window.setVisible(true);
        const auto eval=[&](const juce::String& code){auto result=std::make_shared<std::pair<bool,juce::var>>();e->getBrowser().evaluateJavascript(code,[result](auto r){if(r.getResult())result->second=*r.getResult();result->first=true;});const auto end=juce::Time::getMillisecondCounterHiRes()+5000;while(!result->first&&juce::Time::getMillisecondCounterHiRes()<end)juce::MessageManager::getInstance()->runDispatchLoopUntil(10);return result->second;};
        bool ready=false;for(int i=0;i<120&&!ready;++i){juce::MessageManager::getInstance()->runDispatchLoopUntil(50);ready=bool(eval("Boolean(window.__sanmuComp?.nativeReady)"));}
        if(!ready)std::cerr<<eval("document.body.innerText").toString()<<'\n';
        require(ready,"离线网页未连接原生参数");
        if(argc==2&&juce::String(argv[1])=="--preview")
        {
            // 仅供本机查看同一个插件编辑器；关闭或两分钟后退出，不加载宿主工程。
            const auto end=juce::Time::getMillisecondCounterHiRes()+120000;
            while(window.isVisible()&&juce::Time::getMillisecondCounterHiRes()<end)
                juce::MessageManager::getInstance()->runDispatchLoopUntil(25);
            window.clearContentComponent();e.reset();return 0;
        }

        // 原生网页容器中的自绘菜单：主题、按键及声音参数隔离。
        eval("window.__menuSoundBefore=JSON.stringify(window.__sanmuComp.values);document.querySelector('#gr-range').click()");
        require(bool(eval("!document.querySelector('#gr-range-menu').hidden&&document.querySelector('#gr-range').tagName==='BUTTON'")),"GR 自绘菜单未打开");
        eval("document.querySelector('#gr-range-menu button[value=\"24\"]').click()");
        juce::MessageManager::getInstance()->runDispatchLoopUntil(150);
        require(bool(eval("window.__sanmuComp.ui.grRange===24&&document.querySelector('#gr-range-menu').hidden&&JSON.stringify(window.__sanmuComp.values)===window.__menuSoundBefore")),"GR 菜单未保存档位或影响声音参数");
        eval("document.querySelector('#gr-range').click();document.activeElement.dispatchEvent(new KeyboardEvent('keydown',{key:'Escape',bubbles:true}))");
        require(bool(eval("document.querySelector('#gr-range-menu').hidden&&document.activeElement.id==='gr-range'")),"GR 菜单 Esc 未关闭并恢复焦点");
        eval("window.__sanmuComp.preference('grRange',12)");
        eval("document.querySelector('#level-range').click();document.querySelector('#level-range-menu button[value=\"30\"]').click()");
        juce::MessageManager::getInstance()->runDispatchLoopUntil(150);
        require(bool(eval("window.__sanmuComp.ui.levelRange===30&&JSON.stringify(window.__sanmuComp.values)===window.__menuSoundBefore")),"IN/OUT 放大未持久保存或改变声音");
        require(int(e->handleCommand("state")["ui"]["levelRange"])==30,"原生状态未保存电平显示量程");


        eval("window.__sanmuComp.change('vca_threshold',-24)");juce::MessageManager::getInstance()->runDispatchLoopUntil(250);
        require(std::abs(p.apvts.getRawParameterValue("vca_threshold")->load()+24)<.01,"网页旋钮未写回真实参数");
        auto* parameter=p.apvts.getParameter("vca_makeup");parameter->setValueNotifyingHost(parameter->convertTo0to1(3.2f));juce::MessageManager::getInstance()->runDispatchLoopUntil(250);
        require(std::abs(double(eval("window.__sanmuComp.values.vca_makeup"))-3.2)<.01,"宿主参数未同步页面");
        for(int mode=0;mode<4;++mode)
        {
            eval("window.__sanmuComp.change('mode',"+juce::String(mode)+")");juce::MessageManager::getInstance()->runDispatchLoopUntil(150);
            require(bool(eval(R"JS((()=>{const panel=document.querySelector('.instrument').getBoundingClientRect();return [...document.querySelectorAll('.knob,.fet-ratios,.smooth-control,.opto-mode')].every(e=>{const b=e.getBoundingClientRect();return b.left>=panel.left&&b.right<=panel.right+1&&b.bottom<=panel.bottom+1;});})())JS")),"原生窗口有控件超出面板");
        }
        // 验证真实容器中的功能提示、设置回路和边缘缩放，不改声音参数。
        eval("window.__sanmuComp.preference('language','zh');window.__sanmuComp.preference('tips','follow');window.__soundBefore=JSON.stringify(window.__sanmuComp.values)");
        juce::MessageManager::getInstance()->runDispatchLoopUntil(150);
        for(const auto scale:{.85,1.0,1.15,1.25,1.5})
        {
            eval("window.__sanmuComp.preference('scale',"+juce::String(scale)+");document.querySelector('#settings').click()");
            juce::MessageManager::getInstance()->runDispatchLoopUntil(200);
            require(bool(eval(R"JS((()=>{const d=document.querySelector('#settings-dialog'),b=d.getBoundingClientRect();return b.left>=0&&b.top>=0&&b.right<=innerWidth+1&&b.bottom<=innerHeight+1&&d.scrollHeight<=d.clientHeight+1;})())JS")),"设置面板缩放溢出或截断");
            eval("document.querySelector('#settings-dialog .close').click()");
        }
        eval("window.__sanmuComp.preference('scale',1);document.querySelector('#settings').click();document.querySelector('#help').click()");
        require(bool(eval("document.querySelector('#help-dialog').open&&document.querySelector('#help-dialog').textContent.includes('Command/Ctrl')&&!document.querySelector('#help-dialog').textContent.includes('Shift')")),"帮助入口缺少快捷键说明");
        for(const auto scale:{.85,1.0,1.5})
        {
            eval("window.__sanmuComp.preference('scale',"+juce::String(scale)+")");juce::MessageManager::getInstance()->runDispatchLoopUntil(100);
            require(bool(eval(R"JS((()=>{const d=document.querySelector('#help-dialog');d.querySelectorAll('details').forEach(e=>e.open=true);d.scrollTop=d.scrollHeight;const b=d.getBoundingClientRect(),back=d.querySelector('.back-settings').getBoundingClientRect(),close=d.querySelector('.close').getBoundingClientRect();return b.left>=0&&b.right<=innerWidth+1&&back.bottom<=b.bottom&&back.top>=b.top&&close.top>=b.top&&close.bottom<=b.bottom&&d.scrollWidth<=d.clientWidth+1;})())JS")),"长手册内容、返回或关闭按钮被截断");
        }
        eval("window.__sanmuComp.preference('scale',1);window.__sanmuComp.preference('language','en');window.__sanmuComp.open('help-dialog')");
        require(bool(eval("document.querySelector('[data-manual=common]').textContent.includes('Both channels share')&&document.querySelectorAll('#manual-content details').length===5")),"英文手册未载入");
        eval("window.__sanmuComp.preference('language','zh');window.__sanmuComp.open('help-dialog')");
        eval("document.querySelector('#help-dialog .back-settings').click();document.querySelector('#about').click()");
        require(bool(eval("document.querySelector('#about-dialog').open")),"关于入口无效");
        eval("document.querySelector('#about-dialog .close').click()");
        for(const auto mode:{0,1,2,3})
        {
            eval("window.__sanmuComp.change('mode',"+juce::String(mode)+")");juce::MessageManager::getInstance()->runDispatchLoopUntil(100);
            require(bool(eval(R"JS([...document.querySelectorAll('.knob,[name=mode]')].every(e=>{const key=e.closest('[data-tip]')?.dataset.tip;const text=key&&window.__sanmuComp.t(key);return text&&text!==key&&!text.includes('Shift')&&/[\u4e00-\u9fff]/.test(text);}))JS")),"功能提示遗漏或仍为快捷键");
        }
        eval("document.querySelector('#knob-opto_gain').dispatchEvent(new PointerEvent('pointerover',{bubbles:true}))");
        juce::MessageManager::getInstance()->runDispatchLoopUntil(650);
        require(bool(eval(juce::String::fromUTF8("!document.querySelector('#tooltip').hidden&&document.querySelector('#tooltip').textContent.includes('光电压缩后的音量')"))),"中文悬停提示未出现");
        eval("window.__sanmuComp.hideTip();window.__sanmuComp.preference('tips','en');document.querySelector('#knob-opto_gain').dispatchEvent(new PointerEvent('pointerover',{bubbles:true}))");
        juce::MessageManager::getInstance()->runDispatchLoopUntil(650);
        require(bool(eval("document.querySelector('#tooltip').textContent.includes('optical compression')")),"独立英文提示无效");
        eval("window.__sanmuComp.hideTip();window.__sanmuComp.preference('tips','off');document.querySelector('#knob-opto_gain').dispatchEvent(new PointerEvent('pointerover',{bubbles:true}))");
        juce::MessageManager::getInstance()->runDispatchLoopUntil(650);
        require(bool(eval("document.querySelector('#tooltip').hidden")),"关闭提示后仍出现浮层");
        require(bool(eval(juce::String::fromUTF8("!document.querySelector('#mode-description')&&document.querySelector('.history-duration').textContent==='最近 6 秒'&&document.querySelector('#listen span').textContent==='测算'"))),"界面文案未更新");
        require(bool(eval("window.__soundBefore===JSON.stringify(window.__sanmuComp.values)")),"设置操作改变了声音参数");
        eval("window.__sanmuComp.preference('tips','follow')");
        std::cout<<"功能提示双语及关闭、五档设置布局、帮助与关于入口、显示文案与声音隔离：通过\n";
        eval("window.__sanmuComp.change('mode',1);window.__sanmuComp.change('vca_release',1500)");juce::MessageManager::getInstance()->runDispatchLoopUntil(200);
        require(eval("document.querySelector('#edit-vca_release').value").toString()=="AUTO","自动释放被错误显示为毫秒");
        eval("window.__sanmuComp.change('mode',0);window.__sanmuComp.change('vox_smooth',1)");juce::MessageManager::getInstance()->runDispatchLoopUntil(200);
        require(p.apvts.getRawParameterValue("vox_smooth")->load()==1,"平滑按钮没有驱动参数");
        require(!bool(eval("Boolean(document.querySelector('[data-parameter=stereo_link],[data-parameter=output_trim]'))")),"LEV 显示了无效历史参数");
        p.undoManager.clearUndoHistory();e->handleCommand("ui",data("theme","dark"));
        require(!p.undoManager.canUndo(),"主题进入声音撤销栈");
        e->handleCommand("ab",data("slot","A"));e->handleCommand("copyAB");e->handleCommand("ab",data("slot","B"));
        require(p.apvts.getRawParameterValue("vox_smooth")->load()==1,"复制到 B 未包含当前声音");
        require(p.apvts.getRawParameterValue("theme")->load()==1,"A/B 改变主题");
        auto invalid=data("id","vca_attack");invalid.getDynamicObject()->setProperty("value",-8);
        require(e->handleCommand("set",invalid).hasProperty("error"),"原生入口允许越界参数");
        invalid.getDynamicObject()->setProperty("value","25");require(e->handleCommand("set",invalid).hasProperty("error"),"原生入口允许错误参数类型");
        juce::AudioBuffer<float> block(2,256);juce::MidiBuffer midi;
        for(int frame=0;frame<30;++frame)
        {
            for(int n=0;n<256;++n){const auto value=.25f*std::sin(float(n+frame*256)*.03f);block.setSample(0,n,value);block.setSample(1,n,value);}
            p.processBlock(block,midi);juce::MessageManager::getInstance()->runDispatchLoopUntil(8);
        }
        require(double(eval("window.__sanmuComp.visual.latest.input"))>-15,"真实音频没有驱动新电平");
        eval("document.querySelector('#pause').click();window.__historyBefore=JSON.stringify(window.__sanmuComp.visual.frames)");
        for(int i=0;i<10;++i){block.clear();p.processBlock(block,midi);juce::MessageManager::getInstance()->runDispatchLoopUntil(12);}
        require(bool(eval("window.__historyBefore===JSON.stringify(window.__sanmuComp.visual.frames)")),"暂停没有冻结历史");
        require(double(eval("window.__sanmuComp.visual.latest.input"))<=-90,"暂停历史也冻结了实时电平");
        eval("document.querySelector('#pause').click();window.__sanmuComp.preference('history',false)");juce::MessageManager::getInstance()->runDispatchLoopUntil(150);
        require(bool(eval("document.querySelector('#history').hidden&&window.__sanmuComp.visual.frames.length===0")),"关闭历史保留了过期画面");
        eval("document.querySelector('#listen').click();document.querySelector('#listen-target').value='25';document.querySelector('#listen-start').click()");
        require(!p.getFeatureExtractor().isListening()&&eval("document.querySelector('#listen-error').textContent").toString().isNotEmpty(),"目标输入没有拦截错误值");
        eval("document.querySelector('#listen-target').value='6';document.querySelector('#listen-start').click()");juce::MessageManager::getInstance()->runDispatchLoopUntil(120);
        require(p.getFeatureExtractor().isListening()&&p.getFeatureExtractor().isWaitingForSignal(),"网页测算未进入等待输入");
        require(eval("window.__sanmuComp.listen.status").toString()=="waiting","等待状态未回传界面");
        for(int i=0;i<1200;++i){block.clear();p.processBlock(block,midi);}
        juce::MessageManager::getInstance()->runDispatchLoopUntil(120);
        require(p.getFeatureExtractor().getRecordedSeconds()==0&&p.getFeatureExtractor().isWaitingForSignal(),"宿主静音消耗了五秒采集");
        require(e->handleCommand("listenStop").hasProperty("error"),"等待时允许分析空素材");
        for(int i=0;i<950;++i){for(int n=0;n<256;++n){const auto value=.2f*std::sin(float(n+i*256)*.031f);block.setSample(0,n,value);block.setSample(1,n,value);}p.processBlock(block,midi);if(i==100){juce::MessageManager::getInstance()->runDispatchLoopUntil(100);require(eval("window.__sanmuComp.listen.status").toString()=="capturing","有效信号未进入采集状态");}}
        require(!p.getFeatureExtractor().isListening(),"五秒素材没有自动结束采集");
        for(int i=0;i<600&&eval("window.__sanmuComp.listen.status").toString()!="applied";++i)juce::MessageManager::getInstance()->runDispatchLoopUntil(50);
        require(eval("window.__sanmuComp.listen.status").toString()=="applied","原生五秒测算未完成自动应用");
        eval("document.querySelector('#listen-target').value='5';document.querySelector('#listen-target').dispatchEvent(new KeyboardEvent('keydown',{key:'Enter',bubbles:true}))");juce::MessageManager::getInstance()->runDispatchLoopUntil(150);
        require(p.getFeatureExtractor().isWaitingForSignal(),"回车没有开始新的等待");
        eval("document.querySelector('#listen-cancel').click()");juce::MessageManager::getInstance()->runDispatchLoopUntil(150);
        require(!p.getFeatureExtractor().isListening()&&eval("window.__sanmuComp.listen.status").toString()=="idle","等待输入期间无法取消");
        std::cout<<"原生测算：等待静音、有效输入触发、五秒采集、分析自动应用、回车和取消：通过\n";
        GestureProbe probe;auto* gestured=p.apvts.getParameter("fet_attack");gestured->addListener(&probe);
        e->handleCommand("begin");auto setting=data("id","fet_attack");setting.getDynamicObject()->setProperty("value",5.1);e->handleCommand("set",setting);
        require(probe.begins==1&&probe.ends==0,"手势未正确开始");
        juce::MemoryBlock saved;p.getStateInformation(saved);
        window.clearContentComponent();e.reset();require(probe.begins==probe.ends,"关闭窗口遗留自动化手势");gestured->removeListener(&probe);
        p.setStateInformation(saved.getData(),int(saved.getSize()));e=std::make_unique<SanMuCompAudioProcessorEditor>(p);window.setContentNonOwned(e.get(),true);
        ready=false;for(int i=0;i<100&&!ready;++i){juce::MessageManager::getInstance()->runDispatchLoopUntil(50);ready=bool(eval("Boolean(window.__sanmuComp?.nativeReady)"));}
        require(ready&&eval("document.documentElement.dataset.theme").toString()=="dark"&&bool(eval("document.querySelector('#history').hidden")),"重开窗口未恢复主题与显示偏好");
        std::cout<<"原生网页离线加载、四模式布局、真实参数双向同步、AUTO、平滑、A/B、范围拒绝：通过\n";
        std::cout<<"真实音频电平、暂停与显示关闭、LISTEN 采集、自动化手势收尾、工程重开：通过\n";
        window.clearContentComponent();e.reset();
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
