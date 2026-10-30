// 中央电平历史与增益衰减视图；所有交互只影响显示。
#include "InputLevelMeter.h"
#include "LookAndFeel.h"
#include <algorithm>
#include <cmath>
#include <limits>

InputLevelMeter::InputLevelMeter(CompressorProcessor& compressorRef) : compressor(compressorRef)
{
    setComponentID("input_level_meter");
    clearHistory();
    for (auto* button : {&inputButton, &outputButton, &grButton, &rangeButton, &pauseButton, &clearButton})
        addAndMakeVisible(button);
    const char* ids[] = {"history_input", "history_output", "history_gr"};
    int i = 0;
    for (auto* button : {&inputButton, &outputButton, &grButton})
    {
        button->setComponentID(ids[i++]);
        button->setClickingTogglesState(true);
        button->setToggleState(true, juce::dontSendNotification);
        button->onClick = [this] { repaint(); };
    }
    rangeButton.setComponentID("history_range");
    rangeButton.onClick = [this] { grRange = grRange == 12 ? 24 : grRange == 24 ? 6 : 12; setEnglish(english); };
    pauseButton.setComponentID("history_pause");
    pauseButton.setClickingTogglesState(true);
    pauseButton.onClick = [this]
    {
        paused = pauseButton.getToggleState();
        if (!paused) clearHistory();
        setEnglish(english);
    };
    clearButton.setComponentID("meter_clear");
    clearButton.onClick = [this] { resetPeakHold(); };
    setEnglish(true);
    startTimerHz(60);
}

void InputLevelMeter::setEnglish(bool value)
{
    english = value;
    inputButton.setButtonText("IN"); outputButton.setButtonText("OUT"); grButton.setButtonText("GR");
    rangeButton.setButtonText("GR " + juce::String(grRange) + " dB");
    pauseButton.setButtonText(english ? (paused ? "Resume" : "Pause") : juce::String::fromUTF8(paused ? "继续" : "暂停"));
    clearButton.setButtonText(english ? "Clear peaks" : juce::String::fromUTF8("清除峰值"));
    inputButton.setTooltip(english ? "Show input sample-peak history (dBFS)." : juce::String::fromUTF8("显示输入采样峰值历史，单位 dBFS。"));
    outputButton.setTooltip(english ? "Show output sample-peak history (dBFS)." : juce::String::fromUTF8("显示输出采样峰值历史，单位 dBFS。"));
    grButton.setTooltip(english ? "Show gain reduction (dB); output makeup is excluded." : juce::String::fromUTF8("显示增益衰减，单位 dB；不含输出补偿增益。"));
    rangeButton.setTooltip(english ? "Cycle the GR display range: 6, 12, 24 dB." : juce::String::fromUTF8("切换 GR 显示范围：6、12、24 dB；不改变声音。"));
    pauseButton.setTooltip(english ? "Freeze history for inspection. Audio and live meters continue." : juce::String::fromUTF8("暂停历史曲线以便观察；音频和实时电平表继续运行。"));
    clearButton.setTooltip(english ? "Reset peak holds and input/output overload indicators." : juce::String::fromUTF8("清除峰值保持和输入、输出过载标记。"));
    repaint();
}

void InputLevelMeter::clearHistory()
{
    inputHistory.fill(-100); outputHistory.fill(-100); grHistory.fill(0);
    timeHistory.fill(std::numeric_limits<double>::quiet_NaN());
    writeIndex = 0; hoverPosition = {-1, -1};
}

void InputLevelMeter::setHistoryVisible(bool value)
{
    if (historyVisible == value) return;
    historyVisible = value; paused = false;
    pauseButton.setToggleState(false, juce::dontSendNotification);
    clearHistory(); setEnglish(english);
    for (auto* button : {&inputButton, &outputButton, &grButton, &rangeButton, &pauseButton})
        button->setEnabled(value);
    repaint();
}

void InputLevelMeter::resized()
{
    inputButton.setBounds(36, 0, 44, 26); outputButton.setBounds(84, 0, 50, 26); grButton.setBounds(138, 0, 44, 26);
    rangeButton.setBounds(getWidth() - 360, 0, 94, 26);
    pauseButton.setBounds(getWidth() - 260, 0, 72, 26);
    clearButton.setBounds(getWidth() - 118, 0, 118, 26);
}

juce::Rectangle<float> InputLevelMeter::grBounds() const { return {36, 46, float(getWidth() - 220), 72}; }
juce::Rectangle<float> InputLevelMeter::levelBounds() const { return {36, 144, float(getWidth() - 220), float(getHeight() - 176)}; }

void InputLevelMeter::resetPeakHold()
{
    peakInHold = peakOutHold = -100; peakGrHold = 0;
    peakInHoldTimer = peakOutHoldTimer = peakGrHoldTimer = 0;
    inputClipped = outputClipped = false;
    repaint();
}
void InputLevelMeter::mouseMove(const juce::MouseEvent& e) { hoverPosition = e.position; repaint(); }
void InputLevelMeter::mouseExit(const juce::MouseEvent&) { hoverPosition = {-1, -1}; repaint(); }
void InputLevelMeter::mouseDoubleClick(const juce::MouseEvent& e)
{
    if (e.position.x > getWidth() - 150) resetPeakHold();
}
void InputLevelMeter::timerCallback() { updateDisplay(juce::Time::getMillisecondCounterHiRes()); }

void InputLevelMeter::updateDisplay(double nowMs)
{
    const auto peaks = compressor.consumeMeterPeaks();
    const double timeoutMs = compressor.getMeterIdleTimeoutMs();
    for (size_t i = 0; i < displayedPeaks.size(); ++i)
    {
        if (std::isfinite(peaks[i]))
        {
            displayedPeaks[i] = peaks[i];
            lastDataTimeMs[i] = nowMs;
        }
        else if (nowMs - lastDataTimeMs[i] >= timeoutMs)
        {
            displayedPeaks[i] = i == 2 ? 0.0f : -100.0f;
        }
        // 音频块之间保持最近一次有效读数；真正静音的新块仍会立即更新。
    }
    const float curIn = displayedPeaks[0], curOut = displayedPeaks[1], gr = displayedPeaks[2];

    if (historyVisible && !paused)
    {
    timeHistory[static_cast<size_t>(writeIndex)] = nowMs;
    inputHistory[static_cast<size_t>(writeIndex)]  = curIn;
    outputHistory[static_cast<size_t>(writeIndex)] = curOut;
    grHistory[static_cast<size_t>(writeIndex)]     = gr;

    writeIndex = (writeIndex + 1) % kHistorySize;
    }
    inputClipped = inputClipped || curIn >= 0.0f;
    outputClipped = outputClipped || curOut >= 0.0f;

    // 平滑输入电平与 峰值保持
    smoothIn += 0.50f * (curIn - smoothIn);
    if (curIn >= peakInHold)
    {
        peakInHold = curIn;
        peakInHoldTimer = 35;
    }
    else
    {
        if (peakInHoldTimer > 0)
            --peakInHoldTimer;
        else
            peakInHold += 0.08f * (curIn - peakInHold);
    }

    // 平滑 GR 与 峰值保持（仅平滑表头显示，不改变引擎包络）
    smoothGr += 0.65f * (gr - smoothGr);
    if (gr >= peakGrHold)
    {
        peakGrHold = gr;
        peakGrHoldTimer = 25;
    }
    else
    {
        if (peakGrHoldTimer > 0)
            --peakGrHoldTimer;
        else
            peakGrHold += 0.15f * (gr - peakGrHold);
    }

    // 平滑输出电平与 峰值保持
    smoothOut += 0.50f * (curOut - smoothOut);
    if (curOut >= peakOutHold)
    {
        peakOutHold = curOut;
        peakOutHoldTimer = 35;
    }
    else
    {
        if (peakOutHoldTimer > 0)
            --peakOutHoldTimer;
        else
            peakOutHold += 0.06f * (curOut - peakOutHold);
    }

    repaint();
}

void InputLevelMeter::paint(juce::Graphics& g)
{
    SanMuCompLookAndFeel::syncThemeFrom(*this);
    using L = SanMuCompLookAndFeel;
    if (getWidth() < 460 || getHeight() < 210) return;
    const bool dark = L::getTheme() == SanMuCompTheme::Dark;
    const auto text = L::textWhite(), muted = L::textLight();
    const auto in = L::accentGreen(), out = L::accentYellow(), gr = L::accentPink();
    const auto gainPlot = grBounds(), levelPlot = levelBounds();
    const auto grid = L::glassEdge().withAlpha(dark ? .20f : .26f);
    auto font = [&](float size, bool strong = false) { g.setFont(juce::FontOptions("Avenir Next", strong ? "Demi Bold" : "Medium", size)); };
    for (auto* button : {&inputButton, &outputButton, &grButton, &rangeButton, &pauseButton, &clearButton})
    {
        const auto colour = button == &inputButton ? in : button == &outputButton ? out : button == &grButton ? gr : text;
        button->setColour(juce::TextButton::textColourOnId, colour);
        button->setColour(juce::TextButton::textColourOffId, muted);
    }
    font(14); g.setColour(muted);
    g.drawText(english ? "2 s history" : juce::String::fromUTF8("2 秒历史"), 194, 0, 132, 26, juce::Justification::centredLeft);
    g.setColour(grid); g.drawVerticalLine(getWidth() - 151, 40, float(getHeight() - 10));

    // 增益衰减和电平各用独立刻度，避免把 dB 与 dBFS 当作同一物理量。
    auto drawGrid = [&](juce::Rectangle<float> r, bool gain)
    {
        for (int i = 0; i <= (gain ? 2 : 4); ++i)
        {
            const float fraction = float(i) / (gain ? 2 : 4);
            const float y = r.getY() + fraction * r.getHeight();
            g.setColour(grid); g.drawHorizontalLine(juce::roundToInt(y), r.getX(), r.getRight());
            g.setColour(gain ? gr : muted); font(12.5f);
            const int value = gain ? -juce::roundToInt(fraction * grRange) : -juce::roundToInt(fraction * 60);
            g.drawText(juce::String(value), juce::Rectangle<float>(0, y - 6, 28, 12), juce::Justification::centredRight);
        }
        g.setColour(muted); font(12.5f);
        g.drawText(gain ? "dB" : "dBFS", juce::Rectangle<float>(0, r.getY() - 20, 32, 16), juce::Justification::centredRight);
    };
    drawGrid(gainPlot, true); drawGrid(levelPlot, false);
    const int latest = (writeIndex - 1 + kHistorySize) % kHistorySize;
    const double newest = timeHistory[size_t(latest)];
    auto xFor = [&](int index) { return levelPlot.getRight() - float((newest - timeHistory[size_t(index)]) / 2000.0) * levelPlot.getWidth(); };
    auto yFor = [&](float value, bool gain) { const auto r = gain ? gainPlot : levelPlot; return r.getY() + std::clamp(gain ? value / grRange : -value / 60.f, 0.f, 1.f) * r.getHeight(); };
    auto drawTrace = [&](const auto& values, bool gain, juce::Colour colour, bool dashed)
    {
        const auto r = gain ? gainPlot : levelPlot;
        juce::Path line;
        bool started = false;
        for (int i = kHistorySize - 1; i >= 0; --i)
        {
            const int index = (latest - i + kHistorySize) % kHistorySize;
            if (!std::isfinite(timeHistory[size_t(index)]) || newest - timeHistory[size_t(index)] > 2000) continue;
            const float x = xFor(index), y = yFor(values[size_t(index)], gain);
            if (!started) { line.startNewSubPath(x, y); started = true; } else line.lineTo(x, y);
        }
        if (!started) return;
        g.saveState(); g.reduceClipRegion(r.expanded(1).toNearestInt());
        if (!dashed)
        {
            auto fill = line; fill.lineTo(r.getRight(), gain ? r.getY() : r.getBottom());
            fill.lineTo(line.getBounds().getX(), gain ? r.getY() : r.getBottom()); fill.closeSubPath();
            g.setColour(colour.withAlpha(dark ? .10f : .07f)); g.fillPath(fill);
        }
        g.setColour(colour);
        if (dashed)
        {
            const float dashes[] = {4, 3}; juce::Path dashedLine;
            juce::PathStrokeType(1.3f).createDashedStroke(dashedLine, line, dashes, 2);
            g.fillPath(dashedLine);
        }
        else g.strokePath(line, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.restoreState();
    };
    if (historyVisible)
    {
        if (inputButton.getToggleState()) drawTrace(inputHistory, false, in, true);
        if (outputButton.getToggleState()) drawTrace(outputHistory, false, out, false);
        if (grButton.getToggleState()) drawTrace(grHistory, true, gr, false);
    }
    font(12.5f); g.setColour(muted);
    for (int i = 0; i <= 4; ++i)
    {
        const float x = levelPlot.getX() + levelPlot.getWidth() * i / 4;
        const juce::String time = i == 4 ? (english ? "Now" : juce::String::fromUTF8("现在")) : juce::String(-2 + i * .5, 1) + " s";
        g.drawText(time, juce::Rectangle<float>(x - 22, levelPlot.getBottom() + 6, 44, 16), juce::Justification::centred);
    }
    if (!historyVisible || !std::isfinite(newest))
    {
        font(15); g.setColour(muted);
        g.drawText(!historyVisible ? (english ? "History display off" : juce::String::fromUTF8("历史曲线已关闭"))
            : (english ? "Awaiting signal" : juce::String::fromUTF8("等待音频")), levelPlot, juce::Justification::centred);
    }
    if (paused)
    {
        font(14, true); g.setColour(gr);
        g.drawText(english ? "PAUSED | live meters continue" : juce::String::fromUTF8("已暂停 · 电平表继续运行"),
                   gainPlot.withY(gainPlot.getY() - 20).withHeight(16), juce::Justification::centredRight);
    }
    // 悬停读数对应同一时间点的三个采样峰值，不能将输入减输出冒充 GR。
    if (historyVisible && std::isfinite(newest) && (gainPlot.contains(hoverPosition) || levelPlot.contains(hoverPosition)))
    {
        int selected = latest; float distance = std::numeric_limits<float>::max();
        for (int i = 0; i < kHistorySize; ++i)
            if (std::isfinite(timeHistory[size_t(i)]) && newest - timeHistory[size_t(i)] <= 2000)
                if (const float d = std::abs(xFor(i) - hoverPosition.x); d < distance) { distance = d; selected = i; }
        const float x = xFor(selected);
        g.setColour(muted.withAlpha(.5f)); g.drawVerticalLine(juce::roundToInt(x), gainPlot.getY(), levelPlot.getBottom());
        const auto n = size_t(selected); font(14, true); g.setColour(text);
        const auto readout = juce::String((timeHistory[n] - newest) / 1000, 2) + " s   IN " + juce::String(inputHistory[n], 1)
            + "   OUT " + juce::String(outputHistory[n], 1) + " dBFS   GR " + juce::String(grHistory[n], 1) + " dB";
        g.drawText(readout, juce::Rectangle<float>(36, 120, levelPlot.getWidth(), 20), juce::Justification::centredLeft);
    }

    // 独立的实时表头保持运行；过载标记直到用户清除。
    auto meter = [&](int column, const char* label, juce::Colour colour, float value, float held, bool gain, bool clipped)
    {
        const float x = float(getWidth() - 136 + column * 47);
        const auto r = juce::Rectangle<float>(x + 14, 58, 10, float(getHeight() - 109));
        font(14, true); g.setColour(clipped ? gr : text);
        g.drawText(clipped ? "CLIP" : label, juce::Rectangle<float>(x - 3, 34, 44, 20), juce::Justification::centred);
        g.setColour(grid); g.fillRoundedRectangle(r, 4);
        const auto norm = [&](float v) { return std::clamp(gain ? v / grRange : (v + 60) / 60, 0.f, 1.f); };
        const float amount = norm(value) * r.getHeight();
        auto fill = gain ? r.withHeight(amount) : r.withTop(r.getBottom() - amount);
        g.setColour(colour.withAlpha(.84f)); g.fillRoundedRectangle(fill, 3);
        const float holdY = gain ? r.getY() + norm(held) * r.getHeight() : r.getBottom() - norm(held) * r.getHeight();
        if (gain ? held > .05f : held > -60)
        { g.setColour(text); g.drawLine(r.getX() - 3, holdY, r.getRight() + 3, holdY, 1); }
        font(18, true); g.setColour(text);
        juce::String number = !gain && value < -60 ? juce::String::fromUTF8("−∞") : juce::String(gain ? (value < .05f ? 0.f : -value) : value, 1);
        g.drawText(number, juce::Rectangle<float>(x - 5, r.getBottom() + 8, 46, 20), juce::Justification::centred);
        font(11); g.setColour(muted);
        g.drawText(gain && value > grRange ? "> " + juce::String(grRange) + " dB" : gain ? "dB" : "dBFS",
            juce::Rectangle<float>(x - 5, r.getBottom() + 28, 46, 14), juce::Justification::centred);
    };
    meter(0, "IN", in, smoothIn, peakInHold, false, inputClipped);
    meter(1, "GR", gr, smoothGr, peakGrHold, true, false);
    meter(2, "OUT", out, smoothOut, peakOutHold, false, outputClipped);
}
