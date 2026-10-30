// ==============================================================================
// PluginEditor.h - SanMu Delay 现代化离线 Web 插件编辑器
// 遵循 SanMu UI 设计系统：HTML/CSS/JS 界面，JUCE WebView 离线容器，原生持有参数权威
// ==============================================================================

#pragma once
#include "BetaNotice.h"
#include "UiPreferences.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"
#include <set>

class SanMuDelayAudioProcessorEditor final : public juce::AudioProcessorEditor, private juce::Timer, private juce::AsyncUpdater
{
public:
    explicit SanMuDelayAudioProcessorEditor(SanMuDelayAudioProcessor&);
    ~SanMuDelayAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    juce::var handleCommand(const juce::String&, const juce::var& = {});
    juce::WebBrowserComponent& getBrowser() { return *browser; }

private:
    SanMuUiPreferences preferences { "SanMu Delay" };
    class Browser final : public juce::WebBrowserComponent
    {
    public:
        using juce::WebBrowserComponent::WebBrowserComponent;
        bool pageAboutToLoad(const juce::String& url) override
        {
            return url == getResourceProviderRoot() || url == getResourceProviderRoot() + "index.html";
        }
        void newWindowAttemptingToLoad(const juce::String&) override {}
    };

    SanMuDelayAudioProcessor& audioProcessor;
    sanmu_beta::Notice betaNotice { audioProcessor.betaAccess };
    std::unique_ptr<Browser> browser;
    std::set<juce::String> gestures;
    bool ready = false;
    float windowScale = 1.0f;
    juce::var ui;
    juce::String presetName = "Default Settings";
    std::vector<float> presetValues;
    std::vector<float> lastSentSoundParameters;
    bool hasSentState = false;

    juce::var state() const;
    juce::var schema() const;
    juce::var presets() const;
    std::vector<float> soundParameters() const;
    bool setParameter(const juce::String&, float);
    void finishGestures();
    void saveUi(const juce::var&);
    void restoreWithPreferences(const juce::ValueTree&);
    // 缩放请求延后执行并合并，避免在网页桥接回调内重入宿主窗口调整。
    int pendingWidth = 0, pendingHeight = 0;
    bool nativeCallbackActive = false;
    double resizeSaveDeadline = 0;
    void requestWindowScale(double);
    void handleAsyncUpdate() override;
    void flushResizePreferences();
    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SanMuDelayAudioProcessorEditor)
};
