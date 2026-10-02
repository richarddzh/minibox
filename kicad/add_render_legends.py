"""Add Chinese material legends beside the exported KiCad board PNGs."""

import argparse
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


HERE = Path(__file__).resolve().parent


def add_legend(side, font_path):
    source = HERE / "renders" / f"minibox-carrier-{side}.png"
    with Image.open(source) as image:
        image.load()
        board_image = image.convert("RGB")
    width, height = board_image.size
    scale = height / 2200
    panel_width = round(800 * scale)
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
    draw.text((x, y + round(150 * scale)), "两层铜：F.Cu + B.Cu，无内层铜",
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
         "亮绿色为正面接线端子模型。\n银白色部分为金属螺丝。"),
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
        y += round(200 * scale)
    note = ("背面观察时，左右顺序与正面相反。"
            if side == "back" else "所有连接器从正面插装，在背面焊接。")
    draw.text((x, height - round(210 * scale)), note, font=body, fill="#344253")
    draw.multiline_text((x, height - round(145 * scale)),
                        "颜色用于区分材料，不代表层数。\n图例不属于 PCB 丝印或 Gerber。",
                        font=body, fill="#536170", spacing=round(9 * scale))
    output = source.with_stem(source.stem + "-legend")
    result.save(output, dpi=(200, 200))
    print(output)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--font", type=Path, default=Path(r"C:\Windows\Fonts\msyh.ttc"))
    args = parser.parse_args()
    if not args.font.is_file():
        parser.error(f"Chinese font not found: {args.font}")
    for side in ("front", "back"):
        add_legend(side, args.font)


if __name__ == "__main__":
    main()
