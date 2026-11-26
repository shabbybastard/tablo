#!/usr/bin/env bash
# Regenerate the Cyrillic UI fonts. Requires Node.js + npx (one-time).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
FONT_DIR="/usr/share/fonts/truetype/dejavu"
FONT="$FONT_DIR/DejaVuSans.ttf"
FONT_BOLD="$FONT_DIR/DejaVuSans-Bold.ttf"
for font in "$FONT" "$FONT_BOLD"; do
  if [ ! -f "$font" ]; then
    echo "${font##*/} not found at $font" >&2
    exit 1
  fi
done
export PATH="$HOME/.local/bin:$PATH"

gen_font() {
  local font="$1" size="$2" name="$3"
  npx --yes lv_font_conv@1.5.3 \
    --font "$font" \
    --size "$size" \
    --bpp 4 \
    --format lvgl \
    --no-compress \
    --lv-include lvgl.h \
    --lv-font-name "$name" \
    -r 0x20-0x7F \
    -r 0x400-0x45F \
    -r 0xB0 \
    -r 0xB7 \
    -r 0x2022 \
    -r 0x224B \
    -r 0x2600-0x2603 \
    -r 0x2614 \
    -r 0x26A1 \
    -r 0x2699 \
    -r 0x2744 \
    -r 0x2116 \
    -r 0x2190-0x2193 \
    -r 0x25B2 \
    -r 0x25BC \
    -r 0x2713 \
    -r 0x2717 \
    -r 0x26A0 \
    -o "$ROOT/src/ui/fonts/$name.c"
}

for size in 14 18 28; do
  gen_font "$FONT" "$size" "ui_font_$size"
done
gen_font "$FONT_BOLD" 14 ui_font_14_bold
echo "Fonts regenerated in src/ui/fonts/"
