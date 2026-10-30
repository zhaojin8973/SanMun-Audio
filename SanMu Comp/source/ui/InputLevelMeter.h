// ==============================================================================
// InputLevelMeter.h - 中央大区域：双层动态波形 + 垂直三联表（IN 绿 / GR 粉 / OUT 黄）
// ==============================================================================

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>

#include "../dsp/CompressorProcessor.h"

class InputLevelMeter : public juce::Component, private juce::Timer
{
public:
    explicit InputLevelMeter(CompressorProcessor& compressorRef);
    ~InputLevelMeter() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    void setEnglish(bool value);
    void setHistoryVisible(bool value);
    void clearHistory();
    void mouseDoubleClick(const juce::MouseEvent& e) override;

private:
    friend struct InputLevelMeterTestAccess;
    void timerCallback() override;
    void updateDisplay(double nowMs);
    juce::Rectangle<float> grBounds() const;
    juce::Rectangle<float> levelBounds() const;
    void resetPeakHold();

    CompressorProcessor& compressor;

    // 波形历史点数（约 2.13 秒历史，每秒刷新 60 次）
    static constexpr int kHistorySize = 128;
    std::array<float, kHistorySize> inputHistory;
    std::array<float, kHistorySize> outputHistory;
    std::array<float, kHistorySize> grHistory;
    std::array<double, kHistorySize> timeHistory;
    int writeIndex = 0;
    int grRange = 12;
    bool english = true, paused = false, historyVisible = true;
    bool inputClipped = false, outputClipped = false;
    juce::Point<float> hoverPosition{-1, -1};
    juce::TextButton inputButton, outputButton, grButton, rangeButton, pauseButton, clearButton;

    std::array<float, 3> displayedPeaks{-100.0f, -100.0f, 0.0f};
    std::array<double, 3> lastDataTimeMs{-1.0e10, -1.0e10, -1.0e10};

    // 平滑电平与 Peak Hold 状态
    float smoothIn = -100.0f;
    float peakInHold = -100.0f;
    int   peakInHoldTimer = 0;

    float smoothGr = 0.0f;
    float peakGrHold = 0.0f;
    int   peakGrHoldTimer = 0;

    float smoothOut = -100.0f;
    float peakOutHold = -100.0f;
    int   peakOutHoldTimer = 0;
};
