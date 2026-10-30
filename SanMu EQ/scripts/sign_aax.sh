#!/usr/bin/env bash
# ==============================================================================
# SanMu Audio - AAX PACE Eden 自动代码签名脚本
# ==============================================================================
set -e

WRAPTOOL="${SANMU_WRAPTOOL:-/Applications/PACEAntiPiracy/Eden/Fusion/Versions/6/bin/wraptool}"
ACCOUNT="${SANMU_PACE_ACCOUNT:-${SANMU_AAX_ACCOUNT:?请通过环境变量提供 SANMU_PACE_ACCOUNT 或 SANMU_AAX_ACCOUNT}}"
PASSWORD="${SANMU_AAX_PASSWORD:?请通过发布环境提供 SANMU_AAX_PASSWORD，不要把密码写入脚本}"
SIGN_ID="${SANMU_PACE_SIGN_ID:-${SANMU_AAX_SIGN_ID:?请通过环境变量提供 SANMU_PACE_SIGN_ID 或 SANMU_AAX_SIGN_ID}}"
WCGUID="${SANMU_PACE_WCGUID:-${SANMU_AAX_WCGUID:?请通过环境变量提供 SANMU_PACE_WCGUID 或 SANMU_AAX_WCGUID}}"
KEYCHAIN_PATH="${SANMU_SIGNING_KEYCHAIN:-}"
KEYCHAIN_PASS="${SANMU_KEYCHAIN_PASS:-}"

PLUGIN_DIR="/Library/Application Support/Avid/Audio/Plug-Ins"

echo "========================================================"
echo "  SanMu Audio - AAX PACE Code Signing Tool"
echo "========================================================"

if [ ! -f "$WRAPTOOL" ]; then
    echo "❌ 错误: 未在系统中找到 wraptool: $WRAPTOOL"
    exit 1
fi

# 解锁钥匙串以支持非交互式签名
if [ -n "$KEYCHAIN_PATH" ] && [ -n "$KEYCHAIN_PASS" ] && [ -f "$KEYCHAIN_PATH" ]; then
    security unlock-keychain -p "$KEYCHAIN_PASS" "$KEYCHAIN_PATH" 2>/dev/null || true
    security set-key-partition-list -S apple-tool:,apple:,codesign: -s -k "$KEYCHAIN_PASS" "$KEYCHAIN_PATH" 2>/dev/null || true
fi

sign_plugin() {
    local plugin_name="$1"
    local plugin_path="$PLUGIN_DIR/$plugin_name"

    if [ ! -d "$plugin_path" ]; then
        echo "⚠️ 跳过: 目标插件未找到 -> $plugin_path"
        return
    fi

    echo ""
    echo "📦 正在为 $plugin_name 进行 AAX 官方签名..."
    "$WRAPTOOL" sign \
        --account "$ACCOUNT" \
        --password "$PASSWORD" \
        --signid "$SIGN_ID" \
        --wcguid "$WCGUID" \
        --dsig1-compat off \
        --in "$plugin_path" \
        --out "$plugin_path"

    echo "🔍 正在校验签名有效性..."
    "$WRAPTOOL" verify --in "$plugin_path"
    echo "✅ $plugin_name 官方数字签名成功！"
}

# 依次为插件签名
sign_plugin "SanMu EQ6.aaxplugin"
sign_plugin "SanMu Comp.aaxplugin"

echo ""
echo "🎉 全部 AAX 插件签名完成，已具备全球正式版 Pro Tools 运行资质！"
