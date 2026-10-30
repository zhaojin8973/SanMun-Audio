// ==============================================================================
// IconPaths.h - 官方矢量图标路径（Material Symbols, Apache 2.0 / CC-BY 4.0）
// ==============================================================================
// 图标数据来源：https://github.com/google/material-design-icons
// （symbols/web/headphones|power/materialsymbolsoutlined/*_24px.svg）
// 用 JUCE 内置 Drawable::parseSVGPath 把官方 SVG path 数据解析为 juce::Path，
// 保证图标为专业设计师绘制的标准造型，而非手绘近似。

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace IconPaths
{
namespace detail
{
    /** 解析官方 SVG path 数据（materialicons 960 网格）→ 缩放适配目标正方形 */
    inline juce::Path fromSVG(const char* d, float size)
    {
        juce::Path p = juce::Drawable::parseSVGPath(d);
        p.scaleToFit(0.0f, 0.0f, size, size, true);
        return p;
    }

    // ---- Material Symbols "headphones"（监听耳机, outlined） ----
    constexpr const char* headphonesD =
        "M360-120H200q-33 0-56.5-23.5T120-200v-280q0-75 28.5-140.5t77-114"
        "q48.5-48.5 114-77T480-840q75 0 140.5 28.5t114 77"
        "q48.5 48.5 77 114T840-480v280q0 33-23.5 56.5T760-120H600v-320h160v-40"
        "q0-117-81.5-198.5T480-760q-117 0-198.5 81.5T200-480v40h160v320"
        "Zm-80-240h-80v160h80v-160Zm400 0v160h80v-160h-80Zm-400 0h-80 80Zm400 0h80-80Z";

    // ---- Material Symbols "undo" / "redo"（撤销/反撤销, outlined, 1000 网格） ----
    constexpr const char* undoD =
        "M280-200v-80h284q63 0 109.5-40T720-420q0-60-46.5-100T564-560H312l104 104-56 56-200-200"
        " 200-200 56 56-104 104h252q97 0 166.5 63T800-420q0 94-69.5 157T564-200H280Z";

    constexpr const char* redoD =
        "M396-200q-97 0-166.5-63T160-420q0-94 69.5-157T396-640h252L544-744l56-56 200 200-200"
        " 200-56-56 104-104H396q-63 0-109.5 40T240-420q0 60 46.5 100T396-280h284v80H396Z";

    // ---- Material Symbols "settings"（齿轮, outlined, 1000 网格） ----
    constexpr const char* settingsD =
        "m370-80-16-128q-13-5-24.5-12T307-235l-119 50L78-375l103-78q-1-7-1-13.5v-27"
        "q0-6.5 1-13.5L78-585l110-190 119 50q11-8 23-15t24-12l16-128h220l16 128q13 5"
        " 24.5 12t22.5 15l119-50 110 190-103 78q1 7 1 13.5v27q0 6.5-2 13.5l103 78-110"
        " 190-118-50q-11 8-23 15t-24 12L590-80H370Zm70-80h79l14-106q31-8 57.5-23.5T639"
        "-327l99 41 39-68-86-65q5-14 7-29.5t2-31.5q0-16-2-31.5t-7-29.5l86-65-39-68-99"
        " 42q-22-23-48.5-38.5T533-694l-13-106h-79l-14 106q-31 8-57.5 23.5T321-633l-99"
        "-41-39 68 86 64q-5 15-7 30t-2 32q0 16 2 31t7 30l-86 65 39 68 99-42q22 23 48.5"
        " 38.5T427-266l13 106Zm42-180q58 0 99-41t41-99q0-58-41-99t-99-41q-59 0-99.5"
        " 41T342-480q0 58 40.5 99t99.5 41Zm-2-140Z";

    // ---- Material Symbols "power_settings_new"（电源/旁通图标） ----
    constexpr const char* powerD =
        "M440-440v-400h80v400h-80Zm40 360q-83 0-156-31.5T197-197q-54-54-85.5-127T80-480"
        "q0-75 26.5-143.5T182-748l58 58q-41 47-64.5 106T152-480q0 136 96 232t232 96"
        "q136 0 232-96t96-232q0-65-23.5-124T720-690l58-58q49 56 75.5 124.5T880-480"
        "q0 83-31.5 156T763-197q-54 54-127 85.5T480-80Z";

    // ---- Lucide Icons (24 网格, 与 SanMu Comp/Delay/Reverb 严格同源) ----
    constexpr const char* chevronLeftD = "m15 18-6-6 6-6";
    constexpr const char* chevronRightD = "m9 18 6-6-6-6";
    constexpr const char* chevronDownD = "m6 9 6 6 6-6";
    constexpr const char* copyD =
        "M10 8h10a2 2 0 0 1 2 2v10a2 2 0 0 1-2 2H10a2 2 0 0 1-2-2V10a2 2 0 0 1 2-2z "
        "M4 16c-1.1 0-2-.9-2-2V4c0-1.1.9-2 2-2h10c1.1 0 2 .9 2 2";
    constexpr const char* saveD =
        "M15.2 3a2 2 0 0 1 1.4.6l3.8 3.8a2 2 0 0 1 .6 1.4V19a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2z "
        "M17 21v-7a1 1 0 0 0-1-1H8a1 1 0 0 0-1 1v7 M7 3v4a1 1 0 0 0 1 1h7";
    constexpr const char* trashD =
        "M3 6h18 M19 6v14c0 1-1 2-2 2H7c-1 0-2-1-2-2V6 M8 6V4c0-1 1-2 2-2h4c1 0 2 1 2 2v2 M10 11v6 M14 11v6";
    constexpr const char* closeD = "M18 6 6 18 M6 6l12 12";

}

/** 监听耳机图标路径（12x12, 原点在左上角；与类型曲线/电源统一视觉大小） */
inline const juce::Path& headphones()
{
    static const juce::Path p = detail::fromSVG(detail::headphonesD, 12.0f);
    return p;
}

/** 电源/旁通图标（12x12, 原点左上角） */
inline const juce::Path& power()
{
    static const juce::Path p = detail::fromSVG(detail::powerD, 12.0f);
    return p;
}

/** 撤销图标（12x12, 原点左上角） */
inline const juce::Path& undo()
{
    static const juce::Path p = detail::fromSVG(detail::undoD, 12.0f);
    return p;
}

/** 反撤销（重做）图标（12x12, 原点左上角） */
inline const juce::Path& redo()
{
    static const juce::Path p = detail::fromSVG(detail::redoD, 12.0f);
    return p;
}

/** 设置（齿轮）图标（12x12, 原点左上角） */
inline const juce::Path& settings()
{
    static const juce::Path p = detail::fromSVG(detail::settingsD, 12.0f);
    return p;
}

/** 左箭头图标（12x12） */
inline const juce::Path& chevronLeft()
{
    static const juce::Path p = detail::fromSVG(detail::chevronLeftD, 12.0f);
    return p;
}

/** 右箭头图标（12x12） */
inline const juce::Path& chevronRight()
{
    static const juce::Path p = detail::fromSVG(detail::chevronRightD, 12.0f);
    return p;
}

/** 下拉箭头图标（10x10） */
inline const juce::Path& chevronDown()
{
    static const juce::Path p = detail::fromSVG(detail::chevronDownD, 10.0f);
    return p;
}

/** 复制图标（12x12） */
inline const juce::Path& copy()
{
    static const juce::Path p = detail::fromSVG(detail::copyD, 12.0f);
    return p;
}

/** 保存磁盘图标（12x12） */
inline const juce::Path& save()
{
    static const juce::Path p = detail::fromSVG(detail::saveD, 12.0f);
    return p;
}

/** 删除垃圾桶图标（12x12） */
inline const juce::Path& trash()
{
    static const juce::Path p = detail::fromSVG(detail::trashD, 12.0f);
    return p;
}

/** 关闭 X 图标（12x12） */
inline const juce::Path& close()
{
    static const juce::Path p = detail::fromSVG(detail::closeD, 12.0f);
    return p;
}

inline const juce::Path& plus()
{
    static const juce::Path p = detail::fromSVG("M12 5v14 M5 12h14", 12.0f);
    return p;
}

/** 相位反转图标（∅：圆环 + 45° 斜线；描边绘制，12x12） */
inline juce::Path phaseInvert()
{
    juce::Path p;
    // 第四十轮：圆环填满 12 网格（11px），与其他图标同尺寸；线条统一 1.2
    p.addEllipse(0.5f, 0.5f, 11.0f, 11.0f);
    p.startNewSubPath(2.0f, 10.0f);
    p.lineTo(10.0f, 2.0f);
    return p;
}
}
