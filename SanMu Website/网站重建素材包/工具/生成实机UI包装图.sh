#!/bin/zsh
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"; SHOT="$ROOT/03-插件实图/当前-2026-09-21"; OUT="$ROOT/04-包装宣传图/当前-实机UI"; TMP="$ROOT/04-包装宣传图/.tmp"; mkdir -p "$OUT/单品" "$OUT/套装" "$TMP"
FONT="/System/Library/Fonts/Supplemental/Arial.ttf"
make_single(){
 slug="$1"; product="$2"; accent="$3"; mode="$4"; blurb="$5"; src="$SHOT/${slug}-${mode}-light-en.png"; resized="$TMP/${slug}-single.png"; base="$TMP/${slug}-base.png"
 magick "$src"  -resize '1000x625!' "$resized"
 magick -size 1536x1024 xc:'#f4f4ef' -fill "$accent" -draw 'rectangle 0,0 1536,18' "$base"
 magick "$base" "$resized" -geometry +268+300 -composite "$base"
 magick "$base" -fill '#151515' -font "$FONT" -pointsize 22 -draw "text 92,104 'SANMU AUDIO'" -pointsize 84 -draw "text 92,198 '$product'" -fill '#666660' -pointsize 21 -draw "text 95,238 '$blurb'" -fill '#d9d9d1' -draw 'rectangle 92,272 1444,276' -fill '#666660' -pointsize 18 -draw "text 94,968 'REAL INTERFACE / $mode'" -fill '#151515' -pointsize 18 -draw "text 1118,968 'VST3  ·  AU  ·  AAX'" "$OUT/单品/${product}-actual-ui.png"
}
make_single eq6 EQ6 '#27A88A' clean 'Dynamic parametric equalizer'; make_single comp Comp '#D4933D' vca 'Four compression behaviours'; make_single delay Delay '#3D6FD8' modern 'Time editor / stereo routing'; make_single reverb Reverb '#8170C8' plate 'Plate / Room / Hall / Chamber'
make_suite(){
 theme="$1"; bg="$2"; ink="$3"; suffix="$4"; base="$TMP/suite-$suffix-base.png"; magick -size 1536x1024 xc:"$bg" "$base"
 magick "$SHOT/eq6-clean-${suffix}-en.png" -resize '590x330!' "$TMP/a.png"; magick "$SHOT/comp-vca-${suffix}-en.png" -resize '590x330!' "$TMP/b.png"; magick "$SHOT/delay-modern-${suffix}-en.png" -resize '590x330!' "$TMP/c.png"; magick "$SHOT/reverb-plate-${suffix}-en.png" -resize '590x330!' "$TMP/d.png"
 magick "$base" -fill "$ink" -font "$FONT" -pointsize 22 -draw "text 90,94 'SANMU AUDIO  /  SUITE'" -pointsize 76 -draw "text 90,184 'Four dimensions. One workflow.'" -fill '#777777' -pointsize 20 -draw "text 94,224 '$theme'" -fill '#d0d0c8' -draw 'rectangle 92,260 1444,264' "$base"
 magick "$base" "$TMP/a.png" -geometry +92+310 -composite "$base"; magick "$base" "$TMP/b.png" -geometry +850+310 -composite "$base"; magick "$base" "$TMP/c.png" -geometry +92+650 -composite "$base"; magick "$base" "$TMP/d.png" -geometry +850+650 -composite "$base"
 magick "$base" -fill "$ink" -font "$FONT" -pointsize 18 -draw "text 94,1000 'EQ6   /   COMP   /   DELAY   /   REVERB'" "$OUT/套装/SanMu-Suite-actual-ui-$suffix.png"
}
make_suite 'LIGHT PAPER THEME' '#f4f4ef' '#151515' light; make_suite 'DARK NIGHT THEME' '#15191f' '#f4f4ef' dark
rm -rf "$TMP"
