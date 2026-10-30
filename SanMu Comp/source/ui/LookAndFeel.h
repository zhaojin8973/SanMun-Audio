// ==============================================================================
// LookAndFeel.h - 自定义外观（第五轮：浅色清新 —— 米白 + 薄荷绿 + 天蓝）
// ==============================================================================

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// ---- UI 主题 ----
enum class SanMuCompTheme { Light, Dark };

// SanMu 系列玻璃材质与控件绘制；颜色语义取自 2026-09-10 设计系统。
class SanMuCompLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SanMuCompLookAndFeel();
    ~SanMuCompLookAndFeel() override;

    // ---- 旋钮绘制 ----
    void drawRotarySlider(juce::Graphics& g,
                          int x, int y, int width, int height,
                          float sliderPos, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider& slider) override;

    // ---- 标签绘制 ----
    juce::Label* createSliderTextBox(juce::Slider& slider) override;

    // ---- ComboBox 样式 ----
    void drawComboBox(juce::Graphics& g, int width, int height,
                      bool isButtonDown, int buttonX, int buttonY,
                      int buttonW, int buttonH, juce::ComboBox& box) override;

    void positionComboBoxText(juce::ComboBox& box, juce::Label& label) override;

    // ---- 按钮样式 ----
    juce::Font getTextButtonFont(juce::TextButton& button, int buttonHeight) override;
    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override;

    void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                        bool shouldDrawButtonAsHighlighted,
                        bool shouldDrawButtonAsDown) override;

    /** 下拉框字体与 Zero Latency（Model 按钮 17px）统一 */
    juce::Font getComboBoxFont(juce::ComboBox& comboBox) override;
    /** 弹出菜单中文渲染（PingFang SC） */
    juce::Font getPopupMenuFont() override;

    // ---- 弹窗（下拉菜单）样式 ----
    void drawPopupMenuBackground(juce::Graphics& g, int width, int height) override;

    // ---- Tooltip（第三十八轮：JUCE 6 默认字体 Helvetica 不含中文，
    // 会渲染成乱码；重写为 PingFang SC 支持中文注解） ----
    void drawTooltip(juce::Graphics& g, const juce::String& text,
                     int width, int height) override;
    juce::Rectangle<int> getTooltipBounds(const juce::String& tipText,
                                          juce::Point<int> screenPos,
                                          juce::Rectangle<int> parentArea) override;

    // ---- 玻璃面板绘制工具（供各组件复用） ----
    /** 画一块小玻璃面板：底部投影 + 半透明填充 + 顶部反光 + 1px 边缘高光边框 */
    static void drawGlassPanel(juce::Graphics& g,
                               juce::Rectangle<float> bounds,
                               float cornerRadius);

    /** 绘制全模式通用高精度玻璃圆盘（标题 + 实时数值置于圆内，支持单向/双向光弧与模式专属色） */
    void drawCompDial(juce::Graphics& g,
                      juce::Rectangle<float> bounds,
                      float sliderPos,
                      const juce::String& title,
                      const juce::String& valueText,
                      const juce::Colour& arcColour,
                      bool isBipolar,
                      bool hovered,
                      bool hasAutoCapsule = false);

    void drawLinearSlider(juce::Graphics& g,
                          int x, int y, int width, int height,
                          float sliderPos,
                          float minSliderPos,
                          float maxSliderPos,
                          const juce::Slider::SliderStyle style,
                          juce::Slider& slider) override;

    /** 绘制 1176 经典大圆盘（INPUT / OUTPUT，细弧圈，圆内实时主次显示） */
    void drawFetLargeDial(juce::Graphics& g,
                          juce::Rectangle<float> bounds,
                          float sliderPos,
                          const juce::String& title,
                          const juce::String& valueText,
                          bool isInput,
                          bool isLinked,
                          bool hovered);

    /** 绘制 1176 中型圆盘（ATTACK / RELEASE，细弧圈，圆内实时主次显示） */
    void drawFetMediumDial(juce::Graphics& g,
                           juce::Rectangle<float> bounds,
                           float sliderPos,
                           const juce::String& title,
                           const juce::String& valueText,
                           bool hovered);

    /** 绘制 1176 Ratio 按压式圆形玻璃按键（4:1 黄 -> 8:1 深黄 -> 12:1 橙红 -> 20:1 鲜红 -> ALL 强烈发光红） */
    void drawFetRatioButton(juce::Graphics& g,
                            juce::Rectangle<float> bounds,
                            int ratioIndex,
                            bool isHighlighted,
                            bool isDown);

    /** 绘制 1176 SC (Sidechain HP) 按压式圆形玻璃按键（OFF / 60 Hz / 90 Hz / 150 Hz / 250 Hz 五档翠绿宝石渐进灯光） */
    void drawFetScButton(juce::Graphics& g,
                         juce::Rectangle<float> bounds,
                         int scIndex,
                         bool isHighlighted,
                         bool isDown);

    /** 绘制 1176 Ratio 圆形旋钮（两行文字：主 RATIO，次当前比值如 4:1，细弧圈带 5 档圆弧刻度） */
    void drawFetRatioDial(juce::Graphics& g,
                          juce::Rectangle<float> bounds,
                          float sliderPos,
                          int ratioIndex,
                          bool hovered);

    /** 绘制 1176 小型圆盘（HR / MIX / SC，细弧圈，圆内实时主次显示） */
    void drawFetSmallDial(juce::Graphics& g,
                          juce::Rectangle<float> bounds,
                          float sliderPos,
                          const juce::String& title,
                          const juce::String& valueText,
                          bool isBipolar,
                          bool hovered);

    /** 绘制 1176 专属圆形发光开关按钮（LINK / AUTO） */
    void drawFetCircleButton(juce::Graphics& g,
                             juce::Button& button,
                             const juce::String& title,
                             bool isHighlighted,
                             bool isDown);

    /** 绘制 Input / Output 中间的栓连标志（LINK 开启时发光联通，关闭时暗色解开） */
    static void drawLinkBridge(juce::Graphics& g,
                               juce::Rectangle<float> bounds,
                               bool isLinked,
                               bool isHovered);

    /** 画整块主玻璃板（第五轮）：大圆角、白色半透明板体、灰柔投影、发丝边。
        第三十六轮：圆角半径可传入（窗口缩放时按比例缩放） */
    static void drawMainGlass(juce::Graphics& g,
                              juce::Rectangle<float> bounds,
                              float cornerRadius = 22.0f);

    /** 绘制值气泡（点击旋钮时弹出参数值；EqBandControl / OutputKnobControl 共用） */
    static void drawValueBubble(juce::Graphics& g,
                                juce::Rectangle<float> bounds,
                                const juce::String& text,
                                float alpha);

    /** 旋钮核心绘制（无 Slider 依赖；drawRotarySlider / EqBandControl / OutputKnobControl 复用） */
    static void drawRotaryKnob(juce::Graphics& g,
                               juce::Rectangle<float> bounds,
                               float sliderPos,          // 0..1
                               const juce::Colour& fillColor,
                               bool hovered);

    /** 填充弧绘制（无指针，颜色填充弧表示参数；三圈旋钮与输出旋钮复用） */
    static void drawArcRing(juce::Graphics& g,
                            juce::Point<float> centre,
                            float radius,
                            float sliderPos,           // 0..1
                            const juce::Colour& colour,
                            bool hovered,
                            bool showFill = true);     // false = 只画灰色轨道

    /** 增益弧环（0dB 基准版）：0 在顶部无颜色；正增益顺时针、负增益逆时针 */
    static void drawGainArcRing(juce::Graphics& g,
                                juce::Point<float> centre,
                                float radius,
                                float sliderPos,           // 0..1
                                const juce::Colour& colour,
                                bool hovered);

    // ---- 获取频段颜色 ----
    static juce::Colour getBandColor(int bandIndex);

    // ---- 获取模式胶囊/光弧专属颜色 ----
    static juce::Colour getModeColour(int mode, bool dark);
    static juce::Colour getModeColour(int mode);

    // ---- 主题控制 ----
    static void setTheme(SanMuCompTheme t);
    static SanMuCompTheme getTheme();
    void setInstanceTheme(SanMuCompTheme t) { theme = t; }
    SanMuCompTheme getInstanceTheme() const { return theme; }
    /** 绘制前调用：把静态主题同步为本实例主题（多实例互不干扰） */
    static void syncThemeFrom(juce::Component& c);
    void refreshThemeColours();   // 主题切换后重设组件配色（构造也调用）

    // ---- 配色 tokens（按当前主题返回） ----
    static juce::Colour backgroundDark();
    static juce::Colour backgroundBase();
    static juce::Colour backgroundEnd();
    static juce::Colour backgroundMedium();
    static juce::Colour glassFill();
    static juce::Colour glassFillStrong();
    static juce::Colour glassEdge();
    static juce::Colour glassHighlight();
    static juce::Colour glassShadow();
    static juce::Colour accentGreen();
    static juce::Colour accentPink();
    static juce::Colour accentYellow();
    static juce::Colour accentIndigo();
    static juce::Colour accentBlue();
    static juce::Colour fetBlueStripe();
    static juce::Colour textLight();
    static juce::Colour textWhite();
    static juce::Colour textDim();
    static juce::Colour gridLine();
    static juce::Colour knobTrack();

private:
    SanMuCompTheme theme = SanMuCompTheme::Light;   // 本实例主题（多实例各自独立）

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SanMuCompLookAndFeel)
};
