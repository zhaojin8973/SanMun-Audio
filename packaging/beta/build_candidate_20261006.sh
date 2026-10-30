#!/bin/zsh
set -euo pipefail
root="${SANMU_ROOT:-$(cd "$(dirname "$0")/../.." && pwd)}"
out="$root/internal-test-20261006-catalina-ui-fix3"
mkdir -p "$out/bundles" "$out/components" "$out/pkg" "$out/stage"
app="${SANMU_APP_SIGN_ID:?请通过环境变量提供 SANMU_APP_SIGN_ID}"
installer="${SANMU_INSTALLER_SIGN_ID:-}"
pace_account="${SANMU_PACE_ACCOUNT:-}"
entitlements="$root/packaging/entitlements.plist"
wraptool="${SANMU_WRAPTOOL:-/Applications/PACEAntiPiracy/Eden/Fusion/Versions/6/bin/wraptool}"
skip_pace="${SANMU_SKIP_PACE:-0}"
products=(
  "EQ6|SanMu EQ|build-release-20260928|SanMuQ6|SanMu EQ6|0.1.5|SMQ6|${SANMU_EQ_WCGUID:-}|com.SanMuAudio.SanMuQ6"
  "Comp|SanMu Comp|build-release-20260928|SanMuComp|SanMu Comp|0.1.17|SMC1|${SANMU_COMP_WCGUID:-}|com.SanMuAudio.SanMuComp"
  "Delay|SanMu Delay|build-release-20260928|SanMuDelay|SanMu Delay|0.1.5|SMD1|${SANMU_DELAY_WCGUID:-}|com.SanMuAudio.SanMuDelay"
  "Reverb|SanMu Reverb|build-release-20260928-make|SanMuReverb|SanMu Reverb|1.0.4|SMRV|${SANMU_REVERB_WCGUID:-}|com.SanMuAudio.SanMuReverb"
)
for row in $products; do
  IFS='|' read -r short project build target name version code guid bundle <<< "$row"
  src="$root/$project/$build/source/${target}_artefacts/Release"
  dst="$out/bundles/$short"
  mkdir -p "$dst"
  ditto "$src/VST3/$name.vst3" "$dst/$name.vst3"
  ditto "$src/AU/$name.component" "$dst/$name.component"
  ditto "$src/AAX/$name.aaxplugin" "$dst/$name.aaxplugin"
  codesign --force --deep --options runtime --entitlements "$entitlements" --sign "$app" "$dst/$name.vst3"
  codesign --force --deep --options runtime --entitlements "$entitlements" --sign "$app" "$dst/$name.component"
  if [[ "$skip_pace" == 1 ]]; then
    echo "跳过 $short PACE 数字信封（仅用于结构验证）"
  else
    tmp="$dst/$name.aaxplugin.wrapped"
    "$wraptool" sign --verbose --account "$pace_account" --wcguid "$guid" --signid "$app" --dsig1-compat off --in "$dst/$name.aaxplugin" --out "$tmp" --autoinstall on
    python3 - "$dst/$name.aaxplugin" <<'PY'
import shutil, sys
shutil.rmtree(sys.argv[1], ignore_errors=True)
PY
    mv "$tmp" "$dst/$name.aaxplugin"
  fi
  codesign --force --deep --options runtime --entitlements "$entitlements" --sign "$app" "$dst/$name.aaxplugin"
  codesign --verify --deep --strict --verbose=2 "$dst/$name.vst3" > "$out/${short}-vst3-codesign.txt" 2>&1
  codesign --verify --deep --strict --verbose=2 "$dst/$name.component" > "$out/${short}-au-codesign.txt" 2>&1
  codesign --verify --deep --strict --verbose=2 "$dst/$name.aaxplugin" > "$out/${short}-aax-codesign.txt" 2>&1
  mkdir -p "$out/stage/$short/Library/Audio/Plug-Ins/VST3" "$out/stage/$short/Library/Audio/Plug-Ins/Components" "$out/stage/$short/Library/Application Support/Avid/Audio/Plug-Ins"
  ditto "$dst/$name.vst3" "$out/stage/$short/Library/Audio/Plug-Ins/VST3/$name.vst3"
  ditto "$dst/$name.component" "$out/stage/$short/Library/Audio/Plug-Ins/Components/$name.component"
  ditto "$dst/$name.aaxplugin" "$out/stage/$short/Library/Application Support/Avid/Audio/Plug-Ins/$name.aaxplugin"
  pkgbuild --root "$out/stage/$short" --identifier "$bundle" --version "$version" --install-location / --ownership recommended "$out/components/$short-VST3-AU-AAX.pkg" > "$out/${short}-pkgbuild.txt" 2>&1
  pkgutil --check-signature "$out/components/$short-VST3-AU-AAX.pkg" > "$out/${short}-component-signature.txt" 2>&1 || true
  ditto "$out/components/$short-VST3-AU-AAX.pkg" "$out/pkg/SanMu_${short}_v${version}_CatalinaCandidate.pkg"
  shasum -a 256 "$out/pkg/SanMu_${short}_v${version}_CatalinaCandidate.pkg" > "$out/${short}-sha256.txt"
  echo "完成 $short"
done
cat > "$out/README.md" <<EOF
# SanMu Audio Catalina 兼容候选包（2026-10-06）

本目录是新编译的 macOS Universal 2 候选包，界面兼容修复覆盖旧版 WebKit、五档缩放、弹窗定位、画布清晰度、WebGL 1 粒子/水波和颜色回退。

版本沿用当前工程内测版本：EQ 0.1.5、Comp 0.1.17、Delay 0.1.5、Reverb 1.0.4。候选包未覆盖旧目录。

每个安装包包含 VST3、AU 和 AAX 三种格式。AAX 目前只经过 Apple Developer ID 签名，未加 PACE 数字信封；安装包本身也未做 Developer ID Installer 签名，尚未提交 Apple 公证，因此只能用于 Catalina + Pro Tools 的内部结构和兼容性验收。

请在 Catalina + Pro Tools 上先安装并验证：窗口打开、设置面板、85/100/115/125/150%、旋钮、粒子/水波、电平和实际音频。当前执行机不是 Catalina，不能代替这一步。
EOF
