// ==============================================================================
// test_iconpaths.cpp - 官方 SVG 图标路径解析验证
// 验证 JUCE 内置 Drawable::parseSVGPath 能解析 Material Symbols 的
// 960 网格 path 数据（含 q/T 命令与负坐标），并正确缩放为 16x16。
// 用法：构建 sanmuq6_test_icons 目标后直接运行（退出码 0 = 通过）
// ==============================================================================

#include "ui/IconPaths.h"

#include <cmath>
#include <cstdio>

int main()
{
    const auto& headphones = IconPaths::headphones();

    const auto boundsH = headphones.getBounds();

    std::printf("headphones: notEmpty=%d bounds=%.2f,%.2f %.2fx%.2f\n",
                (int) !headphones.isEmpty(),
                boundsH.getX(), boundsH.getY(),
                boundsH.getWidth(), boundsH.getHeight());

    // 缩放后应近似占满 12x12（scaleToFit 保留宽高比，会有少量留白）
    // 第十五轮：耳机与其他图标统一 12x12 视觉大小
    const auto near12 = [](float v) { return std::abs(v - 12.0f) < 1.5f; };

    bool ok = true;
    ok &= !headphones.isEmpty();
    ok &= near12(boundsH.getWidth()) && near12(boundsH.getHeight());
    ok &= boundsH.getX() >= -0.5f && boundsH.getY() >= -0.5f;  // 原点已平移到左上角

    std::printf(ok ? "PASS: 图标路径解析与缩放正确\n"
                   : "FAIL: 图标路径解析异常\n");
    return ok ? 0 : 1;
}
