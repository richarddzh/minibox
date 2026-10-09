"""Annotate KiCad board PNGs with material legends and board dimensions."""

import argparse
import json
from pathlib import Path
import subprocess
from tempfile import NamedTemporaryFile

from PIL import Image, ImageChops, ImageDraw, ImageFont


HERE = Path(__file__).resolve().parent


def read_geometry(kicad_python):
    script = """
import json
import sys
import pcbnew as pcb
board = pcb.LoadBoard(sys.argv[1])
edges = [p for item in board.GetDrawings() if item.GetLayer() == pcb.Edge_Cuts
         for p in (item.GetStart(), item.GetEnd())]
holes = []
for footprint in board.GetFootprints():
    if not footprint.GetReference().startswith("H"):
        continue
    for pad in footprint.Pads():
        if pad.GetAttribute() != pcb.PAD_ATTRIB_NPTH:
            raise RuntimeError("Mounting holes must be non-plated")
        if pad.GetDrillSize().x != pad.GetDrillSize().y:
            raise RuntimeError("Mounting holes must be circular")
        p = pad.GetPosition()
        holes.append(dict(reference=footprint.GetReference(), x=pcb.ToMM(p.x),
                          y=pcb.ToMM(p.y), diameter=pcb.ToMM(pad.GetDrillSize().x)))
print(json.dumps(dict(left=min(pcb.ToMM(p.x) for p in edges),
                      right=max(pcb.ToMM(p.x) for p in edges),
                      top=min(pcb.ToMM(p.y) for p in edges),
                      bottom=max(pcb.ToMM(p.y) for p in edges),
                      holes=sorted(holes, key=lambda h: h["reference"]))))
"""
    process = subprocess.run(
        [str(kicad_python), "-c", script, str(HERE / "minibox-carrier.kicad_pcb")],
        check=True, stdout=subprocess.PIPE, text=True)
    geometry = json.loads(process.stdout)
    holes = geometry["holes"]
    if len(holes) != 4 or len({h["x"] for h in holes}) != 2 or len({h["y"] for h in holes}) != 2:
        raise ValueError("Dimension annotations require four rectangular mounting-hole centers")
    if len({(h["x"], h["y"]) for h in holes}) != 4:
        raise ValueError("Mounting-hole centers must be distinct")
    return geometry


def add_dimensions(image, side, geometry, font_path, scale):
    red, green, blue = image.split()
    mask = ImageChops.multiply(
        ImageChops.subtract(green, red).point(lambda v: 255 if v > 10 else 0),
        ImageChops.subtract(green, blue).point(lambda v: 255 if v > 4 else 0))
    bounds = mask.getbbox()
    if bounds is None:
        raise ValueError("Cannot locate the green board outline in the orthographic render")
    board_width = geometry["right"] - geometry["left"]
    board_height = geometry["bottom"] - geometry["top"]
    left, top, right, bottom = bounds
    if abs((right - left) / (bottom - top) - board_width / board_height) > 0.02:
        raise ValueError("Render bounds do not match board dimensions; use top/bottom orthographic views")
    margin = round(180 * scale)
    result = Image.new("RGB", (image.width + 2 * margin, image.height + 2 * margin), "#f7f8fa")
    result.paste(image, (margin, margin))
    left, top, right, bottom = (v + margin for v in bounds)
    draw = ImageDraw.Draw(result)
    font = ImageFont.truetype(str(font_path), round(34 * scale))
    small = ImageFont.truetype(str(font_path), round(29 * scale))
    color = "#225f9c"
    stroke = max(1, round(3 * scale))
    arrow = round(12 * scale)

    def label(position, value, anchor="mm", selected_font=font):
        box = draw.textbbox(position, value, font=selected_font, anchor=anchor)
        pad = round(7 * scale)
        draw.rectangle((box[0] - pad, box[1] - pad, box[2] + pad, box[3] + pad), fill="#f7f8fa")
        draw.text(position, value, font=selected_font, fill=color, anchor=anchor)

    def horizontal(x1, x2, y, origin_y, value):
        for x in (x1, x2):
            draw.line((x, origin_y, x, y + arrow), fill=color, width=stroke)
        draw.line((x1, y, x2, y), fill=color, width=stroke)
        draw.polygon([(x1, y), (x1 + arrow, y - arrow / 3), (x1 + arrow, y + arrow / 3)], fill=color)
        draw.polygon([(x2, y), (x2 - arrow, y - arrow / 3), (x2 - arrow, y + arrow / 3)], fill=color)
        label(((x1 + x2) / 2, y), value)

    def vertical(y1, y2, x, origin_x, value):
        for y in (y1, y2):
            draw.line((origin_x, y, x + arrow, y), fill=color, width=stroke)
        draw.line((x, y1, x, y2), fill=color, width=stroke)
        draw.polygon([(x, y1), (x - arrow / 3, y1 + arrow), (x + arrow / 3, y1 + arrow)], fill=color)
        draw.polygon([(x, y2), (x - arrow / 3, y2 - arrow), (x + arrow / 3, y2 - arrow)], fill=color)
        label((x, (y1 + y2) / 2), value)

    horizontal(left, right, top - round(110 * scale), top, f"{board_width:g} mm")
    vertical(top, bottom, left - round(105 * scale), left, f"{board_height:g} mm")
    centers = []
    for hole in geometry["holes"]:
        u = (hole["x"] - geometry["left"]) / board_width
        if side == "back":
            u = 1 - u
        x = left + u * (right - left)
        y = top + (hole["y"] - geometry["top"]) / board_height * (bottom - top)
        centers.append((x, y))
        upper = hole["y"] < (geometry["top"] + geometry["bottom"]) / 2
        label_y = top - round(40 * scale) if upper else bottom + round(40 * scale)
        draw.line((x, y, x, label_y), fill=color, width=stroke)
        label((x, label_y), f'{hole["reference"]} Ø{hole["diameter"]:g}', selected_font=small)
    x1, x2 = min(x for x, _ in centers), max(x for x, _ in centers)
    y1, y2 = min(y for _, y in centers), max(y for _, y in centers)
    holes = geometry["holes"]
    dx = max(h["x"] for h in holes) - min(h["x"] for h in holes)
    dy = max(h["y"] for h in holes) - min(h["y"] for h in holes)
    horizontal(x1, x2, bottom + round(115 * scale), y2, f"孔中心距 {dx:g} mm")
    vertical(y1, y2, right + round(100 * scale), x2, f"孔距 {dy:g} mm")
    return result


def add_legend(side, font_path, geometry):
    source = HERE / "renders" / f"minibox-carrier-{side}.png"
    with Image.open(source) as image:
        image.load()
        board_image = image.convert("RGB")
    scale = board_image.height / 2200
    board_image = add_dimensions(board_image, side, geometry, font_path, scale)
    width, height = board_image.size
    panel_width = round(880 * scale)
    result = Image.new("RGB", (width + panel_width, height), "#f7f8fa")
    result.paste(board_image, (0, 0))
    draw = ImageDraw.Draw(result)
    title = ImageFont.truetype(str(font_path), round(48 * scale))
    heading = ImageFont.truetype(str(font_path), round(34 * scale))
    body = ImageFont.truetype(str(font_path), round(29 * scale))
    x = width + round(50 * scale)
    y = round(100 * scale)
    draw.text((x, y), "正面 · 元件插装面" if side == "front" else "背面 · 手工焊接面",
              font=title, fill="#18212b")
    draw.text((x, y + round(85 * scale)), "颜色图例 / LEGEND", font=heading, fill="#344253")
    draw.text((x, y + round(150 * scale)), "四层：F.Cu / In1 GND / In2 / B.Cu",
              font=body, fill="#344253")
    rows = [
        ("#50543b", "阻焊覆盖的铜",
         "橄榄绿色的铜皮和走线。\n大面积铜皮主要连接 GND。"),
        ("#214936", "无铜区 / 铜间隙",
         "深绿区域及走线周围的隔离带。\n天线区与螺丝孔周边也禁铜。"),
        ("#eccb22", "露出的可焊金属",
         "金色焊盘和镀通孔铜环。\n实际颜色取决于表面处理。"),
        ("#ffffff", "丝印",
         "白色引脚名称、GPIO 和外形标记。\n不导电；圆形轮廓不是切割线。"),
        ("#303030", "连接器塑料外壳",
         "黑灰色为正面排母模型。\n载板上未展示完整功能模块。"),
        ("#4bc77b", "接线端子外壳",
         "WJ500V未附精确3D模型。\n孔径1.50 mm，外形以封装图为准。"),
        ("#141414", "钻孔",
         "小孔是焊接孔或过孔。\n四个大安装孔为非镀通孔。"),
        ("#c8c7dd", "背景 / 光照 / 阴影",
         "灰紫色属于渲染背景及光照。\n不是 PCB 材料，也不是铜层。"),
    ]
    y += round(250 * scale)
    for color, label, description in rows:
        box = (x, y + round(8 * scale), x + round(42 * scale), y + round(50 * scale))
        draw.rounded_rectangle(box, radius=round(5 * scale), fill=color, outline="#aeb6c0")
        tx = x + round(65 * scale)
        draw.text((tx, y), label, font=heading, fill="#18212b")
        draw.multiline_text((tx, y + round(58 * scale)), description,
                            font=body, fill="#536170", spacing=round(9 * scale))
        y += round(185 * scale)
    board_width = geometry["right"] - geometry["left"]
    board_height = geometry["bottom"] - geometry["top"]
    holes = geometry["holes"]
    dx = max(h["x"] for h in holes) - min(h["x"] for h in holes)
    dy = max(h["y"] for h in holes) - min(h["y"] for h in holes)
    diameters = " / ".join(f'Ø{d:g}' for d in sorted({h["diameter"] for h in holes}))
    draw.text((x, y), "机械尺寸 / mm", font=heading, fill="#18212b")
    draw.multiline_text((x, y + round(60 * scale)),
                        f"板外形：{board_width:g} × {board_height:g}\n"
                        f"四个安装孔：{diameters}，非镀通孔\n"
                        f"孔中心距：横向 {dx:g}，纵向 {dy:g}\n"
                        "J3 麦克风：两个独立的 1×3 母座\n"
                        "母座排距 7.62 mm，仍须实物核对",
                        font=body, fill="#536170", spacing=round(13 * scale))
    note = ("背面观察时，左右顺序与正面相反。"
            if side == "back" else "所有连接器从正面插装，在背面焊接。")
    draw.text((x, height - round(210 * scale)), note, font=body, fill="#344253")
    draw.multiline_text((x, height - round(145 * scale)),
                        "尺寸取自 PCB，像素引线仅为示意。\n蓝色引线/图例不属于丝印或 Gerber。",
                        font=body, fill="#536170", spacing=round(9 * scale))
    output = source.with_stem(source.stem + "-legend")
    with NamedTemporaryFile(dir=output.parent, suffix=".png", delete=False) as staging:
        temporary = Path(staging.name)
    try:
        result.save(temporary, dpi=(200, 200))
        temporary.replace(output)
    finally:
        temporary.unlink(missing_ok=True)
    print(output)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--font", type=Path, default=Path(r"C:\Windows\Fonts\msyh.ttc"))
    parser.add_argument("--kicad-python", type=Path,
                        default=Path(r"C:\Program Files\KiCad\10.0\bin\python.exe"))
    args = parser.parse_args()
    if not args.font.is_file():
        parser.error(f"Chinese font not found: {args.font}")
    if not args.kicad_python.is_file():
        parser.error(f"KiCad Python not found: {args.kicad_python}")
    geometry = read_geometry(args.kicad_python)
    for side in ("front", "back"):
        add_legend(side, args.font, geometry)


if __name__ == "__main__":
    main()
