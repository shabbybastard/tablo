# UI fonts

`ui_font_14.c`, `ui_font_18.c` and `ui_font_28.c` are generated from
DejaVu Sans and cover the Latin UI characters, Cyrillic, and the small set of
punctuation/symbols used by the dashboard. `ui_font_14_bold.c` is the bold
14 px variant generated from DejaVu Sans Bold, used for the version label in
the top panel.

Source fonts:

- `/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf`
- `/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf` (bold 14)

Generator: `lv_font_conv` 1.5.3 (Node.js)

## Regenerate

From the repository root:

```sh
export PATH="$HOME/.local/bin:$PATH"

# Regular sizes: DejaVuSans.ttf
for size in 14 18 28; do
  npx --yes lv_font_conv@1.5.3 \
    --font /usr/share/fonts/truetype/dejavu/DejaVuSans.ttf \
    --size "$size" \
    --bpp 4 \
    --format lvgl \
    --no-compress \
    --lv-include lvgl.h \
    --lv-font-name "ui_font_$size" \
    -r 0x20-0x7F \
    -r 0x400-0x45F \
    -r 0xB0 \
    -r 0xB7 \
    -r 0x2022 \
    -r 0x224B \
    -r 0x2600-0x2603 \
    -r 0x2614 \
    -r 0x26A1 \
    -r 0x2744 \
    -r 0x2116 \
    -r 0x2190-0x2193 \
    -r 0x25B2 \
    -r 0x25BC \
    -r 0x2713 \
    -r 0x2717 \
    -r 0x26A0 \
    -o "src/ui/fonts/ui_font_$size.c"
done

# Bold 14 for the top-panel version label: DejaVuSans-Bold.ttf
npx --yes lv_font_conv@1.5.3 \
  --font /usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf \
  --size 14 \
  --bpp 4 \
  --format lvgl \
  --no-compress \
  --lv-include lvgl.h \
  --lv-font-name ui_font_14_bold \
  -r 0x20-0x7F \
  -r 0x400-0x45F \
  -r 0xB0 \
  -r 0x2022 \
  -r 0x2116 \
  -r 0x2190-0x2193 \
  -r 0x25B2 \
  -r 0x25BC \
  -r 0x2713 \
  -r 0x2717 \
  -r 0x26A0 \
  -o src/ui/fonts/ui_font_14_bold.c
```

The resulting `.c` files are committed so normal firmware/preview builds do not
need Node.js. `fonts.h` declares the fonts for C and C++ translation units.

## License

The fonts are derived from DejaVu Sans, which is based on Bitstream Vera.

Copyright (c) 2003 by Bitstream, Inc. All Rights Reserved.
Bitstream Vera is a trademark of Bitstream, Inc.
DejaVu changes are in public domain.

Permission is hereby granted, free of charge, to any person obtaining a copy of
the fonts accompanying this license ("Fonts") and associated documentation
files (the "Font Software"), to reproduce and distribute the Font Software,
including without limitation the rights to use, copy, merge, publish,
distribute, and/or sell copies of the Font Software, and to permit persons to
whom the Font Software is furnished to do so, subject to the following
conditions:

The above copyright and trademark notices and this permission notice shall be
included in all copies of one or more of the Font Software typefaces.

The Font Software may be modified, altered, or added to, and in particular the
designs of glyphs or characters in the Fonts may be modified and additional
glyphs or characters may be added to the Fonts, only if the fonts are renamed to
names not containing either the words "Bitstream" or the word "Vera".

This License becomes null and void to the extent applicable to Fonts or Font
Software that has been modified and is distributed under the "Bitstream Vera"
names.

The Font Software may be sold as part of a larger software package but no copy
of one or more of the Font Software typefaces may be sold by itself.

THE FONT SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO ANY WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF COPYRIGHT, PATENT,
TRADEMARK, OR OTHER RIGHT. IN NO EVENT SHALL BITSTREAM OR THE GNOME FOUNDATION
BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, INCLUDING ANY GENERAL,
SPECIAL, INDIRECT, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF THE USE OR INABILITY TO USE
THE FONT SOFTWARE OR FROM OTHER DEALINGS IN THE FONT SOFTWARE.

Except as contained in this notice, the names of Gnome, the Gnome Foundation,
and Bitstream Inc., shall not be used in advertising or otherwise to promote
the sale, use or other dealings in this Font Software without prior written
authorization from the Gnome Foundation or Bitstream Inc., respectively. For
further information, contact: fonts at gnome dot org.
