"""Generate the explicitly requested 2026-10-09 carrier revision.

Run only after backing up the routed PCB. This creates an unrouted board;
route_board.py and KiCad DRC are separate, mandatory steps.
"""

import argparse
from pathlib import Path
from math import sqrt

import pcbnew as pcb


HERE = Path(__file__).resolve().parent
LIB = Path(r"C:\Program Files\KiCad\10.0\share\kicad\footprints")
BOARD_FILE = HERE / "minibox-carrier.kicad_pcb"
LEFT, TOP, RIGHT, BOTTOM = 5, 3, 99.5, 120
ANTENNA = (81, 20.6, RIGHT, 42.1)
HOLES = ((10, 8), (RIGHT - 5, 8), (10, BOTTOM - 5), (RIGHT - 5, BOTTOM - 5))
CONTROL_BOUNDARY = 76
KEY_PITCH = 19.05
KEY_CENTERS = ((69.25, 89.5), (69.25 + KEY_PITCH, 89.5),
               (69.25 - KEY_PITCH / 2, 89.5 + KEY_PITCH),
               (69.25 + KEY_PITCH / 2, 89.5 + KEY_PITCH))
J1_NAMES = [
    "3V3", "3V3", "RST NC", "KEY1 G4", "KEY2 G5", "KEY3 G6", "KEY4 G7",
    "SDA G15", "SCL G16", "MIC G17", "G18 NC", "G8 NC", "G3 NC", "G46 NC",
    "BL G9", "SCK G10", "MOSI G11", "DC G12", "RST G13", "CS G14", "5V", "GND",
]
J1_NETS = [
    "3V3", "3V3", None, "BUTTON1", "RECORD", "BUTTON3", "BUTTON4",
    "RTC_SDA", "RTC_SCL", "MIC_SD", None, None, None, None,
    "LCD_BL", "LCD_SCK", "LCD_MOSI", "LCD_DC", "LCD_RST", "LCD_CS", "5V_SW", "GND",
]
J2_NAMES = [
    "GND", "TX NC", "RX NC", "X G1", "Y G2", "G42 NC", "DIN G41",
    "BCLK G40", "WS G39", "G38 NC", "G37 NC", "G36 NC", "G35 NC", "G0 NC",
    "G45 NC", "G48 NC", "SD G47", "GAIN G21", "USB+ NC", "USB- NC", "GND", "GND",
]
J2_NETS = [
    "GND", None, None, "GPIO1", "GPIO2", None, "AUDIO_DIN", "I2S_BCLK", "I2S_WS",
    None, None, None, None, None, None, None, "AMP_SD", "AMP_GAIN", None, None, "GND", "GND",
]


def point(x, y):
    return pcb.VECTOR2I(pcb.FromMM(x), pcb.FromMM(y))


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--replace-routed", action="store_true",
                    help="Explicitly authorize replacing the saved board after backup")
args = parser.parse_args()
if BOARD_FILE.exists() and list(pcb.LoadBoard(str(BOARD_FILE)).GetTracks()) and not args.replace_routed:
    raise RuntimeError("Refusing to overwrite routed PCB: back it up, then use --replace-routed")

board = pcb.BOARD()
board.SetCopperLayerCount(4)
board.SetLayerType(pcb.In1_Cu, pcb.LT_POWER)
board.SetLayerType(pcb.In2_Cu, pcb.LT_POWER)
board.GetDesignSettings().SetAuxOrigin(point(LEFT, BOTTOM))
board.GetDesignSettings().m_SolderMaskExpansion = pcb.FromMM(0.02)
board.GetDesignSettings().m_SolderMaskMinWidth = pcb.FromMM(0.1)
nets = {}


def net(name):
    if name not in nets:
        nets[name] = pcb.NETINFO_ITEM(board, name)
        board.Add(nets[name])
    return nets[name]


def load(reference, library, name, x, y, labels, value, angle=0):
    directory = HERE / "Minibox.pretty" if library == "Minibox" else LIB / (library + ".pretty")
    fp = pcb.FootprintLoad(str(directory), name)
    if fp is None:
        raise RuntimeError(f"Missing footprint: {directory / name}")
    fp.SetFPIDAsString(f"{library}:{name}")
    fp.SetReference(reference)
    fp.SetValue(value)
    fp.SetPosition(point(x, y))
    fp.SetOrientationDegrees(angle)
    fp.Reference().SetVisible(False)
    fp.Value().SetVisible(False)
    for pad in fp.Pads():
        label = labels.get(pad.GetNumber())
        if label:
            pad.SetNet(net(label))
        if label in ("5V_IN", "5V_SW"):
            pad.SetLocalClearance(pcb.FromMM(0.5))
    board.Add(fp)
    return fp


def socket(reference, count, x, y, labels, value, angle=0):
    return load(reference, "Connector_PinSocket_2.54mm",
                f"PinSocket_1x{count:02d}_P2.54mm_Vertical", x, y,
                {str(i + 1): label for i, label in enumerate(labels)}, value, angle)


def terminal(reference, count, x, y, labels, angle=0):
    return load(reference, "TerminalBlock_Phoenix",
                f"TerminalBlock_Phoenix_MKDS-1,5-{count}-5.08_1x{count:02d}_P5.08mm_Horizontal",
                x, y, {str(i + 1): label for i, label in enumerate(labels)},
                "5.08mm wire terminal", angle)


def text(value, x, y, angle=0, layer=pcb.F_SilkS, size=1):
    item = pcb.PCB_TEXT(board)
    item.SetText(value)
    item.SetPosition(point(x, y))
    item.SetTextSize(point(size, size))
    item.SetTextThickness(pcb.FromMM(0.15))
    item.SetLayer(layer)
    item.SetTextAngle(pcb.EDA_ANGLE(angle, pcb.DEGREES_T))
    board.Add(item)


def shape(start, end, layer=pcb.Dwgs_User, width=0.12, kind=pcb.SHAPE_T_SEGMENT):
    item = pcb.PCB_SHAPE(board)
    item.SetShape(kind)
    item.SetStart(point(*start))
    item.SetEnd(point(*end))
    item.SetLayer(layer)
    item.SetWidth(pcb.FromMM(width))
    board.Add(item)


def envelope(bounds, label):
    x1, y1, x2, y2 = bounds
    shape((x1, y1), (x2, y2), kind=pcb.SHAPE_T_RECT)
    text(label, (x1 + x2) / 2, (y1 + y2) / 2, layer=pcb.Dwgs_User)


def keepout(bounds, allow_pads=False):
    for layer in (pcb.F_Cu, pcb.In1_Cu, pcb.In2_Cu, pcb.B_Cu):
        area = pcb.ZONE(board)
        area.SetLayer(layer)
        area.SetIsRuleArea(True)
        area.SetDoNotAllowPads(not allow_pads)
        area.SetDoNotAllowTracks(True)
        area.SetDoNotAllowVias(True)
        area.SetDoNotAllowZoneFills(True)
        area.Outline().NewOutline()
        x1, y1, x2, y2 = bounds
        for x, y in ((x1, y1), (x2, y1), (x2, y2), (x1, y2)):
            area.Outline().Append(pcb.FromMM(x), pcb.FromMM(y))
        board.Add(area)


j1 = socket("J1", 22, 80, 19.3, J1_NETS, "ESP32-S3 J1 - USB LEFT", 270)
j2 = socket("J2", 22, 80, 44.7, J2_NETS, "ESP32-S3 J3 - USB LEFT", 270)
mic = load("J3", "Minibox", "INMP441_2x03_Row7.62mm", 52.31, 85.21,
           {"1": "3V3", "2": "GND", "3": "I2S_BCLK", "4": "I2S_WS",
            "5": "MIC_SD", "6": "GND"}, "INMP441 two 1x3 sockets")
amp = socket("J4", 7, 40, 52,
             ["I2S_WS", "I2S_BCLK", "AUDIO_DIN", "AMP_GAIN", "AMP_SD", "GND", "5V_SW"],
             "MAX98357A module socket")
for fp, number in ((j2, "1"), (amp, "6")):
    pad = next(p for p in fp.Pads() if p.GetNumber() == number)
    pad.SetThermalSpokeAngleDegrees(0)
    pad.SetThermalGap(pcb.FromMM(0.2))
    pad.SetLocalThermalSpokeWidthOverride(pcb.FromMM(0.25))
rtc = socket("J5", 6, 63, 10, [None, None, "RTC_SDA", "RTC_SCL", "3V3", "GND"],
             "PCF8563T upright module", 90)
lcd = socket("J6", 14, 23, 10,
             ["3V3", "GND", "LCD_CS", "LCD_RST", "LCD_DC", "LCD_MOSI",
              "LCD_SCK", "LCD_BL", None, None, None, None, None, None],
             "ST7796 14-pin display", 90)
sw = terminal("J7", 3, 88, 65, ["5V_IN", "5V_SW", "GND"], 90)
power = terminal("J8", 2, 14, 53.5, ["5V_IN", "GND"], 270)
cc = terminal("J9", 2, 14, 66, ["USB_CC1", "USB_CC2"], 270)
joy = load("JS1", "Minibox", "YV13S_L7.85_B10Ka_60_0_DL01", 22, 93,
           {"X1": "GND", "X2": "GPIO1", "X3": "3V3",
            "Y1": "GND", "Y2": "GPIO2", "Y3": "3V3"},
           "YV13S-L7.85-B10Ka(60)-0-DL01", 270)
for i, (x, y) in enumerate(KEY_CENTERS, 1):
    load(f"SW{i}", "Minibox", "CPG151101D13", x, y,
         {"1": ("BUTTON1", "RECORD", "BUTTON3", "BUTTON4")[i - 1], "2": "GND"},
         "CPG151101D13")
for i, y in enumerate((58, 63), 1):
    resistor = load(f"R{i}", "Resistor_SMD", "R_0402_1005Metric", 27, y,
                    {"1": f"USB_CC{i}", "2": "GND"}, "5.1k 1%")
    for pad in resistor.Pads():
        pad.SetLocalClearance(pcb.FromMM(0.2))
        pad.SetLocalThermalSpokeWidthOverride(pcb.FromMM(0.2))
        pad.SetThermalGap(pcb.FromMM(0.2))

for i, (x, y) in enumerate(HOLES, 1):
    load(f"H{i}", "MountingHole", "MountingHole_3mm", x, y, {}, "M3 NPTH")
    # A square clearance envelope also reserves the screw head/tool space.
    keepout((x - 3.5, y - 3.5, x + 3.5, y + 3.5), allow_pads=True)
keepout(ANTENNA)
envelope(ANTENNA, "ANTENNA: NO COPPER / NO MODULES")
envelope((20, 18.03, 88.5, 45.97), "ESP32 USB <")
envelope((35, 48, 62, 72), "MAX98357 MODULE RESERVATION")
envelope((60.5, 3, 88, 15), "RTC + BATTERY: MAX 12mm THICK")
shape((22, 93), (37, 93), kind=pcb.SHAPE_T_CIRCLE)
text("JOYSTICK MOTION D30", 22, 110, layer=pcb.Dwgs_User)
shape((LEFT + 4, CONTROL_BOUNDARY), (RIGHT - 4, CONTROL_BOUNDARY))
text("MODULES ABOVE / CONTROLS BELOW", 52, CONTROL_BOUNDARY,
     layer=pcb.Dwgs_User)
for x, y in KEY_CENTERS:
    envelope((x - 9, y - 9, x + 9, y + 9), "KEYCAP MAX 18x18")

for fp, names, y in ((j1, J1_NAMES, 15.8), (j2, J2_NAMES, 48)):
    for pad in fp.Pads():
        text(names[int(pad.GetNumber()) - 1], pcb.ToMM(pad.GetPosition().x), y, 90,
             layer=pcb.F_Fab)
for fp, names, y in (
        (lcd, ["3V3", "GND", "CS", "RST", "DC", "MOSI", "SCK", "BL",
               "NC", "NC", "NC", "NC", "NC", "NC"], 14),
        (rtc, ["CLK NC", "INT NC", "SDA15", "SCL16", "3V3", "GND"], 7),
        (amp, ["WS39", "BCLK40", "DIN41", "GAIN21", "SD47", "GND", "5V"], 57.5)):
    for pad in fp.Pads():
        text(names[int(pad.GetNumber()) - 1], pcb.ToMM(pad.GetPosition().x), y, 90,
             layer=pcb.F_Fab)
for fp, names, x in ((sw, ["IN", "OUT", "GND"], 78),
                     (power, ["5V", "GND"], 22),
                     (cc, ["CC1", "CC2"], 22)):
    for pad in fp.Pads():
        text(names[int(pad.GetNumber()) - 1], x, pcb.ToMM(pad.GetPosition().y))
for value, x, y in (
        ("LCD", 40, 5), ("RTC", 87, 12), ("AMP", 48, 62),
        ("SW", 94, 62), ("PWR", 14, 46), ("JOY", 22, 108),
        ("MIC17", 48.5, 100),
        ("CC Rd 5.1k", 30, 69), ("JOY PUSH UNUSED", 28, 78.5),
        ("MiniBox v1.1 2026-10-09", 48, 36)):
    text(value, x, y)
for i, (x, y) in enumerate(KEY_CENTERS, 1):
    text(f"SW{i} G{i + 3}", x, y + 9)

r = 5
for a, b in (
        ((LEFT + r, TOP), (RIGHT - r, TOP)),
        ((RIGHT, TOP + r), (RIGHT, BOTTOM - r)),
        ((RIGHT - r, BOTTOM), (LEFT + r, BOTTOM)),
        ((LEFT, BOTTOM - r), (LEFT, TOP + r))):
    shape(a, b, pcb.Edge_Cuts, 0.05)
d = r / sqrt(2)
for a, m, b in (
        ((RIGHT-r, TOP), (RIGHT-r+d, TOP+r-d), (RIGHT, TOP+r)),
        ((RIGHT, BOTTOM-r), (RIGHT-r+d, BOTTOM-r+d), (RIGHT-r, BOTTOM)),
        ((LEFT+r, BOTTOM), (LEFT+r-d, BOTTOM-r+d), (LEFT, BOTTOM-r)),
        ((LEFT, TOP+r), (LEFT+r-d, TOP+r-d), (LEFT+r, TOP))):
    item = pcb.PCB_SHAPE(board)
    item.SetShape(pcb.SHAPE_T_ARC)
    item.SetArcGeometry(point(*a), point(*m), point(*b))
    item.SetLayer(pcb.Edge_Cuts)
    item.SetWidth(pcb.FromMM(0.05))
    board.Add(item)

pcb.SaveBoard(str(BOARD_FILE), board)
print(BOARD_FILE)
