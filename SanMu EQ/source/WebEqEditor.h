#pragma once
#include "BetaNotice.h"
#include "UiPreferences.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"
#include <set>

// 与 Comp / Delay 使用相同的离线网页视图，声音参数仍由 APVTS 管理。
class SanMuEqWebEditor final : public juce::AudioProcessorEditor, private juce::Timer, private juce::AsyncUpdater
{
public:
    explicit SanMuEqWebEditor(SanMuQ6AudioProcessor&);
    ~SanMuEqWebEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    juce::var handleCommand(const juce::String&, const juce::var&);
private:
    SanMuUiPreferences preferences { "SanMu EQ" };
    class Browser final : public juce::WebBrowserComponent
    {
    public:
        using juce::WebBrowserComponent::WebBrowserComponent;
        bool pageAboutToLoad(const juce::String& url) override
        { return url == getResourceProviderRoot() || url == getResourceProviderRoot() + "index.html"; }
        void newWindowAttemptingToLoad(const juce::String&) override {}
    };
    SanMuQ6AudioProcessor& audioProcessor;
    sanmu_beta::Notice betaNotice { audioProcessor.betaAccess };
    std::unique_ptr<Browser> browser;
    std::set<juce::String> gestures;
    std::array<EqBand, Params::MAX_BANDS> displayBands;
    juce::var ui;
    juce::String presetName = "Default Settings";
    std::vector<float> presetValues;
    bool ready = false;
    int frameCount = 0;
    double auditionDeadline = 0;
    uint64_t lastMeterSequence = 0;
    double lastMeterUpdate = 0;
    juce::var state() const;
    juce::var schema() const;
    juce::var presets() const;
    juce::var frame(bool includeCurves = true);
    std::vector<float> soundValues() const;
    bool setParameter(const juce::String&, const juce::var&);
    void finishGestures();
    void saveUi();
    // 缩放请求延后执行并合并，避免在网页桥接回调内重入宿主窗口调整。
    int pendingWidth = 0, pendingHeight = 0;
    bool nativeCallbackActive = false;
    double resizeSaveDeadline = 0;
    void requestWindowScale(double);
    void handleAsyncUpdate() override;
    void flushResizePreferences();
    void timerCallback() override;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SanMuEqWebEditor)
};
