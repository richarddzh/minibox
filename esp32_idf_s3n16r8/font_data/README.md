# Minibox Sans 24

`minibox_sans_24.bin` is a 24-pixel, 4-bit antialiased bitmap derivative of
**Noto Sans CJK SC Regular 2.004**. It contains 21,524 glyphs (6,411,342 bytes),
including English, the supported CJK Unified Ideographs block, punctuation,
Latin extensions and arrows. Unsupported characters produce an explicit error.

Copyright 2014-2021 Adobe (http://www.adobe.com/).
The source font and this derivative are distributed under the
**SIL Open Font License 1.1**, included in `OFL.txt`.
The derivative is named Minibox Sans 24; it is not an official Noto font release.

Source:
https://github.com/notofonts/noto-cjk/blob/f8d157532fbfaeda587e826d4cd5b21a49186f7c/Sans/OTF/SimplifiedChinese/NotoSansCJKsc-Regular.otf

Source SHA-256:
`2c76254f6fc379fddfce0a7e84fb5385bb135d3e399294f6eeb6680d0365b74b`

`minibox_sans_24.json` records the source metadata, ranges and output hash.
The converted font, metadata and license are intentionally checked in so
normal firmware builds need neither a font download nor Pillow/fontTools.
ESP-IDF packs this directory into the `fonts` SPIFFS partition automatically.

To regenerate the asset only, install Pillow and fontTools in a suitable existing
Python environment and run:

```powershell
python .\tools\build_font.py path\to\NotoSansCJKsc-Regular.otf
```

Do not commit the downloaded source OTF or Python environment.
The binary format is little-endian: a 24-byte `MBF1` header, sorted 16-byte
Unicode index entries, then packed row-major alpha values (high nibble first).
At startup the firmware sequentially loads the file into PSRAM. Rendering then
reads memory only, avoiding slow SPIFFS seeks and repeated flash access.
