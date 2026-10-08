"""Generate the provisional Minibox carrier in KiCad 10 using its bundled Python."""

from collections import Counter
import json
from pathlib import Path
from math import cos, hypot, pi, sin, sqrt

import pcbnew as pcb


HERE = Path(__file__).resolve().parent
SOCKETS = Path(r"C:\Program Files\KiCad\10.0\share\kicad\footprints\Connector_PinSocket_2.54mm.pretty")
CUSTOM = HERE / "Minibox.pretty"
MOUNTING = Path(r"C:\Program Files\KiCad\10.0\share\kicad\footprints\MountingHole.pretty")
TERMINALS = Path(r"C:\Program Files\KiCad\10.0\share\kicad\footprints\TerminalBlock_Phoenix.pretty")
BOARD_FILE = HERE / "minibox-carrier.kicad_pcb"
PROJECT_FILE = BOARD_FILE.with_suffix(".kicad_pro")
LEFT, TOP, RIGHT, BOTTOM = 5, 3, 105, 115
CORNER_RADIUS = 5
BOARD_NAME = "PangMiaoMiao MiniBox"
BOARD_VERSION = "v1.0"
BOARD_DATE = "2026-10-03"
MOUNTING_POINTS = (
    ("H1", LEFT + 5, 8), ("H2", RIGHT - 5, 8),
    ("H3", LEFT + 5, 110), ("H4", RIGHT - 5, 110),
)
existing_sheets = None
if PROJECT_FILE.exists():
    existing_sheets = json.loads(PROJECT_FILE.read_text(encoding="utf-8")).get(
        "schematic", {}).get("top_level_sheets")


def point(x, y):
    return pcb.VECTOR2I(pcb.FromMM(x), pcb.FromMM(y))


board = pcb.BOARD()
board.SetCopperLayerCount(4)
board.SetLayerType(pcb.In1_Cu, pcb.LT_POWER)
board.SetLayerType(pcb.In2_Cu, pcb.LT_POWER)
COPPER_LAYERS = (pcb.F_Cu, pcb.In1_Cu, pcb.In2_Cu, pcb.B_Cu)
board.GetDesignSettings().m_SolderMaskExpansion = pcb.FromMM(0.05)
board.GetDesignSettings().m_SolderMaskMinWidth = pcb.FromMM(0.1)
nets = {}
socket_counts = Counter()


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
    socket_counts[count] += 1
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
    socket_counts[3] += 2
    return footprint


def terminal(reference, count, x, y, labels, orientation=0):
    assert len(labels) == count
    name = f"TerminalBlock_Phoenix_MKDS-1,5-{count}-5.08_1x{count:02d}_P5.08mm_Horizontal"
    footprint = pcb.FootprintLoad(str(TERMINALS), name)
    if footprint is None:
        raise RuntimeError(f"Missing KiCad footprint: {name}")
    footprint.SetFPIDAsString(f"TerminalBlock_Phoenix:{name}")
    footprint.SetReference(reference)
    footprint.SetValue("5.08mm wire terminal")
    footprint.SetOrientationDegrees(orientation)
    footprint.SetPosition(point(x, y))
    footprint.Reference().SetVisible(False)
    footprint.Value().SetVisible(False)
    for pad in footprint.Pads():
        pad.SetNet(net(labels[int(pad.GetNumber()) - 1]))
        if pad.GetNetname() in ("5V_IN", "5V_SW"):
            pad.SetLocalClearance(pcb.FromMM(0.5))
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
    for layer in COPPER_LAYERS:
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


def antenna_keepout(x1, y1, x2, y2):
    for layer in COPPER_LAYERS:
        keepout = pcb.ZONE(board)
        keepout.SetLayer(layer)
        keepout.SetIsRuleArea(True)
        keepout.SetDoNotAllowPads(True)
        keepout.SetDoNotAllowTracks(True)
        keepout.SetDoNotAllowVias(True)
        keepout.SetDoNotAllowZoneFills(True)
        keepout.Outline().NewOutline()
        for x, y in ((x1, y1), (x2, y1), (x2, y2), (x1, y2)):
            keepout.Outline().Append(pcb.FromMM(x), pcb.FromMM(y))
        board.Add(keepout)
    envelope(x1, y1, x2, y2)


def text(value, x, y, size=1, angle=0):
    item = pcb.PCB_TEXT(board)
    item.SetText(value)
    item.SetPosition(point(x, y))
    item.SetTextSize(point(size, size))
    item.SetTextThickness(pcb.FromMM(0.16))
    item.SetLayer(pcb.F_SilkS)
    item.SetTextAngle(pcb.EDA_ANGLE(angle, pcb.DEGREES_T))
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


def arc(start, mid, end):
    item = pcb.PCB_SHAPE(board)
    item.SetShape(pcb.SHAPE_T_ARC)
    item.SetArcGeometry(point(*start), point(*mid), point(*end))
    item.SetLayer(pcb.Edge_Cuts)
    item.SetWidth(pcb.FromMM(0.05))
    board.Add(item)


# The reference pinout is viewed with USB at the bottom. Rotate both
# complete socket rows 180 degrees in the board plane so USB faces up.
socket("J1", 22, 74.8, 66, [
    "3V3", "3V3", None, "BUTTON1", "RECORD", "BUTTON3", "AMP_SD",
    "MIC_SD", "GPIO16", "GPIO17", "GPIO18", "AMP_GAIN",
    "GPIO3", "GPIO46", "LCD_BL", "LCD_SCK", "LCD_MOSI",
    "LCD_DC", "LCD_RST", "LCD_CS", "5V_SW", "GND",
], "ESP32-S3 LEFT - USB AT TOP", orientation=180)
socket("J2", 22, 49.4, 66, [
    "GND", "GPIO43", "GPIO44", "GPIO1", "GPIO2",
    "GPIO42", "AUDIO_DIN", "I2S_BCLK", "I2S_WS", "GPIO38",
    None, None, None, "GPIO0", "GPIO45", "GPIO48", "RTC_SDA",
    "RTC_SCL", None, None, "GND", "GND",
], "ESP32-S3 RIGHT - USB AT TOP", orientation=180)

# Two separate 1x3 female strips. KiCad numbers alternate by column:
# 1 2 / 3 4 / 5 6. Verify the actual microphone's pin order before insertion.
double_socket("J3", 50, 100,
              ["3V3", "GND", "I2S_BCLK", "I2S_WS", "MIC_SD", "GND"],
              "INMP441 2x3 VDD GND / SCK WS / SD LR")
socket("J4", 7, 12, 38,
       ["I2S_WS", "I2S_BCLK", "AUDIO_DIN", "AMP_GAIN",
        "AMP_SD", "GND", "5V_SW"],
       "MAX98357A LRC BCLK DIN GAIN SD GND VIN")
envelope(7, 34, 34, 58)  # Body and on-module speaker screw terminal.
socket("J5", 6, 16, 12,
       [None, None, "RTC_SDA", "RTC_SCL", "3V3", "GND"],
       "PCF8563T CLK INT SDA SCL VCC GND")

# The owner confirmed the screen's 14-pin header follows assets/tft_spi.jpg.
# Touch and readback pins are unused by the current firmware.
socket("J6", 14, 96, 14,
       ["3V3", "GND", "LCD_CS", "LCD_RST", "LCD_DC",
        "LCD_MOSI", "LCD_SCK", "LCD_BL", None, None, None,
        None, None, None],
       "ST7796 14-PIN DISPLAY HEADER")
terminal("J7", 3, 96, 55.5, ["5V_IN", "5V_SW", "GND"], orientation=-90)
terminal("J8", 2, 28, 11, ["5V_IN", "GND"])
socket("J11", 5, 82, 73,
       ["BUTTON1", "RECORD", "BUTTON3", "3V3", "GND"],
       "THREE-KEY CABLE KeyA KeyB KeyC Vcc Gnd", orientation=90)
socket("J12", 5, 12, 73,
       ["GND", "3V3", "GPIO1", "GPIO2", "GPIO42"],
       "JOYSTICK CABLE G V X Y K", orientation=90)
for reference, x, y in MOUNTING_POINTS:
    mounting_hole(reference, x, y)
antenna_keepout(52, 67, 73.5, 84)

# The amplifier module exposes speaker outputs on its own screw terminal.
for label, x, y, size in [
    ("ESP32-S3", 62, 16, 1), ("USB ^", 62, 20, 1),
    ("MIC 2x3", 61, 88, 1), ("AMP", 12, 33, 1),
    ("RTC", 16, 9, 1), ("LCD", 87.5, 11, 1),
    ("SW", 96, 50.5, 1),
    ("KEYS", 87, 91, 1),
    ("JOY", 17, 79, 1), ("SPK: USE AMP TERMINAL", 19, 60, 1),
]:
    text(label, x, y, size)

text(BOARD_NAME, 30, 85.5, 1.2)
text(f"{BOARD_VERSION}  {BOARD_DATE}", 30, 89)
text("SOCKETS P2.54", 17, 92)
for index, (count, quantity) in enumerate(sorted(socket_counts.items(), reverse=True)):
    text(f"1x{count:02d}  x{quantity}", 17, 94.5 + index * 2)
text("MECHANICAL / mm", 88.5, 93)
for y in (94, 98, 102, 106):
    line(74, y, 103, y, pcb.F_SilkS, 0.15)
for x in (74, 84, 103):
    line(x, 94, x, 106, pcb.F_SilkS, 0.15)
hole_dx = MOUNTING_POINTS[1][1] - MOUNTING_POINTS[0][1]
hole_dy = MOUNTING_POINTS[2][2] - MOUNTING_POINTS[0][2]
for y, label, value in (
        (96, "BOARD", f"{RIGHT - LEFT}x{BOTTOM - TOP}"),
        (100, "PITCH", f"{hole_dx}x{hole_dy}"),
        (104, "HOLES", "4xD3.0")):
    text(label, 79, y)
    text(value, 93.5, y)

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
    "J3": ["VDD", "GND", "SCK", "WS", "SD", "L/R GND"],
    "J4": ["LRC", "BCLK", "DIN", "GAIN", "SD", "GND", "VIN 5V"],
    "J5": ["CLK NC", "INT NC", "SDA", "SCL", "VCC 3V3", "GND"],
    "J6": [
        "3V3", "GND", "CS", "RESET", "DC/RS", "SDI/MOSI",
        "SCK", "LED", "SDO NC", "T_CLK NC", "T_CS NC",
        "T_DIN NC", "T_DO NC", "T_IRQ NC",
    ],
    "J7": ["5V_IN", "5V_SW", "GND"],
    "J8": ["5V_IN", "GND"],
    "J11": ["KeyA", "KeyB", "KeyC", "Vcc", "Gnd"],
    "J12": ["G", "V", "X", "Y", "K"],
}

net_gpio = {}
for footprint in board.GetFootprints():
    if footprint.GetReference() not in ("J1", "J2"):
        continue
    for pad in footprint.Pads():
        name = pin_names[footprint.GetReference()][int(pad.GetNumber()) - 1]
        if name.startswith("GPIO") and pad.GetNetname():
            if pad.GetNetname() in net_gpio and net_gpio[pad.GetNetname()] != name:
                raise RuntimeError(f"Ambiguous GPIO mapping for {pad.GetNetname()}")
            net_gpio[pad.GetNetname()] = name

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
        angle = 0
        if reference == "J8":
            label_x, label_y = x, y - 7
        elif reference == "J11":
            label_x, label_y, angle = x, y + 10, 90
        elif reference == "J12":
            label_x, label_y, angle = x, y - 7, 90
        elif reference == "J3":
            label_x, label_y = (64 if number % 2 else 30), y
        else:
            label_x = {
                "J1": 79.5 if number <= 2 else 80, "J2": 39, "J4": 25,
                "J5": 21.5 if number <= 2 else 26,
                "J6": 88.3,
                "J7": 86.8,
            }[reference]
            label_y = y
        label = pin_names[reference][number - 1]
        if reference == "J5" and number == 3:
            label_y += 0.15
        if reference == "J6" and number == 6:
            label = "MOSI"
        if reference not in ("J1", "J2"):
            if pad.GetNetname() in net_gpio:
                label += " " + net_gpio[pad.GetNetname()]
            elif not pad.GetNetname() and "NC" not in label:
                label += " NC"
        text(label, label_x, label_y, 1, angle)

r = CORNER_RADIUS
for start, end in [
    ((LEFT + r, TOP), (RIGHT - r, TOP)),
    ((RIGHT, TOP + r), (RIGHT, BOTTOM - r)),
    ((RIGHT - r, BOTTOM), (LEFT + r, BOTTOM)),
    ((LEFT, BOTTOM - r), (LEFT, TOP + r)),
]:
    line(*start, *end, pcb.Edge_Cuts, 0.05)
offset = r / sqrt(2)
arc((RIGHT - r, TOP), (RIGHT - r + offset, TOP + r - offset), (RIGHT, TOP + r))
arc((RIGHT, BOTTOM - r), (RIGHT - r + offset, BOTTOM - r + offset), (RIGHT - r, BOTTOM))
arc((LEFT + r, BOTTOM), (LEFT + r - offset, BOTTOM - r + offset), (LEFT, BOTTOM - r))
arc((LEFT, TOP + r), (LEFT + r - offset, TOP + r - offset), (LEFT + r, TOP))

microphone_top = 100 + 2.54 - 9.75
body_bounds = {}
for footprint in board.GetFootprints():
    if footprint.GetReference() in ("J6", "J7", "J11", "J12"):
        positions = [
            position
            for item in footprint.GraphicalItems()
            if isinstance(item, pcb.PCB_SHAPE) and item.GetLayer() == pcb.F_Fab
            for position in (item.GetStart(), item.GetEnd())]
        body_bounds[footprint.GetReference()] = (
            min(pcb.ToMM(p.x) for p in positions), min(pcb.ToMM(p.y) for p in positions),
            max(pcb.ToMM(p.x) for p in positions), max(pcb.ToMM(p.y) for p in positions))
    if footprint.GetReference() in ("J3", "H1", "H2", "H3", "H4"):
        continue
    if pcb.ToMM(footprint.GetBoundingBox(False, False).GetBottom()) >= microphone_top:
        raise RuntimeError(f"{footprint.GetReference()} intrudes below the microphone top")
if abs(body_bounds["J11"][3] - body_bounds["J12"][3]) > 0.001:
    raise RuntimeError("Keyboard and joystick body lower edges are not aligned")
if max(body_bounds[ref][3] for ref in ("J7", "J11", "J12")) > 74.271:
    raise RuntimeError("Interface body lower edges moved downward")
if body_bounds["J7"][1] - body_bounds["J6"][3] < 2 or (
        body_bounds["J11"][1] - body_bounds["J7"][3] < 2):
    raise RuntimeError("Stacked LCD, switch and keyboard bodies need at least 2 mm gaps")
if body_bounds["J7"][2] > RIGHT - 3:
    raise RuntimeError("Switch wire-entry side needs at least 3 mm to the board edge")
if LEFT > 7 - 2:
    raise RuntimeError("Amplifier module envelope needs at least 2 mm to the board edge")
for reference, expected in (("J1", (74.8, 66)), ("J2", (49.4, 66)),
                            ("J3", (50, 100)), ("J4", (12, 38))):
    footprint = next(f for f in board.GetFootprints() if f.GetReference() == reference)
    if footprint.GetPosition() != point(*expected):
        raise RuntimeError(f"{reference} moved from its fixed position")

silk = [item for item in board.GetDrawings()
        if item.GetLayer() in (pcb.F_SilkS, pcb.B_SilkS)]
for footprint in board.GetFootprints():
    silk.extend(item for item in footprint.GraphicalItems()
                if item.GetLayer() in (pcb.F_SilkS, pcb.B_SilkS))
    silk.extend(item for item in (footprint.Reference(), footprint.Value())
                if item.GetLayer() in (pcb.F_SilkS, pcb.B_SilkS) and item.IsVisible())
for _, x, y in MOUNTING_POINTS:
    for item in silk:
        box = item.GetBoundingBox()
        left, top, right, bottom = map(
            pcb.ToMM, (box.GetLeft(), box.GetTop(), box.GetRight(), box.GetBottom()))
        distance = hypot(max(left - x, 0, x - right), max(top - y, 0, y - bottom))
        if distance < 3.5:
            raise RuntimeError(f"Silkscreen intrudes into M3 screw area at ({x}, {y})")

pcb.SaveBoard(str(BOARD_FILE), board)
# KiCad's bundled SWIG API does not expose stackup item setters.
stackup = """(stackup
        (layer "F.SilkS" (type "Top Silk Screen"))
        (layer "F.Paste" (type "Top Solder Paste"))
        (layer "F.Mask" (type "Top Solder Mask") (thickness 0.01))
        (layer "F.Cu" (type "copper") (thickness 0.035))
        (layer "dielectric 1" (type "prepreg") (thickness 0.18)
            (material "FR4") (epsilon_r 4.5) (loss_tangent 0.02))
        (layer "In1.Cu" (type "copper") (thickness 0.035))
        (layer "dielectric 2" (type "core") (thickness 1.08)
            (material "FR4") (epsilon_r 4.5) (loss_tangent 0.02))
        (layer "In2.Cu" (type "copper") (thickness 0.035))
        (layer "dielectric 3" (type "prepreg") (thickness 0.18)
            (material "FR4") (epsilon_r 4.5) (loss_tangent 0.02))
        (layer "B.Cu" (type "copper") (thickness 0.035))
        (layer "B.Mask" (type "Bottom Solder Mask") (thickness 0.01))
        (layer "B.Paste" (type "Bottom Solder Paste"))
        (layer "B.SilkS" (type "Bottom Silk Screen"))
        (copper_finish "HAL lead-free")
        (dielectric_constraints no)
    )
    """
before, setup, after = BOARD_FILE.read_text(encoding="utf-8").partition("(setup\n")
if not setup:
    raise RuntimeError("Generated board has no setup section for the stackup")
BOARD_FILE.write_text(before + setup + "\t\t" + stackup + after, encoding="utf-8")
if existing_sheets is not None:
    project = json.loads(PROJECT_FILE.read_text(encoding="utf-8"))
    project["schematic"]["top_level_sheets"] = existing_sheets
    PROJECT_FILE.write_text(json.dumps(project, indent=2) + "\n", encoding="utf-8")
print(BOARD_FILE)
