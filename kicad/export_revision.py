"""Export the saved carrier for JLC engineering review, not automatic release."""

import csv
import hashlib
import html
import json
from math import hypot
from pathlib import Path
import subprocess
import zipfile

import pcbnew as pcb


HERE = Path(__file__).resolve().parent
CLI = Path(r"C:\Program Files\KiCad\10.0\bin\kicad-cli.exe")
FILE = HERE / "minibox-carrier.kicad_pcb"
OUT = HERE / "assembly" / "revision-20261009"
GERBERS = HERE / "fabrication" / "revision-20261009"
RENDERS = HERE / "renders"
OUT.mkdir(parents=True, exist_ok=True)
GERBERS.mkdir(parents=True, exist_ok=True)
RENDERS.mkdir(exist_ok=True)


def run(*args):
    subprocess.run([str(CLI), *map(str, args)], check=True)


def mm(v):
    return pcb.ToMM(v)


def csv_file(name, headers, rows):
    with (OUT / name).open("w", encoding="utf-8-sig", newline="") as stream:
        writer = csv.writer(stream)
        writer.writerow(headers)
        writer.writerows(rows)


run("pcb", "drc", "--severity-all",
    "--format", "json", "--output", OUT / "drc.json", FILE)
drc = json.loads((OUT / "drc.json").read_text(encoding="utf-8"))
if drc["violations"] or drc["unconnected_items"]:
    raise RuntimeError("Export gate: fix the saved board's DRC/unconnected items first")
board = pcb.LoadBoard(str(FILE))
if board.GetCopperLayerCount() != 4 or abs(mm(board.GetDesignSettings().GetBoardThickness()) - 1.6) > 0.001:
    raise RuntimeError("Expected four copper layers and nominal 1.6 mm thickness")
fps = {f.GetReference(): f for f in board.GetFootprints()}
edges = [v for item in board.GetDrawings() if item.GetLayer() == pcb.Edge_Cuts
         for v in (item.GetStart(), item.GetEnd())]
left, right = min(mm(v.x) for v in edges), max(mm(v.x) for v in edges)
top, bottom = min(mm(v.y) for v in edges), max(mm(v.y) for v in edges)
width, height = right - left, bottom - top
origin = board.GetDesignSettings().GetAuxOrigin()
if abs(mm(origin.x) - left) > 0.001 or abs(mm(origin.y) - bottom) > 0.001:
    raise RuntimeError("Gerber/drill/CPL origin must be the same PCB lower-left corner")
centers = [(mm(fps[f"SW{i}"].GetPosition().x), mm(fps[f"SW{i}"].GetPosition().y))
           for i in range(1, 5)]
joy = (mm(fps["JS1"].GetPosition().x), mm(fps["JS1"].GetPosition().y))
mic = (mm(fps["J3"].GetPosition().x) - 3.81, mm(fps["J3"].GetPosition().y) + 2.54)
key_size, mic_radius, motion_radius = 18, 9.75, 15
boundary, half_gap = 76, 2


def body_bounds(fp):
    boxes = [s.GetBoundingBox() for s in fp.GraphicalItems()
             if isinstance(s, pcb.PCB_SHAPE) and s.GetLayer() == pcb.F_Fab]
    if not boxes:
        raise RuntimeError(f"Missing F.Fab body geometry: {fp.GetReference()}")
    return (min(mm(b.GetLeft()) for b in boxes),
            min(mm(b.GetTop()) for b in boxes),
            max(mm(b.GetRight()) for b in boxes),
            max(mm(b.GetBottom()) for b in boxes))


terminal_bodies = {r: body_bounds(fps[r]) for r in ("J7", "J8", "J9")}
checks = {
    "key_pitch_x_mm": centers[1][0] - centers[0][0],
    "key_pitch_y_mm": centers[2][1] - centers[0][1],
    "key_lower_pitch_x_mm": centers[3][0] - centers[2][0],
    "key_right_pitch_y_mm": centers[3][1] - centers[1][1],
    "key_lower_stagger_mm": centers[0][0] - centers[2][0],
    "keycap_gap_mm": centers[1][0] - centers[0][0] - key_size,
    "rtc_esp_body_gap_mm": 18.03 - 15,
    "amp_keyboard_gap_mm": centers[0][1] - key_size / 2 - 72,
    "mic_left_of_upper_key_gap_mm": centers[0][0] - key_size / 2 - mic[0] - mic_radius,
    "mic_above_lower_key_gap_mm": centers[2][1] - key_size / 2 - mic[1] - mic_radius,
    "joystick_mic_motion_gap_mm": hypot(joy[0] - mic[0], joy[1] - mic[1]) - motion_radius - mic_radius,
    "mic_red_strip_gap_mm": mic[1] - mic_radius - boundary,
    "joystick_red_strip_gap_mm": joy[1] - motion_radius - boundary,
    "terminal_red_strip_gap_mm": boundary - max(b[3] for b in terminal_bodies.values()),
    "joystick_motion_board_left_gap_mm": joy[0] - motion_radius - left,
    "esp_body_board_right_gap_mm": right - 88.5,
}
for name in ("key_pitch_x_mm", "key_pitch_y_mm", "key_lower_pitch_x_mm", "key_right_pitch_y_mm"):
    if abs(checks[name] - 19.05) > 0.001:
        raise RuntimeError(f"Nonstandard keyboard pitch: {name}")
if abs(checks["key_lower_stagger_mm"] - 9.525) > 0.001:
    raise RuntimeError("Lower keys must retain the half-unit stagger")
for x, y in centers:
    if min(x-key_size/2-left, right-x-key_size/2, y-key_size/2-top, bottom-y-key_size/2) < 2:
        raise RuntimeError("Keycap envelope too close to PCB edge")
for name in ("rtc_esp_body_gap_mm", "amp_keyboard_gap_mm",
             "mic_left_of_upper_key_gap_mm", "mic_above_lower_key_gap_mm",
             "joystick_mic_motion_gap_mm", "mic_red_strip_gap_mm",
             "joystick_red_strip_gap_mm", "terminal_red_strip_gap_mm",
             "joystick_motion_board_left_gap_mm", "esp_body_board_right_gap_mm"):
    if checks[name] < 2 - 0.001:
        raise RuntimeError(f"Mechanical reservation violated: {name}={checks[name]}")
if fps["JS1"].GetOrientationDegrees() % 180 != 90:
    raise RuntimeError("Joystick must be vertical")
if fps["J7"].GetOrientationDegrees() % 360 != 90 or any(
        fps[r].GetOrientationDegrees() % 360 != 270 for r in ("J8", "J9")):
    raise RuntimeError("Wire entries must face the nearest outer side")
if any(p.GetNetname() for p in fps["JS1"].Pads() if p.GetNumber().startswith("SW")):
    raise RuntimeError("Joystick press must remain disconnected")
if next(p for p in fps["J2"].Pads() if p.GetNumber() == "6").GetNetname():
    raise RuntimeError("GPIO42 must remain free")
expected = {
    ("J1", "4"): "BUTTON1", ("J1", "5"): "RECORD", ("J1", "6"): "BUTTON3",
    ("J1", "7"): "BUTTON4", ("J1", "8"): "RTC_SDA", ("J1", "9"): "RTC_SCL",
    ("J1", "10"): "MIC_SD", ("J2", "17"): "AMP_SD", ("J2", "18"): "AMP_GAIN",
    ("JS1", "X2"): "GPIO1", ("JS1", "Y2"): "GPIO2",
    ("R1", "1"): "USB_CC1", ("R2", "1"): "USB_CC2",
    ("R1", "2"): "GND", ("R2", "2"): "GND",
}
for (ref, number), name in expected.items():
    if next(p for p in fps[ref].Pads() if p.GetNumber() == number).GetNetname() != name:
        raise RuntimeError(f"Pin assignment mismatch: {ref}/{number}")

run("pcb", "export", "gerbers", "--output", GERBERS,
    "--layers", "F.Cu,In1.Cu,In2.Cu,B.Cu,F.Mask,B.Mask,F.SilkS,B.SilkS,F.Paste,Edge.Cuts",
    "--subtract-soldermask", "--use-drill-file-origin", "--check-zones", FILE)
run("pcb", "export", "drill", "--output", GERBERS, "--format", "excellon",
    "--excellon-units", "mm", "--excellon-separate-th", "--drill-origin", "plot", "--generate-map",
    "--map-format", "svg", FILE)

bom = [
    ["1x22 female socket 2.54mm", "J1,J2", "PinSocket_1x22_P2.54mm_Vertical", 2,
     "", "", "THT; exact height/MPN requires review"],
    ["1x3 female socket 2.54mm", "J3A,J3B", "Two independent 1x3 strips, row 7.62mm", 2,
     "", "", "THT; NOT a standard 2x3 header"],
    ["1x7 female socket 2.54mm", "J4", "PinSocket_1x07_P2.54mm_Vertical", 1, "", "", "THT"],
    ["1x6 female socket 2.54mm", "J5", "PinSocket_1x06_P2.54mm_Vertical", 1, "", "", "THT"],
    ["1x14 female socket 2.54mm", "J6", "PinSocket_1x14_P2.54mm_Vertical", 1, "", "", "THT"],
    ["3-way screw terminal 5.08mm", "J7", "Phoenix MKDS-1,5-3-5.08", 1, "", "", "THT; wire entry RIGHT; MPN not selected"],
    ["2-way screw terminal 5.08mm", "J8,J9", "Phoenix MKDS-1,5-2-5.08", 2, "", "", "THT; wire entry LEFT; MPN not selected"],
    ["CPG151101D13", "SW1,SW2,SW3,SW4", "Minibox:CPG151101D13", 4, "C49234235",
     "HanElectricity CPG151101D13", "THT; exact user-selected part; no hot-swap socket"],
    ["YV13S-L7.85-B10Ka(60)-0-DL01", "JS1", "Minibox:YV13S_L7.85_B10Ka_60_0_DL01",
     1, "C37323747", "YTL YV13S-L7.85-B10Ka(60)-0-DL01", "THT; press unused; mounting leads NC"],
    ["5.1k 1% 0402", "R1,R2", "R_0402_1005Metric", 2, "C25905",
     "UNI-ROYAL 0402WGF5101TCE", "SMT TOP; actual domestic factory stock must be checked"],
]
csv_file("bom-all-review.csv",
         ["Comment", "Designator", "Footprint", "Quantity", "LCSC Part #", "Manufacturer Part", "Notes"], bom)
csv_file("bom-smt-review.csv", ["Comment", "Designator", "Footprint", "LCSC Part #"],
         [["5.1k 1%", "R1,R2", "0402 (1005 metric)", "C25905"]])
csv_file("cpl-smt.csv", ["Designator", "Mid X", "Mid Y", "Layer", "Rotation"],
         [[r, f"{mm(fps[r].GetPosition().x)-left:.4f}",
           f"{bottom-mm(fps[r].GetPosition().y):.4f}", "Top",
           f"{fps[r].GetOrientationDegrees()%360:.2f}"] for r in ("R1", "R2")])
pin_rows, positions = [], []
for ref, fp in sorted(fps.items()):
    if ref.startswith("H"):
        continue
    groups = [("J3A", [p for p in fp.Pads() if p.GetNumber() in ("1", "3", "5")]),
              ("J3B", [p for p in fp.Pads() if p.GetNumber() in ("2", "4", "6")])] if ref == "J3" else [
                  (ref, list(fp.Pads()))]
    for designator, pads in groups:
        pins = [p for p in pads if p.GetAttribute() != pcb.PAD_ATTRIB_NPTH]
        positions.append([designator, f"{sum(mm(p.GetPosition().x) for p in pins)/len(pins)-left:.4f}",
                          f"{bottom-sum(mm(p.GetPosition().y) for p in pins)/len(pins):.4f}",
                          "Top", f"{fp.GetOrientationDegrees()%360:.2f}",
                          "SMT" if ref.startswith("R") else "THT"])
        for p in pads:
            pin_rows.append([designator, ref, p.GetNumber(), p.GetNetname() or "NC",
                             f"{mm(p.GetPosition().x)-left:.4f}",
                             f"{bottom-mm(p.GetPosition().y):.4f}",
                             f"{mm(p.GetDrillSize().x):.3f}",
                             "NPTH" if p.GetAttribute() == pcb.PAD_ATTRIB_NPTH else
                             "SMD" if p.GetAttribute() == pcb.PAD_ATTRIB_SMD else "PTH"])
csv_file("positions-all-review.csv", ["Designator", "Mid X", "Mid Y", "Layer", "Rotation", "Process"], positions)
csv_file("pin-map.csv", ["Designator", "PCB footprint", "PCB pad", "Net",
                        "X mm", "Y mm", "Drill mm", "Type"], pin_rows)

svg = ['<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 110 129">',
       '<style>text{font:2px sans-serif} .label{font-weight:bold}</style>',
       f'<rect x="{left}" y="{top}" width="{width}" height="{height}" rx="5" fill="#edf6ed" stroke="#213b2a" stroke-width=".35"/>',
       '<rect x="5" y="74" width="94.5" height="4" fill="#fbe3e3"/>',
       '<path d="M9 76H95" stroke="#d93939" stroke-width=".3"/>',
       '<text x="36" y="75.3">2mm + 2mm NO BODY STRIP</text>']


def rect(bounds, label, color):
    a, b, c, d = bounds
    svg.extend([f'<rect x="{a}" y="{b}" width="{c-a}" height="{d-b}" fill="{color}" fill-opacity=".55" stroke="#24415c" stroke-width=".25"/>',
                f'<text class="label" x="{(a+c)/2}" y="{(b+d)/2}" text-anchor="middle">{html.escape(label)}</text>'])


rect((20, 18.03, 88.5, 45.97), "ESP32 USB LEFT / ANTENNA RIGHT", "#b9d8ef")
rect((81, 20.6, right, 42.1), "RF NO COPPER", "#facb88")
rect((60.5, 3, 88, 15), "RTC + BATTERY 12mm MAX", "#d3b6eb")
rect((35, 48, 62, 72), "MAX98357 27x24 reservation", "#b9d8ef")
for r, b in terminal_bodies.items():
    rect(b, r + (" IN >" if r == "J7" else " < IN"), "#90cf9a")
for i, (x, y) in enumerate(centers, 1):
    rect((x-9, y-9, x+9, y+9), f"SW{i} GPIO{i+3}", "#b3d7eb")
svg.extend([
    f'<circle cx="{joy[0]}" cy="{joy[1]}" r="15" fill="#d3b6eb" fill-opacity=".25" stroke="#6c4596" stroke-dasharray="1 1" stroke-width=".3"/>',
    f'<text x="{joy[0]}" y="{joy[1]}" text-anchor="middle">JS1 MOTION D30</text>',
    f'<circle cx="{mic[0]}" cy="{mic[1]}" r="{mic_radius}" fill="#ffe6b3" stroke="#8b6a21" stroke-width=".25"/>',
    f'<text x="{mic[0]}" y="{mic[1]}" text-anchor="middle">MIC GPIO17</text>'])
for fp in fps.values():
    for p in fp.Pads():
        x, y = mm(p.GetPosition().x), mm(p.GetPosition().y)
        r = max(0.22, mm(p.GetDrillSize().x)/2)
        svg.append(f'<circle cx="{x}" cy="{y}" r="{r}" fill="white" stroke="#485b4e" stroke-width=".15"/>')
svg.extend([f'<text x="52" y="126" text-anchor="middle">{width:g} x {height:g} mm; envelopes, not exact module models</text>', '</svg>'])
(RENDERS / "assembly-layout.svg").write_text("\n".join(svg), encoding="utf-8")

run("pcb", "export", "svg", "--output", OUT / "assembly-pads.svg",
    "--layers", "F.Fab,F.SilkS,Edge.Cuts,Dwgs.User", "--mode-single",
    "--fit-page-to-board", "--exclude-drawing-sheet", "--sketch-pads-on-fab-layers", FILE)
gerber_zip = HERE / "fabrication" / "minibox-v1.1-gerber-review.zip"
with zipfile.ZipFile(gerber_zip, "w", zipfile.ZIP_DEFLATED) as archive:
    for p in sorted(GERBERS.iterdir()):
        if p.suffix.lower() in (".gtl", ".gbl", ".g1", ".g2", ".gtp", ".gto", ".gbo",
                                ".gts", ".gbs", ".gm1", ".drl", ".gbrjob"):
            archive.write(p, p.name)
tracks = list(board.GetTracks())
manifest = {
    "revision": "2026-10-09 v1.1", "status": "ENGINEERING_REVIEW_NOT_PRODUCTION_APPROVED",
    "board": {"width_mm": width, "height_mm": height, "nominal_thickness_mm": 1.6,
              "layers": 4, "origin_kicad_mm": [left, bottom], "coordinate_axes": "X right, Y up, top view"},
    "drc_violations": len(drc["violations"]), "unconnected_items": len(drc["unconnected_items"]),
    "mechanical_reservations": {k: round(v, 4) for k, v in checks.items()},
    "track_length_mm": round(sum(mm(t.GetLength()) for t in tracks if not isinstance(t, pcb.PCB_VIA)), 3),
    "signal_vias": sum(isinstance(t, pcb.PCB_VIA) and t.GetNetname() != "GND" for t in tracks),
    "gnd_vias": sum(isinstance(t, pcb.PCB_VIA) and t.GetNetname() == "GND" for t in tracks),
    "board_sha256": hashlib.sha256(FILE.read_bytes()).hexdigest(),
    "gerber_sha256": hashlib.sha256(gerber_zip.read_bytes()).hexdigest(),
    "needs_factory_review": [
        "Exact connector MPNs and domestic assembly stock; resistors C25905 verified, not stock confirmed",
        "Actual ESP32/header spacing, RTC battery thickness, amplifier footprint and microphone spacing",
        "Factory stackup, supply current/thermal verification, backfeed check",
        "THT solder process, actual hole tolerances and keycap/joystick cap fit",
        "JLC CPL zero-angle/pin-1 preview; THT data are review coordinates, not SMT placement instructions"],
}
(OUT / "manifest.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
print(json.dumps(manifest, ensure_ascii=False, indent=2))
