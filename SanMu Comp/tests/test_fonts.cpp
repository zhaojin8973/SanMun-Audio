// ==============================================================================
// test_fonts.cpp - 品牌字体匹配与度量诊断
// 验证 "Avenir Next Medium" / "Helvetica Neue" 等族名能否被 JUCE 正确解析，
// 以及各自字体的 ascent/descent（品牌文字基线对齐依赖这些值）。
// 用法：构建 sanmuq6_test_fonts 目标后直接运行
// ==============================================================================

#include <juce_gui_basics/juce_gui_basics.h>

#include <cstdio>

static void report(const juce::String& family, float height, int style)
{
    const juce::Font f(family, height, style);
    std::printf("%-24s typeface='%s' ascent=%.2f descent=%.2f height=%.2f\n",
                family.toRawUTF8(),
                f.getTypefaceName().toRawUTF8(),
                f.getAscent(), f.getDescent(), f.getHeight());
}

int main()
{
    const juce::StringArray names = juce::Font::findAllTypefaceNames();
    std::printf("--- 系统字体中含 Avenir/Helvetica 的族名 ---\n");
    for (const auto& n : names)
        if (n.contains("Avenir") || n.contains("Helvetica"))
            std::printf("  '%s'\n", n.toRawUTF8());

    std::printf("--- 字体解析结果 ---\n");
    report("Helvetica Neue", 22.0f, juce::Font::bold);
    report("Avenir Next Medium", 22.0f, juce::Font::plain);
    report("Avenir Next", 22.0f, juce::Font::plain);
    report("Avenir Next", 22.0f, juce::Font::bold);
    report("Helvetica Neue Medium", 22.0f, juce::Font::plain);
    report("Helvetica Neue", 22.0f, juce::Font::plain);
    return 0;
}
