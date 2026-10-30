#pragma once
#include "BetaNotice.h"
#include "UiPreferences.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"
#include "ai/ListenAssistant.h"
#include <future>
#include <set>

// 界面只通过参数桥接访问处理器，网页不持有或运行音频引擎。
class SanMuCompAudioProcessorEditor final : public juce::AudioProcessorEditor, private juce::Timer, private juce::AsyncUpdater
{
public:
    explicit SanMuCompAudioProcessorEditor(SanMuCompAudioProcessor&);
    ~SanMuCompAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    juce::var handleCommand(const juce::String&, const juce::var& = {});
    juce::WebBrowserComponent& getBrowser() { return *browser; }
private:
    SanMuUiPreferences preferences { "SanMu Comp" };
    class Browser final : public juce::WebBrowserComponent
    {
    public:
        using juce::WebBrowserComponent::WebBrowserComponent;
        bool pageAboutToLoad(const juce::String& url) override
        { return url == getResourceProviderRoot() || url == getResourceProviderRoot() + "index.html"; }
        void newWindowAttemptingToLoad(const juce::String&) override {}
    };
    SanMuCompAudioProcessor& audioProcessor;
    sanmu_beta::Notice betaNotice { audioProcessor.betaAccess };
    std::unique_ptr<Browser> browser;
    std::set<juce::String> gestures;
    bool ready = false;
    float windowScale = 1;
    juce::var ui;
    juce::String presetName = "Default Settings";
    std::vector<float> presetValues;
    std::array<float,3> meterValues {-100,-100,0};
    std::array<double,3> meterTimes {};
    ai::ListenOptions listenOptions;
    std::shared_ptr<const ai::ListenCapture> listenCapture;
    std::shared_ptr<std::atomic<bool>> listenCancelled;
    std::future<ai::ListenResult> listenFuture;
    std::unique_ptr<ai::ListenResult> listenResult;
    std::vector<float> listenStartParameters, listenAppliedParameters;
    juce::ValueTree listenUndoState;
    bool listenApplied = false;
    juce::String listenStatus = "idle", listenError;
    juce::var state() const;
    juce::var schema() const;
    juce::var presets() const;
    juce::var listenState() const;
    std::vector<float> soundParameters() const;
    bool setParameter(const juce::String&, float);
    void finishGestures();
    void saveUi(const juce::var&);
    void restoreWithPreferences(const juce::ValueTree&);
    void startListen(bool reuse);
    void analyseListen();
    void pollListen();
    void applyListen();
    // 缩放请求延后执行并合并，避免在网页桥接回调内重入宿主窗口调整。
    int pendingWidth = 0, pendingHeight = 0;
    bool nativeCallbackActive = false;
    double resizeSaveDeadline = 0;
    void requestWindowScale(double);
    void handleAsyncUpdate() override;
    void flushResizePreferences();
    void timerCallback() override;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SanMuCompAudioProcessorEditor)
};
