#!/bin/zsh
set -euo pipefail
root="${SANMU_ROOT:-$(cd "$(dirname "$0")/../.." && pwd)}"
out="$root/internal-test-20260930-catalina-ui-fix2"
python3 - "$out" <<'PY_CLEAN_OUT'
import shutil, sys
shutil.rmtree(sys.argv[1], ignore_errors=True)
PY_CLEAN_OUT
mkdir -p "$out/bundles" "$out/components"
app="${SANMU_APP_SIGN_ID:?请通过环境变量提供 SANMU_APP_SIGN_ID}"
installer="${SANMU_INSTALLER_SIGN_ID:-}"
pace_account="${SANMU_PACE_ACCOUNT:-}"
entitlements="$root/packaging/entitlements.plist"
wraptool="${SANMU_WRAPTOOL:-/Applications/PACEAntiPiracy/Eden/Fusion/Versions/6/bin/wraptool}"
skip_pace="${SANMU_SKIP_PACE:-0}"
products=(
  "EQ|SanMu EQ|build-release-20260928|SanMuQ6|SanMu EQ6|0.1.5|SMQ6|${SANMU_EQ_WCGUID:-}|com.SanMuAudio.SanMuQ6"
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
    echo "跳过 $short 的 PACE 数字信封：当前包保持 AAX Signing Only（内测模式）"
  else
    tmp="$dst/$name.aaxplugin.wrapped"
    python3 - "$tmp" <<'PY_CLEAN_TMP'
import shutil, sys
shutil.rmtree(sys.argv[1], ignore_errors=True)
PY_CLEAN_TMP
    "$wraptool" sign --verbose --account "$pace_account" --wcguid "$guid" --signid "$app" --dsig1-compat off --in "$dst/$name.aaxplugin" --out "$tmp" --autoinstall on
    python3 - "$dst/$name.aaxplugin" <<'PY_CLEAN_AAX'
import shutil, sys
shutil.rmtree(sys.argv[1], ignore_errors=True)
PY_CLEAN_AAX
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
  echo "完成 $short"
done
