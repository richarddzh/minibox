# Minibox Sans 24

`minibox_sans_24.bin` is a 24-pixel, 2-bit (four-level) antialiased derivative of
**Noto Sans CJK SC Regular 2.004**. It contains 7,311 glyphs (1,123,256 bytes):
all 6,763 GB2312 Hanzi, firmware string characters, English, punctuation,
Latin extensions and arrows. This is not the complete GB2312 symbol repertoire.
The default LVGL UI substitutes `?` for unsupported characters; the optional
hardware-test renderer reports missing glyphs as errors.

Copyright 2014-2021 Adobe (http://www.adobe.com/).
The source font and this derivative are distributed under the
**SIL Open Font License 1.1**, included in `OFL.txt`.
The derivative is named Minibox Sans 24; it is not an official Noto font release.

Source:
https://github.com/notofonts/noto-cjk/blob/f8d157532fbfaeda587e826d4cd5b21a49186f7c/Sans/OTF/SimplifiedChinese/NotoSansCJKsc-Regular.otf

Source SHA-256:
`2c76254f6fc379fddfce0a7e84fb5385bb135d3e399294f6eeb6680d0365b74b`

`minibox_sans_24.json` records the source metadata, character selection and output hash.
The converted font, metadata and license are intentionally checked in so
normal firmware builds need neither a font download nor Pillow/fontTools.
ESP-IDF packs this directory into the `fonts` SPIFFS partition automatically.

To regenerate the asset only, install Pillow and fontTools in a suitable existing
Python environment and run:

```powershell
python .\tools\build_font.py path\to\NotoSansCJKsc-Regular.otf
```

An existing MBF1 `.bin` can also be the input, without Pillow/fontTools:

```powershell
python .\tools\build_font.py .\font_data\minibox_sans_24.bin
```

Bitmap input preserves existing metrics and non-Hanzi glyphs. It cannot add
characters absent from that input: use the original OTF if new firmware strings
require additional characters. The generator rejects missing required glyphs.
Quantization maps alpha to the nearest of 0, 85, 170 and 255.

Do not commit the downloaded source OTF or Python environment.
The binary format is little-endian: a 24-byte `MBF1` header, sorted 16-byte
Unicode index entries, then packed row-major alpha values (most significant
bits first, four pixels per byte at 2-bit depth, padding each glyph separately).
Both firmware renderers also accept the previous 4-bit format.
At startup the firmware sequentially loads the file into PSRAM. Rendering then
reads memory only, avoiding slow SPIFFS seeks and repeated flash access.
