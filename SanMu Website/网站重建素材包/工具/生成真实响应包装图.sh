#!/bin/zsh
set -euo pipefail

# 生成正式的数字产品包装视觉：左侧为编辑部式产品信息，右侧为带顶面与侧面的软件盒。
# 盒面上的 UI 只做等比缩放，不裁切、不重绘，来源仍是 DAW 中的真实音乐响应捕捉。
magick_bin="$(command -v magick)"
root="$(cd "$(dirname "$0")/.." && pwd)"
source_dir="$root/03-插件实图/音乐实录-2026-09-27/修缮裁切"
out_dir="$root/04-包装宣传图/真实响应-2026-09-27"
font_sans="/System/Library/Fonts/HelveticaNeue.ttc"
font_cn="/System/Library/Fonts/Hiragino Sans GB.ttc"
mkdir -p "$out_dir"

单品包装() {
  local id="$1" name="$2" zh_tag="$3" en_tag="$4" accent="$5" light="$6" dark="$7"
  local ui="$source_dir/$light"
  "$magick_bin" -size 1800x1200 xc:'#f7f5f0' \
    -fill '#e8e3da' -stroke none -draw 'circle 1630,120 1630,500' \
    -fill '#181816' -font "$font_sans" -pointsize 68 -gravity northwest -annotate +110+104 "$name" \
    -font "$font_cn" -pointsize 27 -fill '#5d5a53' -annotate +114+214 "$zh_tag" \
    -font "$font_sans" -pointsize 18 -fill '#7b776f' -annotate +114+258 "$en_tag" \
    -fill "$accent" -draw 'roundrectangle 112,334 380,342 4,4' \
    -font "$font_sans" -pointsize 16 -fill '#77736b' -annotate +114+1018 'SANMU AUDIO  /  SOFTWARE INSTRUMENTS' \
    -font "$font_sans" -pointsize 14 -fill '#77736b' -annotate +114+1050 'REAL PLAYBACK RESPONSE  ·  DAW CAPTURE  ·  UI KEPT INTACT' \
    -fill 'rgba(20,20,18,0.18)' -draw 'roundrectangle 752,158 1650,1110 18,18' \
    -fill '#c7c0b5' -draw 'polygon 730,140 790,98 1710,98 1650,140' \
    -fill '#aaa196' -draw 'polygon 1650,140 1710,98 1710,1020 1650,1070' \
    -fill '#fffdfa' -draw 'roundrectangle 730,140 1650,1070 14,14' \
    -fill "$accent" -draw 'roundrectangle 730,140 752,1070 10,10' \
    -fill '#181816' -font "$font_sans" -pointsize 18 -annotate +810+224 'SANMU AUDIO  /  REAL RESPONSE EDITION' \
    -fill '#5d5a53' -font "$font_sans" -pointsize 38 -annotate +810+286 "$name" \
    -fill '#77736b' -font "$font_sans" -pointsize 15 -annotate +810+322 'A product view built from a moving mix' \
    -fill '#eeeae3' -draw 'roundrectangle 800,356 1580,900 12,12' \
    \( "$ui" -resize '744x510>' -bordercolor '#d8d3ca' -border 10x10 \) -gravity northwest -geometry +818+372 -composite \
    -fill '#5d5a53' -font "$font_sans" -pointsize 14 -annotate +810+955 'CAPTURED IN DAW  ·  MUSIC RESPONSE  ·  SANMU AUDIO' \
    -fill "$accent" -draw 'roundrectangle 810,995 1050,1001 3,3' \
    "$out_dir/${id}-light.png"

  ui="$source_dir/$dark"
  "$magick_bin" -size 1800x1200 xc:'#191b20' \
    -fill '#2d3038' -stroke none -draw 'circle 1630,120 1630,500' \
    -fill '#f5f2eb' -font "$font_sans" -pointsize 68 -gravity northwest -annotate +110+104 "$name" \
    -font "$font_cn" -pointsize 27 -fill '#b6b1a8' -annotate +114+214 "$zh_tag" \
    -font "$font_sans" -pointsize 18 -fill '#97938c' -annotate +114+258 "$en_tag" \
    -fill "$accent" -draw 'roundrectangle 112,334 380,342 4,4' \
    -font "$font_sans" -pointsize 16 -fill '#97938c' -annotate +114+1018 'SANMU AUDIO  /  SOFTWARE INSTRUMENTS' \
    -font "$font_sans" -pointsize 14 -fill '#97938c' -annotate +114+1050 'REAL PLAYBACK RESPONSE  ·  DAW CAPTURE  ·  UI KEPT INTACT' \
    -fill 'rgba(0,0,0,0.46)' -draw 'roundrectangle 752,158 1650,1110 18,18' \
    -fill '#41454e' -draw 'polygon 730,140 790,98 1710,98 1650,140' \
    -fill '#2a2d34' -draw 'polygon 1650,140 1710,98 1710,1020 1650,1070' \
    -fill '#252830' -draw 'roundrectangle 730,140 1650,1070 14,14' \
    -fill "$accent" -draw 'roundrectangle 730,140 752,1070 10,10' \
    -fill '#f5f2eb' -font "$font_sans" -pointsize 18 -annotate +810+224 'SANMU AUDIO  /  REAL RESPONSE EDITION' \
    -fill '#f5f2eb' -font "$font_sans" -pointsize 38 -annotate +810+286 "$name" \
    -fill '#aaa59d' -font "$font_sans" -pointsize 15 -annotate +810+322 'A product view built from a moving mix' \
    -fill '#30333a' -draw 'roundrectangle 800,356 1580,900 12,12' \
    \( "$ui" -resize '744x510>' -bordercolor '#4a4e57' -border 10x10 \) -gravity northwest -geometry +818+372 -composite \
    -fill '#b3aea5' -font "$font_sans" -pointsize 14 -annotate +810+955 'CAPTURED IN DAW  ·  MUSIC RESPONSE  ·  SANMU AUDIO' \
    -fill "$accent" -draw 'roundrectangle 810,995 1050,1001 3,3' \
    "$out_dir/${id}-dark.png"
}

单品包装 eq6 'SanMu EQ6' '频谱中的音乐响应' 'MUSIC RESPONSE' '#62a8c8' eq6-light.png eq6-dark.png
单品包装 comp 'SanMu Comp' '动态线与增益控制' 'DYNAMIC CONTROL' '#62bda9' comp-light.png comp-dark.png
单品包装 delay 'SanMu Delay' '节拍同步的回声空间' 'TEMPO ECHOES' '#9c86cf' delay-light.png delay-dark.png
单品包装 reverb 'SanMu Reverb' '水波与粒子中的空间' 'SPACE IN MOTION' '#c49c5c' reverb-light.png reverb-dark.png

套装包装() {
  local theme="$1" bg="$2" fg="$3" muted="$4" spine="$5" frame="$6"
  local a="$source_dir/eq6-${theme}.png" b="$source_dir/comp-${theme}.png" c="$source_dir/delay-${theme}.png" d="$source_dir/reverb-${theme}.png"
  "$magick_bin" -size 1800x1200 xc:"$bg" \
    -fill "$spine" -stroke none -draw 'circle 1640,112 1640,520' \
    -fill "$fg" -font "$font_sans" -pointsize 50 -gravity northwest -annotate +105+100 'SanMu Suite' \
    -font "$font_cn" -pointsize 27 -fill "$muted" -annotate +109+206 '四款插件，一套真实响应' \
    -font "$font_sans" -pointsize 21 -fill "$muted" -annotate +109+248 'FOUR TOOLS  /  ONE MOVING MIX' \
    -fill '#c49c5c' -draw 'roundrectangle 110,330 380,338 4,4' \
    -font "$font_sans" -pointsize 16 -fill "$muted" -annotate +110+1018 'EQ6  ·  COMP  ·  DELAY  ·  REVERB' \
    -font "$font_sans" -pointsize 14 -fill "$muted" -annotate +110+1050 'DAW CAPTURES  ·  UI KEPT INTACT' \
    -fill 'rgba(0,0,0,0.25)' -draw 'roundrectangle 550,158 1650,1110 18,18' \
    -fill "$spine" -draw 'polygon 530,140 590,98 1710,98 1650,140' \
    -fill "$frame" -draw 'polygon 1650,140 1710,98 1710,1020 1650,1070' \
    -fill "$frame" -draw 'roundrectangle 530,140 1650,1070 14,14' \
    -fill '#c49c5c' -draw 'roundrectangle 530,140 552,1070 10,10' \
    -fill "$fg" -font "$font_sans" -pointsize 18 -annotate +610+224 'SANMU AUDIO  /  FOUR-PLUGIN COLLECTION' \
    -fill "$muted" -font "$font_sans" -pointsize 15 -annotate +610+290 'REAL PLAYBACK CAPTURES  ·  UI KEPT INTACT' \
    -fill "$bg" -draw 'roundrectangle 600,336 1580,960 12,12' \
    \( "$a" -resize '448x290>' -bordercolor "$frame" -border 8x8 \) -gravity northwest -geometry +620+360 -composite \
    \( "$b" -resize '448x290>' -bordercolor "$frame" -border 8x8 \) -gravity northwest -geometry +1110+360 -composite \
    \( "$c" -resize '448x290>' -bordercolor "$frame" -border 8x8 \) -gravity northwest -geometry +620+680 -composite \
    \( "$d" -resize '448x290>' -bordercolor "$frame" -border 8x8 \) -gravity northwest -geometry +1110+680 -composite \
    "$out_dir/suite-${theme}.png"
}

套装包装 light '#f7f5f0' '#181816' '#77736b' '#c7c0b5' '#fffdfa'
套装包装 dark '#191b20' '#f5f2eb' '#97938c' '#41454e' '#252830'
