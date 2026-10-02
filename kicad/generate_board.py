"""Generate the provisional Minibox carrier in KiCad 10 using its bundled Python."""

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
existing_sheets = None
if PROJECT_FILE.exists():
    existing_sheets = json.loads(PROJECT_FILE.read_text(encoding="utf-8")).get(
        "schematic", {}).get("top_level_sheets")


def point(x, y):
    return pcb.VECTOR2I(pcb.FromMM(x), pcb.FromMM(y))


board = pcb.BOARD()
board.SetCopperLayerCount(2)
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
    for layer in (pcb.F_Cu, pcb.B_Cu):
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
    for layer in (pcb.F_Cu, pcb.B_Cu):
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

# Standard two-row, three-pin socket. KiCad numbers alternate by column:
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
socket("J6", 14, 110, 18,
       ["3V3", "GND", "LCD_CS", "LCD_RST", "LCD_DC",
        "LCD_MOSI", "LCD_SCK", "LCD_BL", None, None, None,
        None, None, None],
       "ST7796 14-PIN DISPLAY HEADER")
terminal("J7", 3, 115, 61.57, ["5V_IN", "5V_SW", "GND"], orientation=-90)
terminal("J8", 2, 97, 12, ["5V_IN", "GND"])
socket("J11", 5, 90, 73,
       ["BUTTON1", "RECORD", "BUTTON3", "3V3", "GND"],
       "THREE-KEY CABLE KeyA KeyB KeyC Vcc Gnd", orientation=90)
socket("J12", 5, 12, 73,
       ["GND", "3V3", "GPIO1", "GPIO2", "GPIO42"],
       "JOYSTICK CABLE G V X Y K", orientation=90)
for reference, x, y in (
    ("H1", 8, 8), ("H2", 118, 8),
    ("H3", 8, 110), ("H4", 118, 110),
):
    mounting_hole(reference, x, y)
antenna_keepout(52, 67, 73.5, 84)

# The amplifier module exposes speaker outputs on its own screw terminal.
for label, x, y, size in [
    ("ESP32-S3", 62, 16, 1), ("USB ^", 62, 20, 1),
    ("MIC 2x3", 61, 88, 1), ("AMP", 12, 33, 1),
    ("RTC", 16, 9, 1), ("LCD", 110, 55.5, 1),
    ("SW", 115, 56.57, 1), ("5V IN", 90, 8, 1),
    ("KEYS", 95, 79, 1),
    ("JOY", 17, 79, 1), ("SPK: USE AMP TERMINAL", 19, 60, 1),
    ("PROTOTYPE - VERIFY PIN PITCH AND ORDER", 92, 112, 1),
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
    "J3": ["VDD", "GND", "SCK", "WS", "SD", "L/R GND"],
    "J4": ["LRC", "BCLK", "DIN", "GAIN", "SD", "GND", "VIN 5V"],
    "J5": ["CLK NC", "INT NC", "SDA", "SCL", "VCC 3V3", "GND"],
    "J6": [
        "3V3", "GND", "CS", "RESET", "DC/RS", "SDI/MOSI",
        "SCK", "LED", "SDO NC", "T_CLK NC", "T_CS NC",
        "T_DIN NC", "T_DO NC", "T_IRQ NC",
    ],
    "J7": ["5V_IN", "5V_LOAD", "GND"],
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
            label_x, label_y = x, y + 7
        elif reference == "J11":
            label_x, label_y, angle = x, y - 7, 90
        elif reference == "J12":
            label_x, label_y, angle = x, y - 7, 90
        elif reference == "J3":
            label_x, label_y = (64 if number % 2 else 30), y
        else:
            label_x = {
                "J1": 83, "J2": 39, "J4": 25, "J5": 27,
                "J6": 98 if 3 <= number <= 8 else 116,
                "J7": 105,
            }[reference]
            label_y = y
        label = pin_names[reference][number - 1]
        if reference not in ("J1", "J2"):
            if pad.GetNetname() in net_gpio:
                label += " " + net_gpio[pad.GetNetname()]
            elif not pad.GetNetname() and "NC" not in label:
                label += " NC"
        text(label, label_x, label_y, 1, angle)

for start, end in [
    ((8, 3), (118, 3)),
    ((123, 8), (123, 110)),
    ((118, 115), (8, 115)),
    ((3, 110), (3, 8)),
]:
    line(*start, *end, pcb.Edge_Cuts, 0.05)
offset = 5 / sqrt(2)
arc((118, 3), (118 + offset, 8 - offset), (123, 8))
arc((123, 110), (118 + offset, 110 + offset), (118, 115))
arc((8, 115), (8 - offset, 110 + offset), (3, 110))
arc((3, 8), (8 - offset, 8 - offset), (8, 3))

microphone_top = 100 + 2.54 - 9.75
interface_bottoms = []
for footprint in board.GetFootprints():
    if footprint.GetReference() in ("J7", "J11", "J12"):
        # Align the physical body outlines, not the differently padded courtyards.
        interface_bottoms.append(max(
            pcb.ToMM(position.y)
            for item in footprint.GraphicalItems()
            if isinstance(item, pcb.PCB_SHAPE) and item.GetLayer() == pcb.F_Fab
            for position in (item.GetStart(), item.GetEnd())))
    if footprint.GetReference() in ("J3", "H1", "H2", "H3", "H4"):
        continue
    if pcb.ToMM(footprint.GetBoundingBox(False, False).GetBottom()) >= microphone_top:
        raise RuntimeError(f"{footprint.GetReference()} intrudes below the microphone top")
if max(interface_bottoms) - min(interface_bottoms) > 0.001:
    raise RuntimeError("Switch, keyboard and joystick body lower edges are not aligned")

silk = [item for item in board.GetDrawings()
        if item.GetLayer() in (pcb.F_SilkS, pcb.B_SilkS)]
for footprint in board.GetFootprints():
    silk.extend(item for item in footprint.GraphicalItems()
                if item.GetLayer() in (pcb.F_SilkS, pcb.B_SilkS))
    silk.extend(item for item in (footprint.Reference(), footprint.Value())
                if item.GetLayer() in (pcb.F_SilkS, pcb.B_SilkS) and item.IsVisible())
for x, y in ((8, 8), (118, 8), (8, 110), (118, 110)):
    for item in silk:
        box = item.GetBoundingBox()
        left, top, right, bottom = map(
            pcb.ToMM, (box.GetLeft(), box.GetTop(), box.GetRight(), box.GetBottom()))
        distance = hypot(max(left - x, 0, x - right), max(top - y, 0, y - bottom))
        if distance < 3.5:
            raise RuntimeError(f"Silkscreen intrudes into M3 screw area at ({x}, {y})")

pcb.SaveBoard(str(BOARD_FILE), board)
if existing_sheets is not None:
    project = json.loads(PROJECT_FILE.read_text(encoding="utf-8"))
    project["schematic"]["top_level_sheets"] = existing_sheets
    PROJECT_FILE.write_text(json.dumps(project, indent=2) + "\n", encoding="utf-8")
print(BOARD_FILE)
