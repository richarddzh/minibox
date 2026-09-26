"""Convert a licensed OpenType font to Minibox's 24px, 4-bit alpha font file."""

import argparse
import hashlib
import json
from pathlib import Path
import struct

from fontTools.ttLib import TTFont
from PIL import Image, ImageDraw, ImageFont


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument(
        "--output", type=Path,
        default=Path(__file__).resolve().parents[1] / "font_data" / "minibox_sans_24.bin",
    )
    args = parser.parse_args()
    size = 24
    with TTFont(args.source) as source:
        cmap = source.getBestCmap()
        copyright_notice = source["name"].getDebugName(0)
        source_version = source["name"].getDebugName(5)
        ranges = [(0x20, 0x7F), (0xA0, 0x180), (0x2000, 0x2070),
                  (0x2190, 0x2194), (0x3000, 0x3040),
                  (0x4E00, 0xA000), (0xFF00, 0xFFF0)]
        codepoints = sorted({cp for start, end in ranges
                             for cp in range(start, end) if cp in cmap})
    font = ImageFont.truetype(str(args.source), size=size)
    header_size, entry_size = 24, 16
    data_offset = header_size + entry_size * len(codepoints)
    index, bitmaps = bytearray(), bytearray()
    for codepoint in codepoints:
        char = chr(codepoint)
        left, top, right, bottom = font.getbbox(char, anchor="ls")
        width, height = right - left, bottom - top
        if width * height > 4096:
            raise ValueError(f"U+{codepoint:04X} exceeds firmware glyph buffer")
        coverage = []
        if width and height:
            image = Image.new("L", (width, height))
            ImageDraw.Draw(image).text((-left, -top), char, font=font,
                                       fill=255, anchor="ls")
            coverage = [(value * 15 + 127) // 255 for value in image.tobytes()]
        index.extend(struct.pack(
            "<IIHBBhh", codepoint, data_offset + len(bitmaps),
            round(font.getlength(char)), width, height, left, top + size,
        ))
        if len(coverage) % 2:
            coverage.append(0)
        bitmaps.extend((coverage[i] << 4) | coverage[i + 1]
                       for i in range(0, len(coverage), 2))
    required = "屏幕测试中文显示摇杆按键方向计数引脚左右上下松开"
    missing = [char for char in required if ord(char) not in codepoints]
    if missing:
        raise ValueError(f"Missing required glyphs: {missing}")
    payload = struct.pack("<4sHHIIII", b"MBF1", size, 4, len(codepoints),
                          header_size, data_offset, data_offset + len(bitmaps))
    payload += index + bitmaps
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(payload)
    metadata = {
        "name": "Minibox Sans 24",
        "source_font": "Noto Sans CJK SC Regular",
        "source_version": source_version,
        "copyright": copyright_notice,
        "license": "SIL Open Font License 1.1; see OFL.txt",
        "source_sha256": hashlib.sha256(args.source.read_bytes()).hexdigest(),
        "font_sha256": hashlib.sha256(payload).hexdigest(),
        "pixels": size, "alpha_bits": 4, "glyph_count": len(codepoints),
        "bytes": len(payload),
        "ranges": [[hex(start), hex(end - 1)] for start, end in ranges],
    }
    args.output.with_suffix(".json").write_text(
        json.dumps(metadata, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    print(json.dumps(metadata, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
