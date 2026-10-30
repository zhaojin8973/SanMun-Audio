#pragma once
#include "BetaAccess.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace sanmu_beta
{
// 独立于网页刷新与工程状态的底栏提示组件。
class Notice final : public juce::Component, private juce::Timer
{
public:
    explicit Notice(const Access& value) : access(value) { setInterceptsMouseClicks(false,false); startTimer(1000); }
    void paint(juce::Graphics& g) override
    {
        const auto s=access.status();
        g.fillAll(juce::Colour(s == Status::active ? 0xff222936 : 0xff693d27));
        g.setColour(juce::Colours::white); g.setFont(12.f);
        juce::String text;
        if (s == Status::expired) text=juce::String::fromUTF8("内测已到期 · 原声直通，参数已保留，请安装新版 / Beta expired — update required");
        else if (s == Status::clockError) text=juce::String::fromUTF8("系统时间异常 · 请校正日期后重开插件 / Check system date and reopen plugin");
        else if (s == Status::storageError) text=juce::String::fromUTF8("无法保存内测记录 · 原声直通，请检查用户目录权限 / Check user folder permissions");
        else if (s == Status::expiring) text=juce::String::fromUTF8("内测即将到期 · 请提前冻结或导出轨道");
        else text=juce::String::fromUTF8("SanMu Audio · Open Source Edition / 开源版");
        g.drawFittedText(text,getLocalBounds().reduced(8,0),juce::Justification::centred,1);
    }
private:
    const Access& access;
    void timerCallback() override { repaint(); }
};
}
