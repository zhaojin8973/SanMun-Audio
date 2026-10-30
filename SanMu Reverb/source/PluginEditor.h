#pragma once
#include "BetaNotice.h"
#include "UiPreferences.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <set>
#include "PluginProcessor.h"

class SanMuReverbAudioProcessorEditor final : public juce::AudioProcessorEditor, private juce::Timer, private juce::AsyncUpdater
{
public:
    explicit SanMuReverbAudioProcessorEditor(SanMuReverbAudioProcessor&);
    ~SanMuReverbAudioProcessorEditor() override;
    void resized() override;
    void paint(juce::Graphics&) override;
    juce::var handleCommand(const juce::String&, const juce::var&);
    juce::WebBrowserComponent& getBrowser() { return *browser; }
private:
    SanMuUiPreferences preferences { "SanMu Reverb" };
    class Browser final : public juce::WebBrowserComponent
    {
    public:
        using juce::WebBrowserComponent::WebBrowserComponent;
        bool pageAboutToLoad(const juce::String& url) override
        { return url == getResourceProviderRoot() || url == getResourceProviderRoot() + "studio.html"; }
        void newWindowAttemptingToLoad(const juce::String&) override {}
    };
    SanMuReverbAudioProcessor& processorRef;
    sanmu_beta::Notice betaNotice { processorRef.betaAccess };
    std::unique_ptr<Browser> browser;
    std::set<juce::String> gestures;
    bool transaction = false, ready = false;
    juce::var state() const;
    juce::var presets() const;
    void setParameter(const juce::String&, float);
    void finishGesture();
    // 缩放请求延后执行并合并，避免在网页桥接回调内重入宿主窗口调整。
    int pendingWidth = 0, pendingHeight = 0;
    bool nativeCallbackActive = false;
    double resizeSaveDeadline = 0;
    void requestWindowScale(double);
    void handleAsyncUpdate() override;
    void flushResizePreferences();
    void timerCallback() override;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SanMuReverbAudioProcessorEditor)
};
