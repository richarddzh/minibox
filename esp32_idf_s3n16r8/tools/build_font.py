"""Build Minibox's 24px, 2-bit font from OpenType or an existing MBF1 asset."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import struct


def firmware_characters():
    main = Path(__file__).resolve().parents[1] / "main"
    characters = set()
    for path in sorted(main.iterdir()):
        if path.suffix not in (".c", ".h"):
            continue
        text = path.read_text(encoding="utf-8")
        for literal in re.findall(r'"(?:\\.|[^"\\])*"', text):
            literal = re.sub(r"\\u([0-9a-fA-F]{4})|\\U([0-9a-fA-F]{8})",
                             lambda m: chr(int(m[1] or m[2], 16)), literal)
            characters.update(ord(char) for char in literal if ord(char) >= 128)
    return characters


def gb2312_hanzi():
    result = set()
    for lead in range(0xB0, 0xF8):
        for trail in range(0xA1, 0xFF):
            try:
                result.add(ord(bytes((lead, trail)).decode("gb2312")))
            except UnicodeDecodeError:
                continue
    assert len(result) == 6763
    return result


def read_bitmap(path):
    payload = path.read_bytes()
    magic, pixels, bits, count, index, data, total = struct.unpack_from(
        "<4sHHIIII", payload)
    if (magic != b"MBF1" or pixels != 24 or bits not in (2, 4) or
            index != 24 or data != 24 + count * 16 or total != len(payload)):
        raise ValueError("Invalid source bitmap font")
    glyphs = {}
    per_byte = 8 // bits
    mask = (1 << bits) - 1
    for i in range(count):
        cp, offset, advance, w, h, x, y = struct.unpack_from(
            "<IIHBBhh", payload, index + i * 16)
        if offset < data or offset + (w * h + per_byte - 1) // per_byte > total:
            raise ValueError(f"Invalid source glyph U+{cp:04X}")
        coverage = [
            ((payload[offset + j // per_byte] >>
              (8 - bits * (j % per_byte + 1))) & mask) * 255 // mask
            for j in range(w * h)
        ]
        glyphs[cp] = (advance, w, h, x, y, coverage)
    metadata = json.loads(path.with_suffix(".json").read_text(encoding="utf-8"))
    return glyphs, metadata


def read_opentype(path):
    from fontTools.ttLib import TTFont
    from PIL import Image, ImageDraw, ImageFont

    with TTFont(path) as source:
        cmap = source.getBestCmap()
        metadata = {
            "source_font": "Noto Sans CJK SC Regular",
            "source_version": source["name"].getDebugName(5),
            "copyright": source["name"].getDebugName(0),
            "source_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        }
    font = ImageFont.truetype(str(path), size=24)
    wanted = gb2312_hanzi() | firmware_characters() | {
        cp for start, end in [(0x20, 0x7F), (0xA0, 0x180), (0x2000, 0x2070),
                             (0x2190, 0x2194), (0x3000, 0x3040), (0xFF00, 0xFFF0)]
        for cp in range(start, end)
    }
    glyphs = {}
    for cp in sorted(wanted & cmap.keys()):
        char = chr(cp)
        left, top, right, bottom = font.getbbox(char, anchor="ls")
        w, h = right - left, bottom - top
        coverage = []
        if w and h:
            image = Image.new("L", (w, h))
            ImageDraw.Draw(image).text((-left, -top), char, font=font,
                                       fill=255, anchor="ls")
            coverage = list(image.tobytes())
        glyphs[cp] = (round(font.getlength(char)), w, h, left, top + 24, coverage)
    return glyphs, metadata


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument(
        "--output", type=Path,
        default=Path(__file__).resolve().parents[1] / "font_data" / "minibox_sans_24.bin",
    )
    args = parser.parse_args()
    glyphs, source_metadata = (
        read_bitmap(args.source) if args.source.suffix == ".bin"
        else read_opentype(args.source)
    )
    hanzi = gb2312_hanzi()
    required = hanzi | firmware_characters() | set(range(0x20, 0x7F))
    missing = sorted(required - glyphs.keys())
    if missing:
        raise ValueError("Missing required glyphs: " +
                         ", ".join(f"U+{cp:04X}" for cp in missing))
    codepoints = sorted(cp for cp in glyphs
                        if not 0x4E00 <= cp <= 0x9FFF or cp in required)
    header_size, entry_size = 24, 16
    data_offset = header_size + entry_size * len(codepoints)
    index, bitmaps = bytearray(), bytearray()
    for codepoint in codepoints:
        advance, width, height, x, y, coverage = glyphs[codepoint]
        if width * height > 4096:
            raise ValueError(f"U+{codepoint:04X} exceeds firmware glyph buffer")
        coverage = [(value * 3 + 127) // 255 for value in coverage]
        index.extend(struct.pack(
            "<IIHBBhh", codepoint, data_offset + len(bitmaps),
            advance, width, height, x, y,
        ))
        while len(coverage) % 4:
            coverage.append(0)
        bitmaps.extend((coverage[i] << 6) | (coverage[i + 1] << 4) |
                       (coverage[i + 2] << 2) | coverage[i + 3]
                       for i in range(0, len(coverage), 4))
    payload = struct.pack("<4sHHIIII", b"MBF1", 24, 2, len(codepoints),
                          header_size, data_offset, data_offset + len(bitmaps))
    payload += index + bitmaps
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(payload)
    metadata = {
        "name": "Minibox Sans 24",
        **{key: source_metadata[key] for key in
           ("source_font", "source_version", "copyright", "source_sha256")},
        "license": "SIL Open Font License 1.1; see OFL.txt",
        "font_sha256": hashlib.sha256(payload).hexdigest(),
        "pixels": 24, "alpha_bits": 2, "glyph_count": len(codepoints),
        "bytes": len(payload),
        "charset": "6763 GB2312 Hanzi + firmware strings + existing non-Hanzi",
        "gb2312_hanzi_count": 6763,
        "extra_hanzi": [chr(cp) for cp in codepoints
                        if 0x4E00 <= cp <= 0x9FFF and cp not in hanzi],
    }
    args.output.with_suffix(".json").write_text(
        json.dumps(metadata, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    print(json.dumps(metadata, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
