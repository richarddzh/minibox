"""Generate the explicitly requested 2026-10-10 carrier revision.

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
KEY_CENTERS = ((70.85, 89.5), (70.85 + KEY_PITCH, 89.5),
               (62.75, 89.5 + KEY_PITCH), (81.8, 89.5 + KEY_PITCH))
J1_NAMES = [
    "3V3", "3V3", "EN NC", "KEY1 4", "KEY2 5", "KEY3 6", "KEY4 7",
    "SDA15", "SCL16", "17 NC", "18 NC", "8 NC", "3 NC", "46 NC",
    "BL9", "SCK10", "MOSI11", "DC12", "RST13", "CS14", "5V", "GND",
]
J1_NETS = [
    "3V3", "3V3", None, "BUTTON1", "RECORD", "BUTTON3", "BUTTON4",
    "RTC_SDA", "RTC_SCL", None, None, None, None, None,
    "LCD_BL", "LCD_SCK", "LCD_MOSI", "LCD_DC", "LCD_RST", "LCD_CS", "5V_SW", "GND",
]
J2_NAMES = [
    "GND", "TX43 NC", "RX44 NC", "X 1", "Y 2", "GAIN42", "DIN41",
    "BCLK40", "WS39", "38 NC", "37 NC", "36 NC", "35 NC", "0 NC",
    "45 NC", "48 NC", "SD47", "SD21", "D+20 NC", "D-19 NC", "GND", "GND",
]
J2_NETS = [
    "GND", None, None, "GPIO1", "GPIO2", "AMP_GAIN", "AUDIO_DIN", "I2S_BCLK", "I2S_WS",
    None, None, None, None, None, None, None, "AMP_SD", "MIC_SD", None, None, "GND", "GND",
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
        if pad.GetAttribute() != pcb.PAD_ATTRIB_NPTH:
            pad.SetLocalClearance(pcb.FromMM(0.3))
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
    return load(reference, "Minibox", f"WJ500V_5.08_{count}P",
                x, y, {str(i + 1): label for i, label in enumerate(labels)},
                "WJ500V-5.08-2P" if count == 2 else "WJ500V-5.08-03P-14-00A", angle)


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
mic = load("J3", "Minibox", "INMP441_2x03_Row7.62mm", 48.265, 87.71,
           {"1": "GND", "2": "GND", "3": "3V3", "4": "I2S_WS",
            "5": "MIC_SD", "6": "I2S_BCLK"}, "INMP441 two 1x3 sockets")
amp = socket("J4", 7, 40, 52,
             ["I2S_WS", "I2S_BCLK", "AUDIO_DIN", "AMP_GAIN", "AMP_SD", "GND", "5V_SW"],
             "MAX98357A module socket")
for fp, number in ((j2, "1"), (j2, "22"), (amp, "6")):
    pad = next(p for p in fp.Pads() if p.GetNumber() == number)
    pad.SetThermalSpokeAngleDegrees(30 if fp == j2 and number == "1" else 0)
    pad.SetThermalGap(pcb.FromMM(0.2))
    pad.SetLocalThermalSpokeWidthOverride(pcb.FromMM(0.25))
rtc = socket("J5", 6, 63, 10, [None, None, "RTC_SCL", "RTC_SDA", "3V3", "GND"],
             "PCF8563T upright module", 90)
lcd = socket("J6", 14, 23, 10,
             ["3V3", "GND", "LCD_CS", "LCD_RST", "LCD_DC", "LCD_MOSI",
              "LCD_SCK", "LCD_BL", None, None, None, None, None, None],
             "ST7796 14-pin display", 90)
sw = terminal("J7", 3, 88, 65, ["5V_IN", "5V_SW", "GND"], 90)
power = terminal("J8", 2, 11.5, 48.56, ["5V_IN", "GND"], 270)
cc = terminal("J9", 2, 11.5, 61.06, ["USB_CC1", "USB_CC2"], 270)
joy = load("JS1", "Minibox", "YV13S_L7.85_B10Ka_60_0_DL01", 16.8, 95.5,
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
        pad.SetLocalClearance(pcb.FromMM(0.3))
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
shape((16.8, 95.5), (31.8, 95.5), kind=pcb.SHAPE_T_CIRCLE)
text("JOYSTICK MOTION D30", 20, 112.5, layer=pcb.Dwgs_User)
shape((LEFT + 4, CONTROL_BOUNDARY), (RIGHT - 4, CONTROL_BOUNDARY))
text("MODULES ABOVE / CONTROLS BELOW", 52, CONTROL_BOUNDARY,
     layer=pcb.Dwgs_User)
for x, y in KEY_CENTERS:
    envelope((x - 9, y - 9, x + 9, y + 9), "KEYCAP MAX 18x18")

for fp, names, y in ((j1, J1_NAMES, 25), (j2, J2_NAMES, 39)):
    for pad in fp.Pads():
        text(names[int(pad.GetNumber()) - 1], pcb.ToMM(pad.GetPosition().x), y, 90)
for fp, names, y in (
        (lcd, ["3V3", "GND", "CS14", "RST13", "DC12", "MOSI11", "SCK10", "BL9",
               "SDO NC", "TCK NC", "TCS NC", "TDI NC", "TDO NC", "IRQ NC"], 14.5),
        (rtc, ["CLK NC", "INT NC", "SCL16", "SDA15", "3V3", "GND"], 14.5)):
    for pad in fp.Pads():
        text(names[int(pad.GetNumber()) - 1], pcb.ToMM(pad.GetPosition().x), y, 90)
amp_names = ["WS39", "BCLK40", "DIN41", "GAIN42", "SD47", "GND", "5V"]
for pad in amp.Pads():
    text(amp_names[int(pad.GetNumber()) - 1], 47.5, pcb.ToMM(pad.GetPosition().y))
mic_names = ["GND", "L/R GND", "3V3", "WS39", "SD21", "SCK40"]
for pad in mic.Pads():
    number = int(pad.GetNumber())
    text(mic_names[number - 1], 44.455 if number % 2 else 30,
         pcb.ToMM(pad.GetPosition().y))
for pad in joy.Pads():
    number = pad.GetNumber()
    if number.startswith("X"):
        text({"X1": "GND", "X2": "X 1", "X3": "3V3"}[number], 10.5,
             pcb.ToMM(pad.GetPosition().y))
    elif number.startswith("Y"):
        text({"Y1": "GND", "Y2": "Y 2", "Y3": "3V3"}[number],
             pcb.ToMM(pad.GetPosition().x), 108.5, 90)
for fp, names, x in ((sw, ["IN5V", "OUT5V", "GND"], 75.5),
                     (power, ["5V IN", "GND"], 20),
                     (cc, ["CC1", "CC2"], 20)):
    for pad in fp.Pads():
        text(names[int(pad.GetNumber()) - 1], x, pcb.ToMM(pad.GetPosition().y))
for value, x, y in (
        ("LCD", 40, 5), ("RTC", 87, 12), ("AMP", 55, 72),
        ("SW", 94, 62), ("PWR", 11.5, 42), ("JOY", 20, 113.5),
        ("MIC21", 44.455, 102.5),
        ("CC Rd 5.1k", 30, 69), ("JOY PUSH UNUSED", 28, 78.5),
        ("MiniBox v1.2 2026-10-10", 48, 32)):
    text(value, x, y)
for i, (x, y) in enumerate(KEY_CENTERS, 1):
    text(f"SW{i} {i + 3}", x, y + 9)
    text(str(i + 3), x - 3.81, y - 0.3)
    text("GND", x + 2.54, y - 2.9)

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
