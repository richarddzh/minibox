"""Generate the provisional Minibox carrier in KiCad 10 using its bundled Python."""

from pathlib import Path

import pcbnew as pcb


HERE = Path(__file__).resolve().parent
SOCKETS = Path(r"C:\Program Files\KiCad\10.0\share\kicad\footprints\Connector_PinSocket_2.54mm.pretty")
BOARD_FILE = HERE / "minibox-carrier.kicad_pcb"


def point(x, y):
    return pcb.VECTOR2I(pcb.FromMM(x), pcb.FromMM(y))


board = pcb.BOARD()
board.SetCopperLayerCount(2)
nets = {}


def net(name):
    if name not in nets:
        info = pcb.NETINFO_ITEM(board, name)
        board.Add(info)
        nets[name] = info
    return nets[name]


def socket(reference, count, x, y, labels, value):
    assert len(labels) == count
    name = f"PinSocket_1x{count:02d}_P2.54mm_Vertical"
    footprint = pcb.FootprintLoad(str(SOCKETS), name)
    if footprint is None:
        raise RuntimeError(f"Missing KiCad footprint: {name}")
    footprint.SetFPIDAsString(f"Connector_PinSocket_2.54mm:{name}")
    footprint.SetReference(reference)
    footprint.SetValue(value)
    footprint.SetPosition(point(x, y))
    footprint.Reference().SetPosition(point(x + 2, y - 2))
    footprint.Value().SetVisible(False)
    for pad in footprint.Pads():
        label = labels[int(pad.GetNumber()) - 1]
        if label:
            pad.SetNet(net(label))
    board.Add(footprint)
    return footprint


def text(value, x, y, size=1):
    item = pcb.PCB_TEXT(board)
    item.SetText(value)
    item.SetPosition(point(x, y))
    item.SetTextSize(point(size, size))
    item.SetTextThickness(pcb.FromMM(0.16))
    item.SetLayer(pcb.F_SilkS)
    board.Add(item)


def line(x1, y1, x2, y2, layer, width):
    item = pcb.PCB_SHAPE(board)
    item.SetShape(pcb.SHAPE_T_SEGMENT)
    item.SetStart(point(x1, y1))
    item.SetEnd(point(x2, y2))
    item.SetLayer(layer)
    item.SetWidth(pcb.FromMM(width))
    board.Add(item)


# The reference photo shows 22 pins per side, viewed from the component side
# with both USB sockets at the bottom. Pin 1 is at the top of each column.
socket("J1", 22, 27, 22, [
    "3V3", "3V3", None, "GPIO4", "GPIO5", "GPIO6", "AMP_SD",
    "AUDIO_DIN", "I2S_BCLK", "I2S_WS", "MIC_SD", "AMP_GAIN",
    "GPIO3", "GPIO46", "LCD_BL", "LCD_SCK", "LCD_MOSI",
    "LCD_DC", "LCD_RST", "LCD_CS", "5V_SW", "GND",
], "ESP32-S3 LEFT - USB AT BOTTOM")
socket("J2", 22, 52.4, 22, [
    "GND", "GPIO43", "GPIO44", "RTC_SDA", "RTC_SCL",
    "BUTTON3", "RECORD", "BUTTON1", "GPIO39", "GPIO38",
    None, None, None, "GPIO0", "GPIO45", "GPIO48", "GPIO47",
    "GPIO21", None, None, "GND", "GND",
], "ESP32-S3 RIGHT - USB AT BOTTOM")

# INMP441 circular module: actual mechanical pin arrangement must be measured.
socket("J3", 6, 12, 32,
       ["3V3", "GND", "I2S_BCLK", "I2S_WS", "MIC_SD", "GND"],
       "INMP441 VDD GND SCK WS SD L/R")
socket("J4", 7, 80, 53,
       ["I2S_WS", "I2S_BCLK", "AUDIO_DIN", "AMP_GAIN",
        "AMP_SD", "GND", "5V_SW"],
       "MAX98357A LRC BCLK DIN GAIN SD GND VIN")
socket("J5", 6, 98, 35,
       [None, None, "RTC_SDA", "RTC_SCL", "3V3", "GND"],
       "PCF8563T CLK INT SDA SCL VCC GND")

# A generic 10-position *interface* for the ST7796 screen, not a verified
# mating footprint. Its supply is intentionally left isolated.
socket("J6", 10, 72, 10,
       ["LCD_VCC", "GND", "LCD_CS", "LCD_RST", "LCD_DC",
        "LCD_MOSI", "LCD_SCK", "LCD_BL", None, None],
       "ST7796 PROVISIONAL HEADER")
socket("J7", 3, 98, 10, ["5V_IN", "5V_SW", "GND"],
       "SWITCH IN LOAD GND - VERIFY ORDER")
socket("J8", 2, 98, 78, ["5V_IN", "GND"],
       "EXTERNAL 5V INPUT + GND")
socket("J10", 1, 63, 87, ["LCD_VCC"], "LCD VCC - WIRE AFTER VOLTAGE CHECK")

# The amplifier module exposes speaker outputs on its own screw terminal.
# J6's VCC is deliberately isolated pending module verification.
for label, x, y, size in [
    ("ESP32-S3", 40, 18, 1), ("USB v", 40, 81, 1),
    ("MIC", 12, 29, 1), ("AMP", 80, 50, 1),
    ("RTC", 98, 32, 1), ("LCD", 72, 7, 1),
    ("SW", 98, 7, 1), ("5V IN", 103, 75, 1),
    ("LCD VCC?", 63, 92, 1), ("SPK: USE AMP TERMINAL", 82, 96, 0.85),
    ("PROTOTYPE - VERIFY PIN PITCH AND ORDER", 57, 101, 0.85),
]:
    text(label, x, y, size)

for start, end in [
    ((3, 3), (112, 3)),
    ((112, 3), (112, 104)),
    ((112, 104), (3, 104)),
    ((3, 104), (3, 3)),
]:
    line(*start, *end, pcb.Edge_Cuts, 0.05)

pcb.SaveBoard(str(BOARD_FILE), board)
print(BOARD_FILE)
