"""Generate the provisional Minibox carrier in KiCad 10 using its bundled Python."""

from pathlib import Path
from math import cos, pi, sin

import pcbnew as pcb


HERE = Path(__file__).resolve().parent
SOCKETS = Path(r"C:\Program Files\KiCad\10.0\share\kicad\footprints\Connector_PinSocket_2.54mm.pretty")
CUSTOM = HERE / "Minibox.pretty"
MOUNTING = Path(r"C:\Program Files\KiCad\10.0\share\kicad\footprints\MountingHole.pretty")
TESTPOINTS = Path(r"C:\Program Files\KiCad\10.0\share\kicad\footprints\TestPoint.pretty")
BOARD_FILE = HERE / "minibox-carrier.kicad_pcb"


def point(x, y):
    return pcb.VECTOR2I(pcb.FromMM(x), pcb.FromMM(y))


board = pcb.BOARD()
board.SetCopperLayerCount(4)
board.GetDesignSettings().m_SolderMaskExpansion = pcb.FromMM(0.05)
board.GetDesignSettings().m_SolderMaskMinWidth = pcb.FromMM(0.1)
nets = {}


def net(name):
    if name not in nets:
        info = pcb.NETINFO_ITEM(board, name)
        board.Add(info)
        nets[name] = info
    return nets[name]


def socket(reference, count, x, y, labels, value, orientation=0):
    assert len(labels) == count
    name = f"PinSocket_1x{count:02d}_P2.54mm_Vertical"
    footprint = pcb.FootprintLoad(str(SOCKETS), name)
    if footprint is None:
        raise RuntimeError(f"Missing KiCad footprint: {name}")
    footprint.SetFPIDAsString(f"Connector_PinSocket_2.54mm:{name}")
    footprint.SetReference(reference)
    footprint.SetValue(value)
    footprint.SetOrientationDegrees(orientation)
    footprint.SetPosition(point(x, y))
    footprint.Reference().SetPosition(point(x + 2, y - 2))
    footprint.Reference().SetVisible(False)
    footprint.Value().SetVisible(False)
    for pad in footprint.Pads():
        label = labels[int(pad.GetNumber()) - 1]
        if label:
            pad.SetNet(net(label))
    board.Add(footprint)
    return footprint


def double_socket(reference, x, y, labels, value):
    name = "INMP441_2x03_Row7.62mm"
    footprint = pcb.FootprintLoad(str(CUSTOM), name)
    if footprint is None:
        raise RuntimeError(f"Missing KiCad footprint: {name}")
    footprint.SetFPIDAsString(f"Minibox:{name}")
    footprint.SetReference(reference)
    footprint.SetValue(value)
    footprint.SetPosition(point(x, y))
    footprint.Reference().SetPosition(point(x + 3, y - 2))
    footprint.Reference().SetVisible(False)
    footprint.Value().SetVisible(False)
    for pad in footprint.Pads():
        pad.SetNet(net(labels[int(pad.GetNumber()) - 1]))
    board.Add(footprint)
    return footprint


def mounting_hole(reference, x, y):
    name = "MountingHole_3mm"
    footprint = pcb.FootprintLoad(str(MOUNTING), name)
    if footprint is None:
        raise RuntimeError(f"Missing KiCad footprint: {name}")
    footprint.SetFPIDAsString(f"MountingHole:{name}")
    footprint.SetReference(reference)
    footprint.SetPosition(point(x, y))
    footprint.Reference().SetVisible(False)
    footprint.Value().SetVisible(False)
    board.Add(footprint)
    for layer in (pcb.F_Cu, pcb.In1_Cu, pcb.In2_Cu, pcb.B_Cu):
        keepout = pcb.ZONE(board)
        keepout.SetLayer(layer)
        keepout.SetIsRuleArea(True)
        keepout.SetDoNotAllowPads(False)
        keepout.SetDoNotAllowTracks(True)
        keepout.SetDoNotAllowVias(True)
        keepout.SetDoNotAllowZoneFills(True)
        keepout.Outline().NewOutline()
        for index in range(32):
            angle = 2 * pi * index / 32
            keepout.Outline().Append(
                pcb.FromMM(x + 3.5 * cos(angle)),
                pcb.FromMM(y + 3.5 * sin(angle)))
        board.Add(keepout)


def test_point(reference, x, y):
    name = "TestPoint_Pad_D1.0mm"
    footprint = pcb.FootprintLoad(str(TESTPOINTS), name)
    if footprint is None:
        raise RuntimeError(f"Missing KiCad footprint: {name}")
    footprint.SetFPIDAsString(f"TestPoint:{name}")
    footprint.SetReference(reference)
    footprint.SetValue("3V3")
    footprint.SetPosition(point(x, y))
    footprint.Reference().SetVisible(False)
    footprint.Value().SetVisible(False)
    next(iter(footprint.Pads())).SetNet(net("3V3"))
    board.Add(footprint)


def text(value, x, y, size=1):
    item = pcb.PCB_TEXT(board)
    item.SetText(value)
    item.SetPosition(point(x, y))
    item.SetTextSize(point(size, size))
    item.SetTextThickness(pcb.FromMM(0.16))
    item.SetLayer(pcb.F_SilkS)
    board.Add(item)


def envelope(x1, y1, x2, y2):
    item = pcb.PCB_SHAPE(board)
    item.SetShape(pcb.SHAPE_T_RECT)
    item.SetStart(point(x1, y1))
    item.SetEnd(point(x2, y2))
    item.SetLayer(pcb.Dwgs_User)
    item.SetWidth(pcb.FromMM(0.12))
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
socket("J1", 22, 24, 12, [
    "3V3", "3V3", None, "GPIO4", "GPIO5", "GPIO6", "AMP_SD",
    "AUDIO_DIN", "I2S_BCLK", "I2S_WS", "MIC_SD", "AMP_GAIN",
    "GPIO3", "GPIO46", "LCD_BL", "LCD_SCK", "LCD_MOSI",
    "LCD_DC", "LCD_RST", "LCD_CS", "5V_SW", "GND",
], "ESP32-S3 LEFT - USB AT BOTTOM")
socket("J2", 22, 49.4, 12, [
    "GND", "GPIO43", "GPIO44", "RTC_SDA", "RTC_SCL",
    "BUTTON3", "RECORD", "BUTTON1", "GPIO39", "GPIO38",
    None, None, None, "GPIO0", "GPIO45", "GPIO48", "GPIO47",
    "GPIO21", None, None, "GND", "GND",
], "ESP32-S3 RIGHT - USB AT BOTTOM")

# Standard two-row, three-pin socket. KiCad numbers alternate by column:
# 1 2 / 3 4 / 5 6. Verify the actual microphone's pin order before insertion.
double_socket("J3", 50, 90,
              ["3V3", "GND", "I2S_BCLK", "I2S_WS", "MIC_SD", "GND"],
              "INMP441 2x3 VDD GND / SCK WS / SD LR")
socket("J4", 7, 68, 39,
       ["I2S_WS", "I2S_BCLK", "AUDIO_DIN", "AMP_GAIN",
        "AMP_SD", "GND", "5V_SW"],
       "MAX98357A LRC BCLK DIN GAIN SD GND VIN")
envelope(64, 35, 91, 59)  # Body and on-module speaker screw terminal.
socket("J5", 6, 80, 10,
       [None, None, "RTC_SDA", "RTC_SCL", "3V3", "GND"],
       "PCF8563T CLK INT SDA SCL VCC GND")

# The owner confirmed the screen's 14-pin header follows assets/tft_spi.jpg.
# Touch and readback pins are unused by the current firmware.
socket("J6", 14, 110, 10,
       ["3V3", "GND", "LCD_CS", "LCD_RST", "LCD_DC",
        "LCD_MOSI", "LCD_SCK", "LCD_BL", None, None, None,
        None, None, None],
       "ST7796 14-PIN DISPLAY HEADER")
socket("J7", 3, 105, 88, ["5V_IN", "5V_SW", "GND"],
       "SWITCH IN LOAD GND - VERIFY ORDER")
socket("J8", 2, 97, 10, ["5V_IN", "GND"],
       "EXTERNAL 5V INPUT + GND")
test_point("TP1", 65, 104)
socket("J11", 5, 72, 90,
       ["BUTTON1", "RECORD", "BUTTON3", "3V3", "GND"],
       "THREE-KEY CABLE KeyA KeyB KeyC Vcc Gnd", orientation=90)
socket("J12", 5, 12, 90,
       ["GND", "3V3", "GPIO4", "GPIO5", "GPIO6"],
       "JOYSTICK CABLE G V X Y K", orientation=90)
for reference, x, y in (
    ("H1", 8, 8), ("H2", 118, 8),
    ("H3", 8, 110), ("H4", 118, 110),
):
    mounting_hole(reference, x, y)

# The amplifier module exposes speaker outputs on its own screw terminal.
for label, x, y, size in [
    ("ESP32-S3", 36.7, 8, 1), ("USB v", 36.7, 73, 1),
    ("MIC 2x3", 50, 78, 1), ("AMP", 69, 32, 1),
    ("RTC", 80, 7, 1), ("LCD", 110, 53, 1),
    ("SW", 105, 85, 1), ("5V IN", 97, 7, 1),
    ("3V3", 65, 101, 1), ("KEYS", 77, 81, 1),
    ("JOY", 17, 81, 1), ("SPK: USE AMP TERMINAL", 80, 64, 1),
    ("PROTOTYPE - VERIFY PIN PITCH AND ORDER", 63, 112, 1),
]:
    text(label, x, y, size)

pin_names = {
    "J1": [
        "3V3", "3V3", "RST", "GPIO4", "GPIO5", "GPIO6",
        "GPIO7", "GPIO15", "GPIO16", "GPIO17", "GPIO18", "GPIO8",
        "GPIO3", "GPIO46", "GPIO9", "GPIO10", "GPIO11", "GPIO12",
        "GPIO13", "GPIO14", "5V", "GND",
    ],
    "J2": [
        "GND", "GPIO43", "GPIO44", "GPIO1", "GPIO2", "GPIO42",
        "GPIO41", "GPIO40", "GPIO39", "GPIO38", "GPIO37 NC",
        "GPIO36 NC", "GPIO35 NC", "GPIO0", "GPIO45", "GPIO48",
        "GPIO47", "GPIO21", "GPIO20 NC", "GPIO19 NC", "GND", "GND",
    ],
    "J3": ["VDD", "GND", "SCK", "WS", "SD", "L/R"],
    "J4": ["LRC", "BCLK", "DIN", "GAIN", "SD", "GND", "VIN 5V"],
    "J5": ["CLK NC", "INT NC", "SDA", "SCL", "VCC 3V3", "GND"],
    "J6": [
        "VCC 3V3", "GND", "CS", "RESET", "DC/RS", "SDI/MOSI",
        "SCK", "LED", "SDO NC", "T_CLK NC", "T_CS NC",
        "T_DIN NC", "T_DO NC", "T_IRQ NC",
    ],
    "J7": ["5V_IN", "5V_LOAD", "GND"],
    "J8": ["5V_IN", "GND"],
    "J11": ["KeyA", "KeyB", "KeyC", "Vcc", "Gnd"],
    "J12": ["G", "V", "X", "Y", "K"],
}

for footprint in board.GetFootprints():
    reference = footprint.GetReference()
    if reference not in pin_names:
        continue
    pads = list(footprint.Pads())
    assert len(pads) == len(pin_names[reference]), reference
    for pad in pads:
        number = int(pad.GetNumber())
        position = pad.GetPosition()
        x, y = pcb.ToMM(position.x), pcb.ToMM(position.y)
        if reference == "J11":
            label_x, label_y = x, y - (4.5 if number % 2 else 6.5)
        elif reference == "J12":
            label_x, label_y = x, y - 4.5
        elif reference == "J3":
            label_x, label_y = (59 if number % 2 else 33), y
        else:
            label_x = {
                "J1": 15, "J2": 58.5, "J4": 96, "J5": 86,
                "J6": 103 if number <= 2 else 116,
                "J7": 113, "J8": 93,
            }[reference]
            label_y = y
        text(pin_names[reference][number - 1], label_x, label_y, 1)

for start, end in [
    ((3, 3), (123, 3)),
    ((123, 3), (123, 115)),
    ((123, 115), (3, 115)),
    ((3, 115), (3, 3)),
]:
    line(*start, *end, pcb.Edge_Cuts, 0.05)

pcb.SaveBoard(str(BOARD_FILE), board)
print(BOARD_FILE)
