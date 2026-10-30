#!/usr/bin/env bash
# AAX 发布签名：凭据由发布环境提供，只处理明确指定的构建产物。
set -euo pipefail

: "${SANMU_AAX_ACCOUNT:?请设置签名账号}"
: "${SANMU_AAX_PASSWORD:?请通过发布环境提供签名密码}"
: "${SANMU_AAX_SIGN_ID:?请设置代码签名身份}"
: "${SANMU_AAX_WCGUID:?请设置 PACE 包装配置标识}"

wraptool_path="${SANMU_AAX_WRAPTOOL:-/Applications/PACEAntiPiracy/Eden/Fusion/Versions/6/bin/wraptool}"
plugin_path="${1:?必须指定待签名的 AAX 构建产物路径}"

if [[ ! -x "$wraptool_path" || ! -d "$plugin_path" || "$plugin_path" != *.aaxplugin ]]; then
    echo '签名工具或 AAX 产物路径无效。' >&2
    exit 1
fi

# 钥匙串由发布环境预先解锁；不删除资源，不改变系统钥匙串授权。
"$wraptool_path" sign \
    --account "$SANMU_AAX_ACCOUNT" \
    --password "$SANMU_AAX_PASSWORD" \
    --signid "$SANMU_AAX_SIGN_ID" \
    --wcguid "$SANMU_AAX_WCGUID" \
    --dsig1-compat off \
    --in "$plugin_path" \
    --out "$plugin_path"
"$wraptool_path" verify --in "$plugin_path"
echo '指定 AAX 产物的 PACE 签名及校验完成；宿主加载仍需实测。'
