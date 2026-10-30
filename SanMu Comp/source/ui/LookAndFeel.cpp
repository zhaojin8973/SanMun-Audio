// ==============================================================================
// LookAndFeel.cpp - 自定义外观实现（第五轮：浅色清新 —— 米白 + 薄荷绿 + 天蓝）
// ==============================================================================

#include "LookAndFeel.h"

// ---- 双主题配色 ----
static SanMuCompTheme currentTheme = SanMuCompTheme::Light;

void SanMuCompLookAndFeel::setTheme(SanMuCompTheme t) { currentTheme = t; }
SanMuCompTheme SanMuCompLookAndFeel::getTheme()       { return currentTheme; }

void SanMuCompLookAndFeel::syncThemeFrom(juce::Component& c)
{
    if (auto* lf = dynamic_cast<SanMuCompLookAndFeel*>(&c.getLookAndFeel()))
        currentTheme = lf->getInstanceTheme();
}

static juce::Colour themeColour(const juce::Colour& light,
                                const juce::Colour& dark)
{
    return currentTheme == SanMuCompTheme::Dark ? dark : light;
}

juce::Colour SanMuCompLookAndFeel::backgroundDark()
{ return themeColour(juce::Colour(0xffe9ecef), juce::Colour(0xff101218)); }
juce::Colour SanMuCompLookAndFeel::backgroundBase()
{ return themeColour(juce::Colour(0xfff5f6f8), juce::Colour(0xff292d36)); }
juce::Colour SanMuCompLookAndFeel::backgroundEnd()
{ return themeColour(juce::Colour(0xffeceef2), juce::Colour(0xff24272e)); }
juce::Colour SanMuCompLookAndFeel::backgroundMedium()
{ return themeColour(juce::Colour(0xffeceef2), juce::Colour(0xff292d36)); }
juce::Colour SanMuCompLookAndFeel::glassFill()
{ return themeColour(juce::Colour(0x66ffffff), juce::Colour(0x3dffffff)); }
juce::Colour SanMuCompLookAndFeel::glassFillStrong()
{ return themeColour(juce::Colour(0x85ffffff), juce::Colour(0x55ffffff)); }
juce::Colour SanMuCompLookAndFeel::glassEdge()
{ return themeColour(juce::Colour(0xff91a0b0), juce::Colour(0xff8593a8)); }
juce::Colour SanMuCompLookAndFeel::glassHighlight()
{ return themeColour(juce::Colour(0xffffffff), juce::Colour(0xffffffff)); }
juce::Colour SanMuCompLookAndFeel::glassShadow()
{ return themeColour(juce::Colour(0x14242b38), juce::Colour(0x66000000)); }
juce::Colour SanMuCompLookAndFeel::accentGreen()
{ return themeColour(juce::Colour(0xff377f70), juce::Colour(0xff8cc8b9)); }
juce::Colour SanMuCompLookAndFeel::accentPink()
{ return themeColour(juce::Colour(0xffb24f66), juce::Colour(0xffe9a1b3)); }
juce::Colour SanMuCompLookAndFeel::accentYellow()
{ return themeColour(juce::Colour(0xff9c772b), juce::Colour(0xffd9c48a)); }
juce::Colour SanMuCompLookAndFeel::accentIndigo()
{ return themeColour(juce::Colour(0xff60a5fa), juce::Colour(0xff60a5fa)); }
juce::Colour SanMuCompLookAndFeel::accentBlue()
{ return themeColour(juce::Colour(0xff0f766e), juce::Colour(0xff2dd4bf)); }
juce::Colour SanMuCompLookAndFeel::fetBlueStripe()
{ return themeColour(juce::Colour(0xff368fb8), juce::Colour(0xff4db2e2)); }
juce::Colour SanMuCompLookAndFeel::textLight()
{ return themeColour(juce::Colour(0xff606573), juce::Colour(0xffb6c1ce)); }
juce::Colour SanMuCompLookAndFeel::textWhite()
{ return themeColour(juce::Colour(0xff30333e), juce::Colour(0xfff4f4f6)); }
juce::Colour SanMuCompLookAndFeel::textDim()
{ return themeColour(juce::Colour(0xff606573), juce::Colour(0xffb6c1ce)); }
juce::Colour SanMuCompLookAndFeel::gridLine()
{ return themeColour(juce::Colour(0x1494a3b8), juce::Colour(0x22ffffff)); }
juce::Colour SanMuCompLookAndFeel::knobTrack()
{ return themeColour(juce::Colour(0x2294a3b8), juce::Colour(0x2ecbd5e1)); }

// ---- 频段颜色（第五轮：浅底可读的深色系，功能识别色） ----
static const juce::Colour bandColors[6] = {
    juce::Colour(0xff1d4ed8),  // Band 1: 深蓝
    juce::Colour(0xff059669),  // Band 2: 深绿
    juce::Colour(0xffd97706),  // Band 3: 琥珀
    juce::Colour(0xffe11d48),  // Band 4: 玫红
    juce::Colour(0xff7c3aed),  // Band 5: 紫
    juce::Colour(0xffc026d3),  // Band 6: 品红（天蓝与 Band 1 深蓝仍接近，换掉）
};

juce::Colour SanMuCompLookAndFeel::getBandColor(int bandIndex)
{
    jassert(bandIndex >= 0 && bandIndex < 6);
    return bandColors[bandIndex];
}

juce::Colour SanMuCompLookAndFeel::getModeColour(int mode, bool dark)
{
    switch (mode)
    {
        case 0: // VOX (现代流行人声 珊瑚粉 Coral Pink)
            return dark ? juce::Colour(0xFFE6A5BC) : juce::Colour(0xFF995570);
        case 1: // VCA (钴蓝 / 深海蓝)
            return dark ? juce::Colour(0xFFABC8E0) : juce::Colour(0xFF456C91);
        case 2: // FET (橙红 / 火焰橙)
            return dark ? juce::Colour(0xFFE5B091) : juce::Colour(0xFFA16844);
        case 3: // Opto (琥珀暖金 / 古铜黄)
            return dark ? juce::Colour(0xFFDCC5A0) : juce::Colour(0xFF866B45);
        default:
            return dark ? juce::Colour(0xFF86C8FF) : juce::Colour(0xFF1E88E5);
    }
}

juce::Colour SanMuCompLookAndFeel::getModeColour(int mode)
{
    return getModeColour(mode, getTheme() == SanMuCompTheme::Dark);
}

// ==============================================================================
// 玻璃面板绘制工具
// ==============================================================================

void SanMuCompLookAndFeel::drawGlassPanel(juce::Graphics& g, juce::Rectangle<float> b, float radius)
{
    const bool dark = getTheme() == SanMuCompTheme::Dark;
    g.setColour(glassShadow()); g.fillRoundedRectangle(b.translated(0, 2), radius);
    g.setGradientFill(juce::ColourGradient(dark ? juce::Colour(0xff373b43) : juce::Colour(0xfffafbfc), b.getX(), b.getY(),
        dark ? juce::Colour(0xff282c34) : juce::Colour(0xffeef0f4), b.getX(), b.getBottom(), false));
    g.fillRoundedRectangle(b, radius);
    g.setColour(glassEdge().withAlpha(.23f)); g.drawRoundedRectangle(b.reduced(.5f), radius, 1);
    g.setColour(juce::Colours::white.withAlpha(dark ? .05f : .65f));
    g.drawRoundedRectangle(b.reduced(1.5f), radius - 1, 1);
}

void SanMuCompLookAndFeel::drawValueBubble(juce::Graphics& g,
                                         juce::Rectangle<float> bounds,
                                         const juce::String& text,
                                         float alpha)
{
    if (alpha <= 0.01f || text.isEmpty())
        return;

    // 气泡本体：深灰底（浅色 UI 下的强对比气泡），圆角，小三角指向旋钮
    g.setColour(textWhite().withAlpha(0.88f * alpha));
    g.fillRoundedRectangle(bounds, 6.0f);

    // 小三角（指向下方旋钮）
    juce::Path pointer;
    const float triW = 8.0f;
    const float triH = 5.0f;
    pointer.startNewSubPath(bounds.getCentreX() - triW * 0.5f, bounds.getBottom());
    pointer.lineTo(bounds.getCentreX() + triW * 0.5f, bounds.getBottom());
    pointer.lineTo(bounds.getCentreX(), bounds.getBottom() + triH);
    pointer.closeSubPath();
    g.fillPath(pointer);

    // 文字（白色，居中）
    g.setColour(backgroundDark().withAlpha(0.95f * alpha));
    g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
    g.drawText(text, bounds, juce::Justification::centred, false);
}

// ==============================================================================
// 整块主玻璃板（第五轮：白色半透明浅色板，清新通透）
// ==============================================================================

void SanMuCompLookAndFeel::drawMainGlass(juce::Graphics& g, juce::Rectangle<float> b, float radius)
{
    drawGlassPanel(g, b, radius);
}

SanMuCompLookAndFeel::SanMuCompLookAndFeel()
{
    refreshThemeColours();
}

void SanMuCompLookAndFeel::refreshThemeColours()
{
    setTheme(theme);
    const bool dark = theme == SanMuCompTheme::Dark;

    setColour(juce::ResizableWindow::backgroundColourId, backgroundDark());
    setColour(juce::Slider::backgroundColourId,          backgroundMedium());
    setColour(juce::Slider::trackColourId,               knobTrack());
    setColour(juce::Slider::rotarySliderFillColourId,    accentBlue());
    setColour(juce::Slider::rotarySliderOutlineColourId, glassEdge().withAlpha(0.18f));
    setColour(juce::Slider::textBoxTextColourId,         textWhite());
    setColour(juce::Slider::textBoxBackgroundColourId,
              dark ? juce::Colour(0xff232a33) : juce::Colours::white.withAlpha(0.85f));
    setColour(juce::Slider::textBoxOutlineColourId,      glassEdge().withAlpha(0.22f));
    setColour(juce::Label::textColourId,                 textLight());
    setColour(juce::ComboBox::backgroundColourId,
              dark ? juce::Colour(0xff232b35) : juce::Colours::white.withAlpha(0.60f));
    // 第三十九轮：预设/模式文字与按钮同色（石板灰），保证顶部排外观一致
    setColour(juce::ComboBox::textColourId,              textWhite());
    setColour(juce::ComboBox::outlineColourId,           glassEdge().withAlpha(0.25f));
    setColour(juce::ComboBox::arrowColourId,             textLight().withAlpha(0.7f));
    setColour(juce::ComboBox::buttonColourId,            accentGreen());
    setColour(juce::PopupMenu::backgroundColourId,
              dark ? juce::Colour(0xf2242c36) : juce::Colours::white.withAlpha(0.96f));
    setColour(juce::PopupMenu::textColourId,             textWhite());
    setColour(juce::PopupMenu::highlightedBackgroundColourId, accentGreen().withAlpha(0.30f));
    setColour(juce::TextButton::buttonColourId,          accentBlue());
    setColour(juce::TextButton::textColourOffId,         textLight());
    setColour(juce::TextButton::textColourOnId,          textWhite());

    // Tooltip 配色：玻璃白底 + 深色文字（与整块玻璃板风格一致）
    setColour(juce::TooltipWindow::backgroundColourId,
              dark ? juce::Colour(0xFF232A33) : juce::Colour(0xEFFFFFFF));
    setColour(juce::TooltipWindow::textColourId,
              dark ? juce::Colour(0xFFE2E8F0) : juce::Colour(0xFF22303A));
    setColour(juce::TooltipWindow::outlineColourId,      glassEdge());

    // 设置默认字体
    setDefaultSansSerifTypefaceName("PingFang SC");
}

SanMuCompLookAndFeel::~SanMuCompLookAndFeel() = default;

// ==============================================================================
// Tooltip 绘制（第三十八轮）：中文字体 PingFang SC
// JUCE 6 的默认 tooltip 字体不支持 CJK 字形，中文显示为乱码；
// 这里用 macOS 系统黑体渲染，英文部分也由同一字体覆盖
// ==============================================================================

void SanMuCompLookAndFeel::drawTooltip(juce::Graphics& g,
                                     const juce::String& text,
                                     int width, int height)
{
    // 随主题的圆角气泡：无描边，文字用主色
    const bool dark = theme == SanMuCompTheme::Dark;
    const auto b = juce::Rectangle<float>(0.0f, 0.0f,
                                          static_cast<float>(width),
                                          static_cast<float>(height));
    g.fillAll(dark ? juce::Colour(0xFF232A33) : juce::Colour(0xFFF8FAFC));
    g.setColour(dark ? juce::Colour(0xE6303842) : juce::Colour(0xE6FFFFFF));
    g.fillRoundedRectangle(b, 6.0f);
    g.setColour(textWhite());
    g.setFont(juce::FontOptions("PingFang SC", 12.0f, juce::Font::plain));
    g.drawText(text,
               juce::Rectangle<int>(8, 3, width - 16, height - 6),
               juce::Justification::centredLeft, true);
}

juce::Rectangle<int> SanMuCompLookAndFeel::getTooltipBounds(
    const juce::String& tipText,
    juce::Point<int> screenPos,
    juce::Rectangle<int> parentArea)
{
    // 与 drawTooltip 同款字体计算宽高，保证中文多行文本不被截断
    const juce::Font f(juce::FontOptions("PingFang SC", 12.0f, juce::Font::plain));
    int w = 0;
    const juce::StringArray lines = juce::StringArray::fromLines(tipText);
    for (const auto& line : lines)
        w = juce::jmax(w, juce::roundToInt(
            juce::GlyphArrangement::getStringWidth(f, line)));
    w += 18;
    const int h = juce::roundToInt(f.getHeight()) * juce::jmax(1, lines.size()) + 10;

    return juce::Rectangle<int>(screenPos.x > parentArea.getCentreX()
                                    ? screenPos.x - (w + 12)
                                    : screenPos.x + 24,
                                screenPos.y > parentArea.getCentreY()
                                    ? screenPos.y - (h + 6)
                                    : screenPos.y + 6,
                                w, h)
        .constrainedWithin(parentArea);
}

// ==============================================================================
// 旋钮核心绘制 - 浅色清新式（第五轮：白色玻璃圆面 + 灰轨道 + 频段色弧 + 深青绿指针）
// 无 Slider 依赖，供 drawRotarySlider / EqBandControl / OutputKnobControl 复用
// ==============================================================================

void SanMuCompLookAndFeel::drawRotaryKnob(juce::Graphics& g,
                                        juce::Rectangle<float> bounds,
                                        float sliderPos,
                                        const juce::Colour& fillColor,
                                        bool hovered)
{
    const float radius = juce::jmin(bounds.getWidth() / 2.0f,
                                    bounds.getHeight() / 2.0f) - 4.0f;
    const float centreX = bounds.getCentreX();
    const float centreY = bounds.getCentreY();
    const float rx = centreX - radius;
    const float ry = centreY - radius;
    const float rw = radius * 2.0f;

    constexpr float startAngle = -2.356194f; // -135°
    constexpr float endAngle   =  2.356194f; // +135°
    const float toAngle = startAngle + sliderPos * (endAngle - startAngle);
    const float trackRadius = radius - 2.5f;

    // ---- 1. 白色/暗色磨砂玻璃底面 ----
    {
        const bool dark = getTheme() == SanMuCompTheme::Dark;
        juce::ColourGradient face(
            dark ? juce::Colour(0xcc2a323c) : juce::Colours::white.withAlpha(0.95f),
            centreX, centreY - radius * 0.4f,
            dark ? juce::Colour(0xd91b2129) : juce::Colour(0xffe4eae8),
            centreX, centreY + radius,
            true);
        g.setGradientFill(face);
        g.fillEllipse(rx, ry, rw, rw);

        // 1px 发丝边框（悬停时微提亮）
        g.setColour(hovered ? fillColor.withAlpha(0.35f) : glassEdge().withAlpha(0.18f));
        g.drawEllipse(rx, ry, rw, rw, 1.0f);
    }

    // ---- 2. 交互发丝能量弧（仅悬停/拖动时浮现） ----
    if (hovered)
    {
        // 底轨
        juce::Path trackPath;
        trackPath.addCentredArc(centreX, centreY, trackRadius, trackRadius, 0.0f, startAngle, endAngle, true);
        g.setColour(glassEdge().withAlpha(0.20f));
        g.strokePath(trackPath, juce::PathStrokeType(1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // 激活能量线
        if (sliderPos > 0.005f)
        {
            juce::Path filledPath;
            filledPath.addCentredArc(centreX, centreY, trackRadius, trackRadius, 0.0f, startAngle, toAngle, true);

            g.setColour(fillColor.withAlpha(0.25f));
            g.strokePath(filledPath, juce::PathStrokeType(3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            g.setColour(fillColor);
            g.strokePath(filledPath, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }
}

// ==============================================================================
// 填充弧绘制（无指针，颜色填充弧表示参数；三圈旋钮与输出旋钮复用）
// ==============================================================================

void SanMuCompLookAndFeel::drawArcRing(juce::Graphics& g,
                                     juce::Point<float> centre,
                                     float radius,
                                     float sliderPos,
                                     const juce::Colour& colour,
                                     bool hovered,
                                     bool showFill)
{
    constexpr float startAngle = -2.356f;   // -135°
    constexpr float endAngle   =  2.356f;   // +135°
    const float toAngle = startAngle + sliderPos * (endAngle - startAngle);

    // ---- 轨道弧（灰细线） ----
    juce::Path track;
    track.addCentredArc(centre.x, centre.y, radius, radius, 0.0f, startAngle, endAngle, true);
    g.setColour(glassEdge().withAlpha(hovered ? 0.20f : 0.13f));
    g.strokePath(track, juce::PathStrokeType(1.4f,
                           juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // ---- 参数填充弧（仅 showFill 时绘制） ----
    if (showFill)
    {
        juce::Path filled;
        filled.addCentredArc(centre.x, centre.y, radius, radius, 0.0f, startAngle, toAngle, true);

        g.setColour(colour.withAlpha(hovered ? 0.26f : 0.16f));
        g.strokePath(filled, juce::PathStrokeType(5.0f,
                               juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(colour);
        g.strokePath(filled, juce::PathStrokeType(2.6f,
                               juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

void SanMuCompLookAndFeel::drawGainArcRing(juce::Graphics& g,
                                         juce::Point<float> centre,
                                         float radius,
                                         float sliderPos,
                                         const juce::Colour& colour,
                                         bool hovered)
{
    // 0dB 基准 = 顶部（-90°）；正增益顺时针（向右下）、负增益逆时针（向左下）
    constexpr float zeroAngle = -juce::MathConstants<float>::halfPi;
    constexpr float range = 2.356f;   // ±135°

    const auto arcPath = [&](float a0, float a1, bool startNew)
    {
        juce::Path p;
        constexpr int N = 24;
        for (int i = 0; i <= N; ++i)
        {
            const float a = a0 + (a1 - a0) * static_cast<float>(i) / N;
            const float x = centre.x + radius * std::cos(a);
            const float y = centre.y + radius * std::sin(a);
            if (i == 0)
            {
                if (startNew) p.startNewSubPath(x, y);
                else          p.lineTo(x, y);
            }
            else
            {
                p.lineTo(x, y);
            }
        }
        return p;
    };

    // ---- 轨道弧（灰细线，顶部向左右各 135°） ----
    const juce::Path track = arcPath(zeroAngle - range, zeroAngle + range, true);
    g.setColour(glassEdge().withAlpha(hovered ? 0.20f : 0.13f));
    g.strokePath(track, juce::PathStrokeType(1.4f,
                         juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // ---- 0dB 基准点（顶部小圆点） ----
    g.setColour(glassEdge().withAlpha(hovered ? 0.55f : 0.40f));
    g.fillEllipse(centre.x - 1.5f, centre.y - radius - 1.5f, 3.0f, 3.0f);

    // ---- 方向弧：0dB 无颜色，正/负增益从顶部向对应方向延伸 ----
    const float span = (sliderPos - 0.5f) * 2.0f * range;   // -135°..+135°
    if (std::abs(span) > 0.01f)
    {
        const juce::Path filled = arcPath(zeroAngle, zeroAngle + span, true);
        const juce::Colour arcColour = span > 0.0f
            ? colour : colour.darker(0.30f);
        g.setColour(arcColour.withAlpha(hovered ? 0.26f : 0.16f));
        g.strokePath(filled, juce::PathStrokeType(5.0f,
                               juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(arcColour);
        g.strokePath(filled, juce::PathStrokeType(2.6f,
                               juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

void SanMuCompLookAndFeel::drawCompDial(juce::Graphics& g, juce::Rectangle<float> b, float position,
    const juce::String& title, const juce::String& value, const juce::Colour& accent, bool bipolar, bool hovered, bool hasAuto)
{
    const bool dark = getTheme() == SanMuCompTheme::Dark;
    const float diameter = juce::jmin(92.f, b.getWidth() - 18.f, b.getHeight() - 58.f);
    if (diameter < 16) return;
    const float r = diameter * .5f, cx = b.getCentreX(), cy = b.getY() + 26 + r;
    const auto face = juce::Rectangle<float>(cx - r + 7, cy - r + 7, diameter - 14, diameter - 14);
    constexpr float start = -2.356194f, end = 2.356194f;
    juce::Path track; track.addCentredArc(cx, cy, r, r, 0, start, end, true);
    g.setColour(accent.withAlpha(.26f)); g.strokePath(track, juce::PathStrokeType(1.6f));
    for (int i = 0; i <= 10; ++i)
    {
        const float a = start + (end - start) * i / 10;
        g.setColour(accent.withAlpha(.50f));
        g.drawLine(cx + (r + 3) * std::sin(a), cy - (r + 3) * std::cos(a),
                   cx + (r + 5) * std::sin(a), cy - (r + 5) * std::cos(a), .9f);
    }
    const float angle = start + std::clamp(position, 0.f, 1.f) * (end - start);
    const float origin = bipolar ? 0.f : start;
    juce::Path active; active.addCentredArc(cx, cy, r, r, 0, juce::jmin(origin, angle), juce::jmax(origin, angle), true);
    g.setColour(accent); g.strokePath(active, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(glassShadow()); g.fillEllipse(face.translated(0, 1.5f));
    g.setGradientFill(juce::ColourGradient(dark ? juce::Colour(0xff3c414a) : juce::Colour(0xfffcfdfe), face.getX(), face.getY(),
        dark ? juce::Colour(0xff2a2f37) : juce::Colour(0xffe6ecef), face.getRight(), face.getBottom(), false));
    g.fillEllipse(face);
    g.setColour(hovered ? accent : glassEdge().withAlpha(dark ? .7f : .5f)); g.drawEllipse(face, .9f);
    juce::Path shine; shine.addCentredArc(cx, cy, r - 10, r - 10, 0, -1.1f, .95f, true);
    g.setColour(glassEdge().withAlpha(.65f)); g.strokePath(shine, juce::PathStrokeType(.7f));
    g.setColour(accent);
    g.drawLine(cx + (r - 22) * std::sin(angle), cy - (r - 22) * std::cos(angle),
               cx + (r - 12) * std::sin(angle), cy - (r - 12) * std::cos(angle), 2.f);
    g.setColour(textLight()); g.setFont(juce::FontOptions("Avenir Next", "Demi Bold", 15.f));
    g.drawText(title, b.withHeight(20), juce::Justification::centred);
    g.setColour(textWhite()); g.setFont(juce::FontOptions("Avenir Next", "Medium", diameter >= 88 ? 24.f : 21.f));
    g.drawText(value, juce::Rectangle<float>(b.getX(), cy + r + 12, b.getWidth(), 25), juce::Justification::centred);
    juce::ignoreUnused(hasAuto);
}

void SanMuCompLookAndFeel::drawFetLargeDial(juce::Graphics& g,
                                            juce::Rectangle<float> bounds,
                                            float sliderPos,
                                            const juce::String& title,
                                            const juce::String& valueText,
                                            bool isInput,
                                            bool isLinked,
                                            bool hovered)
{
    juce::ignoreUnused(isInput);
    const bool dark = getTheme() == SanMuCompTheme::Dark;
    const auto fetModeCol = getModeColour(2, dark);
    const auto arcColour = isLinked ? accentPink() : fetModeCol;
    drawCompDial(g, bounds, sliderPos, title, valueText, arcColour, false, hovered);
}

void SanMuCompLookAndFeel::drawFetRatioButton(juce::Graphics& g,
                                              juce::Rectangle<float> bounds,
                                              int ratioIndex,
                                              bool isHighlighted,
                                              bool isDown)
{
    const float radius = juce::jmin(bounds.getWidth() / 2.0f, bounds.getHeight() / 2.0f) - 3.0f;
    const float centreX = bounds.getCentreX();
    const float centreY = bounds.getCentreY() + (isDown ? 0.8f : 0.0f);
    const float rx = centreX - radius;
    const float ry = centreY - radius;
    const float rw = radius * 2.0f;

    const bool dark = getTheme() == SanMuCompTheme::Dark;
    const int currentIdx = juce::jlimit(0, 4, ratioIndex);

    // 5 档鲜明透亮珊瑚粉/红指示灯（与中央 GR 压缩表同色系）：
    // 4:1 -> 8:1 -> 12:1 -> 20:1 -> ALL
    static const juce::Colour kLampColours[5] = {
        juce::Colour(0xffff8a8a), // 4:1: 柔和珊瑚粉
        juce::Colour(0xffff7875), // 8:1: 原版经典珊瑚粉/红
        juce::Colour(0xffff5277), // 12:1: 鲜明透亮玫红
        juce::Colour(0xffe11d48), // 20:1: 浓郁深珊瑚红
        juce::Colour(0xffff0055)  // ALL: 强烈宝石全红
    };
    static const float kGlowAlphas[5] = { 0.45f, 0.58f, 0.70f, 0.82f, 0.96f };

    const auto lampColour = kLampColours[currentIdx];
    const float glowAlpha = kGlowAlphas[currentIdx] * (isHighlighted ? 1.15f : 1.0f);

    // ---- 1. 白色/暗色通透玻璃圆盘基底 ----
    {
        juce::ColourGradient face(
            dark ? juce::Colour(0xcc2a323c) : juce::Colours::white.withAlpha(0.95f),
            centreX, centreY - radius * 0.4f,
            dark ? juce::Colour(0xd91b2129) : juce::Colour(0xffe4eae8),
            centreX, centreY + radius,
            true);
        g.setGradientFill(face);
        g.fillEllipse(rx, ry, rw, rw);

        // ---- 2. 明显生动的宝石灯光（双层辐射透光） ----
        // 外层柔和漫射光
        juce::ColourGradient outerGlow(
            lampColour.withAlpha(glowAlpha * 0.55f),
            centreX, centreY,
            lampColour.withAlpha(0.0f),
            centreX, centreY + radius,
            true);
        g.setGradientFill(outerGlow);
        g.fillEllipse(rx, ry, rw, rw);

        // 核心明亮聚光灯
        juce::ColourGradient coreLamp(
            lampColour.withAlpha(glowAlpha),
            centreX, centreY - 2.0f,
            lampColour.withAlpha(0.05f),
            centreX, centreY + radius * 0.75f,
            true);
        g.setGradientFill(coreLamp);
        g.fillEllipse(rx + 3.0f, ry + 3.0f, rw - 6.0f, rw - 6.0f);

        // ALL 模式专属：强烈呼吸发光能量双环
        if (currentIdx == 4)
        {
            g.setColour(lampColour.withAlpha(isHighlighted ? 0.85f : 0.65f));
            g.drawEllipse(rx + 2.0f, ry + 2.0f, rw - 4.0f, rw - 4.0f, 2.0f);
            g.setColour(juce::Colours::white.withAlpha(0.60f));
            g.drawEllipse(rx + 3.5f, ry + 3.5f, rw - 7.0f, rw - 7.0f, 1.0f);
        }

        // 1px 发丝边框（染上指示灯鲜明色彩）
        g.setColour(lampColour.withAlpha(isHighlighted ? 0.80f : 0.45f));
        g.drawEllipse(rx, ry, rw, rw, 1.0f);
    }

    // ---- 3. 圆圈内主次文字 ----
    static const char* kRatioLabels[] = { "4:1", "8:1", "12:1", "20:1", "ALL" };
    const juce::String ratioText = kRatioLabels[currentIdx];

    // 主：RATIO
    g.setFont(juce::FontOptions("Avenir Next", "Demi Bold", 10.5f));
    g.setColour(textLight().withAlpha(isHighlighted ? 0.98f : 0.88f));
    g.drawText("RATIO",
               juce::Rectangle<float>(centreX - radius, centreY - 12.0f, radius * 2.0f, 13.0f),
               juce::Justification::centred, false);

    // 次：当前比值（字体鲜亮发光）
    g.setFont(juce::FontOptions("Avenir Next", "Bold", 10.5f));
    const auto valueTextColour = dark ? lampColour.brighter(0.45f) : lampColour.darker(0.15f);
    g.setColour(valueTextColour);
    g.drawText(ratioText,
               juce::Rectangle<float>(centreX - radius, centreY + 2.0f, radius * 2.0f, 13.0f),
               juce::Justification::centred, false);
}

void SanMuCompLookAndFeel::drawFetRatioDial(juce::Graphics& g,
                                            juce::Rectangle<float> bounds,
                                            float /*sliderPos*/,
                                            int ratioIndex,
                                            bool hovered)
{
    drawFetRatioButton(g, bounds, ratioIndex, hovered, false);
}

void SanMuCompLookAndFeel::drawFetScButton(juce::Graphics& g,
                                           juce::Rectangle<float> bounds,
                                           int scIndex,
                                           bool isHighlighted,
                                           bool isDown)
{
    const float radius = juce::jmin(bounds.getWidth() / 2.0f, bounds.getHeight() / 2.0f) - 3.0f;
    const float centreX = bounds.getCentreX();
    const float centreY = bounds.getCentreY() + (isDown ? 0.8f : 0.0f);
    const float rx = centreX - radius;
    const float ry = centreY - radius;
    const float rw = radius * 2.0f;

    const bool dark = getTheme() == SanMuCompTheme::Dark;
    const int currentIdx = juce::jlimit(0, 4, scIndex);

    // 5 档鲜明透亮珊瑚粉/红指示灯（与 RATIO 压碎按键及 GR 表同色系）：
    // OFF -> 60Hz -> 90Hz -> 150Hz -> 250Hz
    static const juce::Colour kScColours[5] = {
        juce::Colour(0xffff7875), // OFF: 柔和雅致珊瑚粉微光
        juce::Colour(0xffff8a8a), // 60Hz: 柔和珊瑚粉
        juce::Colour(0xffff7875), // 90Hz: 原版经典珊瑚粉/红
        juce::Colour(0xffff5277), // 150Hz: 鲜明透亮玫红
        juce::Colour(0xffff0055)  // 250Hz: 极光璀璨宝石红
    };
    static const float kScAlphas[5] = { 0.26f, 0.50f, 0.65f, 0.78f, 0.95f };

    const auto lampColour = kScColours[currentIdx];
    const float glowAlpha = kScAlphas[currentIdx] * (isHighlighted ? 1.15f : 1.0f);

    // ---- 1. 白色/暗色通透玻璃圆盘基底 ----
    {
        juce::ColourGradient face(
            dark ? juce::Colour(0xcc2a323c) : juce::Colours::white.withAlpha(0.95f),
            centreX, centreY - radius * 0.4f,
            dark ? juce::Colour(0xd91b2129) : juce::Colour(0xffe4eae8),
            centreX, centreY + radius,
            true);
        g.setGradientFill(face);
        g.fillEllipse(rx, ry, rw, rw);

        // ---- 2. 内部柔和透出的宝石灯光（OFF 档亦有温润的珊瑚粉基调） ----
        // 外层柔和漫射光
        juce::ColourGradient outerGlow(
            lampColour.withAlpha(glowAlpha * 0.55f),
            centreX, centreY,
            lampColour.withAlpha(0.0f),
            centreX, centreY + radius,
            true);
        g.setGradientFill(outerGlow);
        g.fillEllipse(rx, ry, rw, rw);

        // 核心聚光灯
        juce::ColourGradient coreLamp(
            lampColour.withAlpha(glowAlpha),
            centreX, centreY - 2.0f,
            lampColour.withAlpha(0.04f),
            centreX, centreY + radius * 0.75f,
            true);
        g.setGradientFill(coreLamp);
        g.fillEllipse(rx + 3.0f, ry + 3.0f, rw - 6.0f, rw - 6.0f);

        // 250Hz 最高档专属：强烈发光能量环
        if (currentIdx == 4)
        {
            g.setColour(lampColour.withAlpha(isHighlighted ? 0.85f : 0.65f));
            g.drawEllipse(rx + 2.0f, ry + 2.0f, rw - 4.0f, rw - 4.0f, 2.0f);
            g.setColour(juce::Colours::white.withAlpha(0.60f));
            g.drawEllipse(rx + 3.5f, ry + 3.5f, rw - 7.0f, rw - 7.0f, 1.0f);
        }

        // 1px 发丝边框（染上指示灯鲜明色彩）
        g.setColour(lampColour.withAlpha(currentIdx > 0 ? (isHighlighted ? 0.80f : 0.45f)
                                                        : (isHighlighted ? 0.40f : 0.22f)));
        g.drawEllipse(rx, ry, rw, rw, 1.0f);
    }

    // ---- 3. 圆圈内主次文字 ----
    static const char* kScLabels[] = { "OFF", "60 Hz", "90 Hz", "150 Hz", "250 Hz" };
    const juce::String scText = kScLabels[currentIdx];

    // 主：SC
    g.setFont(juce::FontOptions("Avenir Next", "Demi Bold", 10.5f));
    g.setColour(textLight().withAlpha(isHighlighted ? 0.98f : 0.88f));
    g.drawText("SC",
               juce::Rectangle<float>(centreX - radius, centreY - 12.0f, radius * 2.0f, 13.0f),
               juce::Justification::centred, false);

    // 次：当前档位（字体鲜亮发光，OFF 呈现优雅透粉）
    g.setFont(juce::FontOptions("Avenir Next", "Bold", 10.0f));
    const auto valueTextColour = (currentIdx == 0)
        ? (dark ? juce::Colour(0xffffa39e).withAlpha(0.85f) : juce::Colour(0xffcf1322).withAlpha(0.85f))
        : (dark ? lampColour.brighter(0.45f) : lampColour.darker(0.15f));
    g.setColour(valueTextColour);
    g.drawText(scText,
               juce::Rectangle<float>(centreX - radius, centreY + 2.0f, radius * 2.0f, 13.0f),
               juce::Justification::centred, false);
}

void SanMuCompLookAndFeel::drawFetMediumDial(juce::Graphics& g,
                                             juce::Rectangle<float> bounds,
                                             float sliderPos,
                                             const juce::String& title,
                                             const juce::String& valueText,
                                             bool hovered)
{
    const bool dark = getTheme() == SanMuCompTheme::Dark;
    const auto arcColour = getModeColour(2, dark);
    drawCompDial(g, bounds, sliderPos, title, valueText, arcColour, false, hovered);
}

void SanMuCompLookAndFeel::drawFetSmallDial(juce::Graphics& g,
                                            juce::Rectangle<float> bounds,
                                            float sliderPos,
                                            const juce::String& title,
                                            const juce::String& valueText,
                                            bool isBipolar,
                                            bool hovered)
{
    const bool dark = getTheme() == SanMuCompTheme::Dark;
    const auto arcColour = getModeColour(2, dark);
    drawCompDial(g, bounds, sliderPos, title, valueText, arcColour, isBipolar, hovered);
}

void SanMuCompLookAndFeel::drawFetCircleButton(juce::Graphics& g,
                                                juce::Button& button,
                                                const juce::String& title,
                                                bool isHighlighted,
                                                bool isDown)
{
    const auto b = button.getLocalBounds().toFloat().reduced(2.0f);
    const float radius = juce::jmin(b.getWidth(), b.getHeight()) * 0.5f;
    const float centreX = b.getCentreX();
    const float centreY = b.getCentreY();
    const bool active = button.getToggleState() || isDown;
    const bool dark = getTheme() == SanMuCompTheme::Dark;
    const juce::Colour activeColour = fetBlueStripe();

    const float rx = centreX - radius;
    const float ry = centreY - radius;
    const float rw = radius * 2.0f;

    // ---- 1. 白色/暗色玻璃底面 ----
    if (active)
    {
        juce::ColourGradient activeGrad(
            activeColour, centreX, centreY - radius * 0.4f,
            dark ? juce::Colour(0xFF0369A1) : juce::Colour(0xFF0284C7), centreX, centreY + radius,
            true);
        g.setGradientFill(activeGrad);
        g.fillEllipse(rx, ry, rw, rw);

        g.setColour(juce::Colours::white.withAlpha(0.6f));
        g.drawEllipse(rx + 0.5f, ry + 0.5f, rw - 1.0f, rw - 1.0f, 1.0f);
    }
    else
    {
        juce::ColourGradient face(
            dark ? juce::Colour(0xcc2a323c) : juce::Colours::white.withAlpha(0.95f),
            centreX, centreY - radius * 0.4f,
            dark ? juce::Colour(0xd91b2129) : juce::Colour(0xffe4eae8),
            centreX, centreY + radius,
            true);
        g.setGradientFill(face);
        g.fillEllipse(rx, ry, rw, rw);

        g.setColour(isHighlighted ? activeColour.withAlpha(0.40f) : glassEdge().withAlpha(0.18f));
        g.drawEllipse(rx + 0.5f, ry + 0.5f, rw - 1.0f, rw - 1.0f, 1.0f);
    }

    // ---- 2. 铭牌文字 ----
    g.setFont(juce::FontOptions("Avenir Next", "Demi Bold", 9.0f));
    g.setColour(active ? juce::Colours::white : textLight());
    g.drawText(title,
               juce::Rectangle<float>(centreX - radius, centreY - 7.0f, radius * 2.0f, 14.0f),
               juce::Justification::centred, false);
}

void SanMuCompLookAndFeel::drawLinearSlider(juce::Graphics& g,
                                          int x, int y, int width, int height,
                                          float sliderPos,
                                          float minSliderPos,
                                          float maxSliderPos,
                                          const juce::Slider::SliderStyle /*style*/,
                                          juce::Slider& slider)
{
    setTheme(theme);
    juce::ignoreUnused(minSliderPos, maxSliderPos);
    const auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                               static_cast<float>(width), static_cast<float>(height));
    const bool dark = getTheme() == SanMuCompTheme::Dark;
    const bool hovered = slider.isMouseOverOrDragging();
    const juce::String title = slider.getProperties()["dialTitle"].toString();
    const juce::String suffix = slider.getProperties()["suffix"].toString();
    const float val = static_cast<float>(slider.getValue());
    const auto accentCol = slider.findColour(juce::Slider::rotarySliderFillColourId);

    // 格式化实时数值文本
    juce::String valText;
    if (title == "GATE" && val <= -79.5f)
        valText = "OFF";
    else if (title == "COMP" || title == "GAIN" || title == "GATE")
        valText = juce::String(val, 1) + " dB";
    else if (title == "MIX")
        valText = juce::String(juce::roundToInt(val)) + " %";
    else
        valText = juce::String(val, 1) + suffix;

    const float cx = bounds.getCentreX();
    const float topY = bounds.getY() + 30.0f;
    const float btmY = bounds.getBottom() - 32.0f;
    const float trackH = btmY - topY;
    const float trackW = (title == "COMP") ? 20.0f : 16.0f;
    const float trackX = cx - trackW * 0.5f;

    // 1. 顶部标题
    const auto titleCol = dark ? juce::Colour(0xFFE2E8F0) : juce::Colour(0xFF334155);
    g.setFont(juce::FontOptions("Avenir Next", "Demi Bold", (title == "COMP" ? 17.0f : 16.0f)));
    g.setColour(hovered ? (dark ? juce::Colours::white : juce::Colour(0xFF0F172A)) : titleCol);
    g.drawText(title, bounds.withHeight(20.0f), juce::Justification::centred, false);

    // 2. 底部数值

    g.setFont(juce::FontOptions("Avenir Next", "Bold", (title == "COMP" ? 24.0f : 22.0f)));
    g.setColour(textWhite());
    g.drawText(valText, juce::Rectangle<float>(bounds.getX(), btmY + 2.0f, bounds.getWidth(), 20.0f), juce::Justification::centred, false);

    // 3. 垂直滑道凹槽（深色磨砂内凹晶体槽）
    const auto trackRect = juce::Rectangle<float>(trackX, topY, trackW, trackH);
    g.setColour(dark ? juce::Colour(0x6010151c) : juce::Colour(0x30c8d4d8));
    g.fillRoundedRectangle(trackRect, trackW * 0.5f);

    // 4. 侧边微光刻度点 (Tick Marks)
    const int numTicks = 9;
    for (int i = 0; i < numTicks; ++i)
    {
        const float norm = static_cast<float>(i) / static_cast<float>(numTicks - 1);
        const float ty = topY + norm * trackH;
        g.setColour(glassEdge().withAlpha(dark ? 0.35f : 0.22f));
        g.drawLine(trackX - 5.0f, ty, trackX - 2.0f, ty, 1.0f);
        g.drawLine(trackRect.getRight() + 2.0f, ty, trackRect.getRight() + 5.0f, ty, 1.0f);
    }

    // 5. 动态能量流光填充
    const float curThumbY = juce::jlimit(topY, btmY, sliderPos);
    if (title == "COMP")
    {
        // COMP 是向下推入平准，从顶部 topY 到 curThumbY 填充
        const float fillH = curThumbY - topY;
        if (fillH > 1.0f)
        {
            auto fillRect = juce::Rectangle<float>(trackX + 2.0f, topY + 2.0f, trackW - 4.0f, fillH);
            juce::ColourGradient grad(accentCol.withAlpha(0.30f), cx, topY,
                                      accentCol.withAlpha(0.70f), cx, curThumbY, false);
            g.setGradientFill(grad);
            g.fillRoundedRectangle(fillRect, (trackW - 4.0f) * 0.5f);
        }
    }
    else
    {
        // 其它推子从底部 btmY 向上填充到 curThumbY
        const float fillH = btmY - curThumbY;
        if (fillH > 1.0f)
        {
            auto fillRect = juce::Rectangle<float>(trackX + 2.0f, curThumbY, trackW - 4.0f, fillH);
            juce::ColourGradient grad(accentCol.withAlpha(0.70f), cx, curThumbY,
                                      accentCol.withAlpha(0.30f), cx, btmY, false);
            g.setGradientFill(grad);
            g.fillRoundedRectangle(fillRect, (trackW - 4.0f) * 0.5f);
        }
    }

    // 6. 滑道发丝边框
    g.setColour(hovered ? accentCol.withAlpha(0.40f) : glassEdge().withAlpha(0.20f));
    g.drawRoundedRectangle(trackRect, trackW * 0.5f, 1.0f);

    // 7. 纯粹悬浮水晶推子把手 (Glass Fader Cap)
    const float capW = (title == "COMP") ? 38.0f : 32.0f;
    const float capH = 14.0f;
    const auto capRect = juce::Rectangle<float>(cx - capW * 0.5f, curThumbY - capH * 0.5f, capW, capH);

    // 把手投影
    g.setColour(juce::Colour(0x35000000));
    g.fillRoundedRectangle(capRect.translated(0.0f, 2.0f), 3.5f);

    // 把手玻璃本体
    juce::ColourGradient capFace(
        dark ? juce::Colour(0xFF334155) : juce::Colours::white.withAlpha(0.98f),
        cx, capRect.getY(),
        dark ? juce::Colour(0xFF1E293B) : juce::Colour(0xFFE2E8F0),
        cx, capRect.getBottom(), false);
    g.setGradientFill(capFace);
    g.fillRoundedRectangle(capRect, 3.5f);

    // 把手中央防滑刻线
    g.setColour(accentCol.withAlpha(hovered ? 0.95f : 0.75f));
    g.fillRect(cx - capW * 0.35f, curThumbY - 0.75f, capW * 0.7f, 1.5f);

    // 把手 1px 发丝边框
    g.setColour(hovered ? accentCol : glassEdge().withAlpha(0.35f));
    g.drawRoundedRectangle(capRect, 3.5f, 1.0f);
}

void SanMuCompLookAndFeel::drawRotarySlider(juce::Graphics& g,
                                          int x, int y, int width, int height,
                                          float sliderPos,
                                          float /*rotaryStartAngle*/, float /*rotaryEndAngle*/,
                                          juce::Slider& slider)
{
    setTheme(theme);
    const juce::String title = slider.getProperties()["dialTitle"].toString();
    const juce::String suffix = slider.getProperties()["suffix"].toString();
    const bool isBipolar = (bool) slider.getProperties()["isBipolar"];
    const bool isLinked = (bool) slider.getProperties()["isLinked"];
    const float val = static_cast<float>(slider.getValue());
    const bool hovered = slider.isMouseOverOrDragging();

    juce::Colour arcColour = slider.findColour(juce::Slider::rotarySliderFillColourId);
    if (isLinked)
        arcColour = accentPink();

    // 格式化实时数值
    juce::String valText;
    float actualPos = sliderPos;
    if (title == "INPUT" || title == "OUTPUT")
    {
        if (sliderPos <= 0.002f)
        {
            valText = juce::String::charToString(0x221E); // "∞"
        }
        else
        {
            const float atten = (1.0f - sliderPos) * 48.0f;
            if (atten >= 47.95f)
                valText = juce::String::charToString(0x221E);
            else if (atten <= 0.05f)
                valText = "0.0";
            else
                valText = juce::String(atten, 1);
        }
    }
    else if (title == "RATIO")
    {
        if (slider.getProperties()["isFetRatioChoice"] || slider.getProperties()["isFetRatioDial"])
        {
            const int ratioIdx = juce::jlimit(0, 4, static_cast<int>(std::lround(val)));
            static const char* kRatioLabels[] = { "4:1", "8:1", "12:1", "20:1", "ALL" };
            valText = kRatioLabels[ratioIdx];
            actualPos = static_cast<float>(ratioIdx) / 4.0f;
        }
        else if (slider.getProperties()["isVcaRatio"])
        {
            if (val <= 3.0f)
            {
                valText = "2 : 1";
                actualPos = 0.0f;
            }
            else if (val <= 7.0f)
            {
                valText = "4 : 1";
                actualPos = 0.5f;
            }
            else
            {
                valText = "10 : 1";
                actualPos = 1.0f;
            }
        }
        else
        {
            valText = juce::String(val, 1) + " : 1";
        }
    }
    else if (title == "SC" || title == "SC FILTER")
    {
        if (slider.getProperties()["isFetScChoice"])
        {
            static const float kScSteps[5] = { 0.0f, 60.0f, 90.0f, 150.0f, 250.0f };
            int scIdx = 0;
            float minDiff = 1e9f;
            for (int i = 0; i < 5; ++i)
            {
                const float diff = std::abs(val - kScSteps[i]);
                if (diff < minDiff) { minDiff = diff; scIdx = i; }
            }
            if (scIdx == 0)
                valText = "OFF";
            else
                valText = juce::String(juce::roundToInt(kScSteps[scIdx])) + " Hz";
            actualPos = static_cast<float>(scIdx) / 4.0f;
        }
        else
        {
            if (val <= 1.0f)
            {
                valText = "OFF";
                actualPos = 0.0f;
            }
            else
            {
                valText = juce::String(juce::roundToInt(val)) + " Hz";
                actualPos = sliderPos;
            }
        }
    }
    else if (title == "ATTACK")
    {
        if (slider.getProperties()["isFetTimeDial"])
            valText = juce::String(val, 1);
        else
            valText = (val < 10.0f ? juce::String(val, 1) : juce::String(juce::roundToInt(val))) + " ms";
    }
    else if (title == "RELEASE")
    {
        if (slider.getProperties()["isFetTimeDial"])
            valText = juce::String(val, 1);
        else if (val >= 1200.0f)
            valText = "AUTO";
        else
            valText = juce::String(juce::roundToInt(val)) + " ms";
    }
    else if (title == "GATE")
    {
        if (val <= -79.0f)
            valText = "OFF";
        else
            valText = juce::String(val, 1) + " dB";
    }
    else if (title == "COMP")
    {
        if (std::abs(val) < 0.05f)
            valText = "0.0 dB";
        else
            valText = juce::String(val, 1) + " dB";
    }
    else if (title == "GAIN")
    {
        if (slider.getProperties()["isVoxGain"])
            valText = juce::String(val, 1) + " dB";
        else
            valText = juce::String(val, 1);
    }
    else if (title == "PEAK RED")
    {
        valText = juce::String(val, 1);
    }
    else if (title == "HR" || title == "TRIM" || title == "MAKEUP")
    {
        valText = (val > 0.05f ? "+" : "") + juce::String(val, 1) + " dB";
    }
    else if (title == "MIX")
    {
        valText = juce::String(juce::roundToInt(val)) + " %";
    }
    else if (title == "THRESHOLD" || title == "THRESH" || title == "KNEE")
    {
        valText = juce::String(val, 1) + " dB";
    }
    else
    {
        valText = juce::String(val, 1) + suffix;
    }

    const bool hasAutoCapsule = (bool) slider.getProperties()["hasAutoCapsule"];

    drawCompDial(g,
                 juce::Rectangle<float>(static_cast<float>(x),
                                        static_cast<float>(y),
                                        static_cast<float>(width),
                                        static_cast<float>(height)),
                 actualPos, title, valText, arcColour, isBipolar, hovered, hasAutoCapsule);
}

// ==============================================================================
// 旋钮下方的数值标签 - 显示在旋钮正下方
// ==============================================================================

juce::Label* SanMuCompLookAndFeel::createSliderTextBox(juce::Slider& /*slider*/)
{
    auto* label = new juce::Label();
    label->setJustificationType(juce::Justification::centred);
    label->setColour(juce::Label::textColourId, textLight());
    label->setFont(juce::FontOptions(10.0f));
    label->setInterceptsMouseClicks(false, false);
    return label;
}

// ==============================================================================
// ComboBox 样式 - 玻璃下拉选择器
// ==============================================================================

void SanMuCompLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height,
                                      bool /*isButtonDown*/,
                                      int /*buttonX*/, int /*buttonY*/,
                                      int /*buttonW*/, int /*buttonH*/,
                                      juce::ComboBox& box)
{
    juce::Rectangle<int> boxBounds(0, 0, width, height);

    // 第四十轮：hover 状态来自 editor 定时轮询（properties），
    // 避免 JUCE isMouseOver 在 REAPER Retina 下断触闪烁
    const bool hovered = box.isMouseOverOrDragging()
                         || (bool) box.getProperties()["hoverState"];
    if (hovered)
    {
        // 水印框紧贴文字：高/底距按控件高度等比（18/24、5/24），随缩放一致
        const juce::Font f(juce::FontOptions(13.0f));
        const float textW = juce::GlyphArrangement::getStringWidth(f, box.getText());
        const float frameH = height * 18.0f / 24.0f;
        const juce::Rectangle<float> frame(
            (width  - (textW + 14.0f)) * 0.5f,
            height - frameH - height * 5.0f / 24.0f,
            textW + 14.0f, frameH);
        g.setColour(juce::Colour(0x52DDE5EB));
        g.fillRoundedRectangle(frame, 4.0f);
    }
}

void SanMuCompLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    // 第四十轮：无箭头；文字真实基线 = 下拉底 - 7/24 下拉高（等比缩放），与整排下划线对齐
    const juce::Font f(juce::FontOptions(13.0f));
    label.setBounds(4, 0, box.getWidth() - 8,
                    juce::roundToInt(box.getHeight() * 17.0f / 24.0f + f.getDescent()));
    label.setJustificationType(juce::Justification::centredBottom);
    label.setFont(juce::FontOptions(13.0f));
}

// ==============================================================================
// 栓连标志绘制（Input 与 Output 之间的联动桥）
// ==============================================================================

void SanMuCompLookAndFeel::drawLinkBridge(juce::Graphics& g,
                                          juce::Rectangle<float> bounds,
                                          bool isLinked,
                                          bool isHovered)
{
    const float cx = bounds.getCentreX();
    const float cy = bounds.getCentreY();
    const bool dark = getTheme() == SanMuCompTheme::Dark;

    const auto linkCol   = accentPink();

    const float lineY = cy;
    const float ringW = 13.0f;
    const float ringH = 9.0f;
    const float ringR = 3.5f;

    if (isLinked)
    {
        // 激活联动状态：纯净珊瑚粉/红细线（1.4px 清爽细线，与未激活细轨完全等粗，无加粗光晕，无白色高光点）
        g.setColour(linkCol);
        g.drawLine(bounds.getX() + 2.0f, lineY, cx - 11.0f, lineY, 1.4f);
        g.drawLine(cx + 11.0f, lineY, bounds.getRight() - 2.0f, lineY, 1.4f);

        juce::Path linkP;
        linkP.addRoundedRectangle(cx - 9.0f, cy - ringH * 0.5f, ringW, ringH, ringR);
        linkP.addRoundedRectangle(cx - 4.0f, cy - ringH * 0.5f, ringW, ringH, ringR);
        g.strokePath(linkP, juce::PathStrokeType(1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    else
    {
        // 未激活：1.4px 哑光细轨与暗调双环，与旋钮底轨完全对齐
        g.setColour(glassEdge().withAlpha(isHovered ? (dark ? 0.45f : 0.35f) : (dark ? 0.25f : 0.18f)));
        g.drawLine(bounds.getX() + 2.0f, lineY, cx - 11.0f, lineY, 1.4f);
        g.drawLine(cx + 11.0f, lineY, bounds.getRight() - 2.0f, lineY, 1.4f);

        juce::Path leftRing;
        leftRing.addRoundedRectangle(cx - 10.0f, cy - ringH * 0.5f, ringW, ringH, ringR);
        juce::Path rightRing;
        rightRing.addRoundedRectangle(cx - 3.0f,  cy - ringH * 0.5f, ringW, ringH, ringR);

        g.strokePath(leftRing,  juce::PathStrokeType(1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.strokePath(rightRing, juce::PathStrokeType(1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

// ==============================================================================
// 按钮样式 - 玻璃按钮
// ==============================================================================

void SanMuCompLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                              const juce::Colour& /*backgroundColour*/,
                                              bool shouldDrawButtonAsHighlighted,
                                              bool shouldDrawButtonAsDown)
{
    if (button.getComponentID().startsWith("history_") || button.getComponentID() == "meter_clear")
    {
        setTheme(theme);
        auto b = button.getLocalBounds().toFloat().reduced(.5f);
        const bool active = button.getToggleState();
        g.setColour((active ? textWhite() : glassEdge()).withAlpha(active ? .07f : .035f));
        g.fillRoundedRectangle(b, 7);
        g.setColour(button.hasKeyboardFocus(true) ? textWhite() : glassEdge().withAlpha(shouldDrawButtonAsHighlighted ? .65f : .3f));
        g.drawRoundedRectangle(b, 7, 1);
        return;
    }

    setTheme(theme);
    if ((bool) button.getProperties()["isLinkBridge"])
    {
        const bool isLinked = button.getToggleState();
        const bool isHovered = shouldDrawButtonAsHighlighted || button.isMouseOver() || (bool) button.getProperties()["hoverState"];
        drawLinkBridge(g, button.getLocalBounds().toFloat(), isLinked, isHovered);
        return;
    }

    if ((bool) button.getProperties()["isCircleButton"])
    {
        const juce::String title = button.getProperties()["buttonTitle"].toString();
        drawFetCircleButton(g, button, title, shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);
        return;
    }

    if ((bool) button.getProperties()["isTopBarButton"])
    {
        const auto b = button.getLocalBounds().toFloat().reduced(0.5f);
        const bool active = button.getToggleState() || shouldDrawButtonAsDown;
        const bool hovered = shouldDrawButtonAsHighlighted || button.isMouseOver() || (bool) button.getProperties()["hoverState"];
        const bool dark = getTheme() == SanMuCompTheme::Dark;

        // 玻璃胶囊背景 (圆角 6px)
        juce::Colour bgCol;
        if (active && !(bool) button.getProperties()["isGearButton"])
        {
            bgCol = dark ? juce::Colour(0x66E11D48) : juce::Colour(0x33E11D48);
        }
        else
        {
            bgCol = dark
                ? (hovered ? juce::Colour(0x3DFFFFFF) : juce::Colour(0x1AFFFFFF))
                : (hovered ? juce::Colour(0xCCFFFFFF) : juce::Colour(0x80FFFFFF));
        }

        g.setColour(bgCol);
        g.fillRoundedRectangle(b, 6.0f);

        // 1px 发丝边框
        juce::Colour borderCol;
        if (active && !(bool) button.getProperties()["isGearButton"])
            borderCol = accentPink();
        else
            borderCol = hovered ? glassEdge().withAlpha(dark ? 0.60f : 0.45f) : glassEdge().withAlpha(dark ? 0.30f : 0.20f);

        g.setColour(borderCol);
        g.drawRoundedRectangle(b, 6.0f, 1.0f);

        // 如果是齿轮按钮，绘制精致齿轮矢量图标
        if ((bool) button.getProperties()["isGearButton"])
        {
            const float cx = b.getCentreX();
            const float cy = b.getCentreY();
            const float rOuter = 5.8f;
            const float rInner = 4.3f;
            const float rHole  = 2.2f;
            constexpr int numTeeth = 6;

            juce::Path p;
            for (int i = 0; i < numTeeth * 2; ++i)
            {
                const float a = static_cast<float>(i) * juce::MathConstants<float>::twoPi / static_cast<float>(numTeeth * 2);
                const float r = (i % 2 == 0) ? rOuter : rInner;
                const float x = cx + r * std::cos(a);
                const float y = cy + r * std::sin(a);
                if (i == 0) p.startNewSubPath(x, y);
                else        p.lineTo(x, y);
            }
            p.closeSubPath();

            const juce::Colour iconCol = hovered ? textWhite() : textLight();
            g.setColour(iconCol);
            g.strokePath(p, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.drawEllipse(cx - rHole, cy - rHole, rHole * 2.0f, rHole * 2.0f, 1.3f);
        }
        return;
    }

    if ((bool) button.getProperties()["isOptoModeButton"])
    {
        const bool isLimit = button.getToggleState() || shouldDrawButtonAsDown;
        const bool hovered = shouldDrawButtonAsHighlighted || button.isMouseOver();
        const auto bounds = button.getLocalBounds().toFloat();
        const bool dark = (getTheme() == SanMuCompTheme::Dark);

        // 1. 磨砂质感底座
        juce::ColourGradient bg(
            dark ? juce::Colour(0xcc242b35) : juce::Colour(0xffe2e8ec),
            bounds.getX(), bounds.getY(),
            dark ? juce::Colour(0xd9181e26) : juce::Colour(0xffcfd8df),
            bounds.getX(), bounds.getBottom(),
            false);
        g.setGradientFill(bg);
        g.fillRoundedRectangle(bounds, bounds.getHeight() * 0.5f);

        // 2. 发丝光描边
        const juce::Colour strokeCol = isLimit ? accentPink().withAlpha(hovered ? 0.90f : 0.60f)
                                               : (dark ? juce::Colour(0x40ffffff) : juce::Colour(0x608090a0));
        g.setColour(strokeCol);
        g.drawRoundedRectangle(bounds.reduced(0.5f), bounds.getHeight() * 0.5f, 1.0f);

        // 3. 模式切换滑动滑块（左侧 COMP / 右侧 LIMIT）
        const float pillW = (bounds.getWidth() - 4.0f) * 0.5f;
        const float pillH = bounds.getHeight() - 4.0f;
        const float pillX = isLimit ? (bounds.getX() + bounds.getWidth() - pillW - 2.0f) : (bounds.getX() + 2.0f);
        const juce::Rectangle<float> activePill(pillX, bounds.getY() + 2.0f, pillW, pillH);

        juce::ColourGradient pillGrad(
            isLimit ? accentPink().withAlpha(0.95f) : (dark ? juce::Colour(0xff3a4654) : juce::Colours::white),
            activePill.getX(), activePill.getY(),
            isLimit ? accentPink().darker(0.2f) : (dark ? juce::Colour(0xff2a3440) : juce::Colour(0xffe0e6eb)),
            activePill.getX(), activePill.getBottom(),
            false);
        g.setGradientFill(pillGrad);
        g.fillRoundedRectangle(activePill, activePill.getHeight() * 0.5f);

        // 4. 文字（左 COMP, 右 LIMIT）
        const auto font = juce::FontOptions("Avenir Next", "Bold", 10.0f);
        g.setFont(font);

        const juce::Rectangle<float> compRect(bounds.getX() + 2.0f, bounds.getY(), pillW, bounds.getHeight());
        const juce::Rectangle<float> limitRect(bounds.getX() + bounds.getWidth() - pillW - 2.0f, bounds.getY(), pillW, bounds.getHeight());

        // COMP 文字
        g.setColour(!isLimit ? (dark ? juce::Colours::white : juce::Colour(0xff1a2228))
                             : (dark ? juce::Colour(0x80ffffff) : juce::Colour(0x80405060)));
        g.drawText("COMP", compRect, juce::Justification::centred, false);

        // LIMIT 文字
        g.setColour(isLimit ? juce::Colours::white
                            : (dark ? juce::Colour(0x80ffffff) : juce::Colour(0x80405060)));
        g.drawText("LIMIT", limitRect, juce::Justification::centred, false);

        return;
    }

    const bool active = button.getToggleState() || shouldDrawButtonAsDown;
    // 第四十轮：hover 状态来自 editor 定时轮询（properties），
    // 避免 JUCE isMouseOver 在 REAPER Retina 下断触闪烁
    const bool hovered = shouldDrawButtonAsHighlighted
                         || button.isMouseOver()
                         || (bool) button.getProperties()["hoverState"];

    if (active || hovered)
    {
        // 第四十轮：文字按钮（A/B）水印框紧贴文字（高/底距按按钮高等比）
        const auto font = getTextButtonFont(static_cast<juce::TextButton&>(button),
                                            button.getHeight());
        const float textW = juce::GlyphArrangement::getStringWidth(
            font, static_cast<juce::TextButton&>(button).getButtonText());
        const float frameH = button.getHeight() * 18.0f / 24.0f;
        const juce::Rectangle<float> frame(
            (button.getWidth()  - (textW + 12.0f)) * 0.5f,
            button.getHeight() - frameH - button.getHeight() * 5.0f / 24.0f,
            textW + 12.0f, frameH);
        // 激活/按下：灰白水印比 hover 深一档（点击后保持），无描边
        g.setColour(active ? juce::Colour(0x8CD9E2E9)
                           : juce::Colour(0x52DDE5EB));
        g.fillRoundedRectangle(frame, 4.0f);
    }
    // 默认（无 hover/激活）：不绘制任何背景，保持无框
}

juce::Font SanMuCompLookAndFeel::getTextButtonFont(juce::TextButton&, int /*buttonHeight*/)
{
    // 第四十轮：A/B 文字 17px，与 13x13 图标视觉等大（整体字号加大）
    return juce::FontOptions(17.0f);
}

juce::Font SanMuCompLookAndFeel::getComboBoxFont(juce::ComboBox& box)
{
    // 设置面板等紧凑下拉：14px；顶部预设/Model 等：17px
    if ((bool) box.getProperties()["compactText"])
        return juce::FontOptions("Helvetica Neue", "Light", 14.0f);
    return juce::FontOptions(17.0f);
}

juce::Font SanMuCompLookAndFeel::getPopupMenuFont()
{
    return juce::FontOptions("PingFang SC", 13.0f, juce::Font::plain);
}

void SanMuCompLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button,
                                        bool shouldDrawButtonAsHighlighted,
                                        bool shouldDrawButtonAsDown)
{
    if (button.getComponentID().startsWith("history_") || button.getComponentID() == "meter_clear")
    {
        setTheme(theme);
        g.setFont(juce::FontOptions("PingFang SC", 14.f, juce::Font::plain));
        g.setColour(button.findColour(button.getToggleState() ? juce::TextButton::textColourOnId : juce::TextButton::textColourOffId)
                    .withMultipliedAlpha(button.isEnabled() ? 1.f : .4f));
        g.drawText(button.getButtonText(), button.getLocalBounds(), juce::Justification::centred);
        return;
    }

    setTheme(theme);
    if ((bool) button.getProperties()["isLinkBridge"])
    {
        // 已经在 drawLinkBridge 中完整绘制几何图标
        return;
    }

    if ((bool) button.getProperties()["isCircleButton"])
    {
        // 已经在 drawFetCircleButton 中完整绘制
        return;
    }

    if ((bool) button.getProperties()["isOptoModeButton"])
    {
        // 已经在 drawButtonBackground 中完整绘制双档滑块与文字
        return;
    }

    if ((bool) button.getProperties()["isTopBarButton"])
    {
        if ((bool) button.getProperties()["isGearButton"])
            return; // 齿轮已在背景中完整绘制

        const bool active = button.getToggleState() || shouldDrawButtonAsDown;
        const bool hovered = shouldDrawButtonAsHighlighted || button.isMouseOver();
        const auto font = juce::FontOptions("Avenir Next", "Demi Bold", 12.0f);
        g.setFont(font);

        juce::Colour textCol;
        if (active)
            textCol = accentPink();
        else
            textCol = hovered ? textWhite() : textLight();

        g.setColour(textCol);
        g.drawText(button.getButtonText(), button.getLocalBounds(), juce::Justification::centred, false);
        return;
    }

    if ((bool) button.getProperties()["isCapsule"])
    {
        const bool active = button.getToggleState() || shouldDrawButtonAsDown;
        const auto font = juce::FontOptions("Avenir Next", active ? "Bold" : "Medium", 12.0f);
        g.setFont(font);
        g.setColour(active ? juce::Colours::white : (getTheme() == SanMuCompTheme::Dark ? juce::Colour(0xAAFFFFFF) : juce::Colour(0xAA25343D)));
        g.drawText(button.getButtonText(), button.getLocalBounds(), juce::Justification::centred, false);
        return;
    }

    // Model / 预设等"紧凑文字"按钮：更细更小，避免抢眼；
    // 中文按钮（帮助）用 PingFang SC 避免乱码
    juce::Font font = getTextButtonFont(button, button.getHeight());
    if ((bool) button.getProperties()["chineseText"])
        font = juce::FontOptions("PingFang SC", 14.0f, juce::Font::plain);
    else if ((bool) button.getProperties()["compactText"])
        font = juce::FontOptions("Helvetica Neue", "Light", 14.0f);
    g.setFont(font);

    // 激活 / 悬停：白字；默认：冷灰蓝字
    if (button.getToggleState() || shouldDrawButtonAsDown || shouldDrawButtonAsHighlighted)
        g.setColour(textWhite());
    else
        g.setColour(button.findColour(juce::TextButton::textColourOffId));

    // 第四十轮：真实基线绘制（基线 = 按钮底 - 7/24 按钮高，随缩放等比），
    // 避免 drawText 用字框底对齐导致视觉低 3~4px
    const float baselineY = button.getHeight() * 17.0f / 24.0f;
    juce::GlyphArrangement glyphs;
    glyphs.addLineOfText(font, button.getButtonText(), 0.0f, baselineY);
    const auto bb = glyphs.getBoundingBox(0, glyphs.getNumGlyphs(), true);
    glyphs.draw(g, juce::AffineTransform::translation(
        (button.getWidth() - bb.getWidth()) * 0.5f - bb.getX(), 0.0f));
}

// ==============================================================================
// 弹窗（下拉菜单）样式 - 玻璃面板
// ==============================================================================

void SanMuCompLookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int width, int height)
{
    const bool dark = theme == SanMuCompTheme::Dark;
    const auto b = juce::Rectangle<float>(0.0f, 0.0f,
                                          static_cast<float>(width),
                                          static_cast<float>(height));
    if (dark)
    {
        // Dark 模式：单层磨砂圆角深色玻璃底色（四周 100% 透明无毛刺无黑白角）
        g.setColour(juce::Colour(0xf2242c36));
        g.fillRoundedRectangle(b, 8.0f);

        g.setColour(glassEdge().withAlpha(0.35f));
        g.drawRoundedRectangle(b.reduced(0.5f), 8.0f, 1.0f);
    }
    else
    {
        // Light 模式：还原立体通透水晶玻璃面板（柔和投影 + 半透明磨砂玻璃 + 顶部光泽反光 + 精致边缘发丝边）
        drawGlassPanel(g, b, 8.0f);
    }
}
