"""Align JLC library datums to the saved PCB; never rotate the PCB itself."""

import csv
import hashlib
import io
import json
from math import cos, hypot, radians, sin
from pathlib import Path

import pcbnew as pcb


HERE = Path(__file__).resolve().parent
BOARD = HERE / "minibox-carrier.kicad_pcb"
OUT = HERE / "assembly"

# JLC library head-relative pads, in millimetres, X right/Y up, read 2026-10-09.
SOCKETS = {
    "J1": (22, "C54973843"), "J2": (22, "C54973843"),
    "J3A": (3, "C54973828"), "J3B": (3, "C54973828"),
    "J4": (7, "C54973832"), "J5": (6, "C54973826"),
    "J6": (14, "C54973850"),
}
JOY_PADS = [
    ("X1", -1.740103, -7.512329), ("X2", 0.760019, -7.512329),
    ("X3", 3.259887, -7.512329), ("Y1", 9.489999, -1.312443),
    ("Y2", 9.489999, 1.187679), ("Y3", 9.489999, 3.687547),
    ("SWB", -4.990033, 4.437609), ("SWD", -4.990033, -2.062251),
    ("SWA", -9.489897, 4.437609), ("SWC", -9.489897, -2.062251),
    ("M3", -4.239971, -5.137429), ("M4", 5.760009, -5.137429),
    ("M2", 5.760009, 7.512533), ("M1", -4.239971, 7.512533),
]


def library_pads(ref):
    if ref in SOCKETS:
        count, code = SOCKETS[ref]
        numbers = ("1", "3", "5") if ref == "J3A" else (
            ("2", "4", "6") if ref == "J3B" else tuple(map(str, range(1, count + 1))))
        return code, [(pin, (i - (count - 1) / 2) * 2.54, 0)
                      for i, pin in enumerate(numbers)], 0.001
    if ref in ("J7", "J8", "J9"):
        count = 3 if ref == "J7" else 2
        return ("C72334" if count == 3 else "C8465",
                [(str(i + 1), (i - (count - 1) / 2) * 5.08, 0)
                 for i in range(count)], 0.001)
    if ref in ("SW1", "SW2", "SW3", "SW4"):
        return "C49234235", [("1", -3.175, -1.27), ("2", 3.175, 1.27)], 0.001
    if ref == "JS1":
        return "C37323747", JOY_PADS, 0.03
    if ref in ("R1", "R2"):
        return "C25905", [("1", -0.432816, 0), ("2", 0.432816, 0)], 0.08
    raise ValueError(f"No verified JLC library mapping for {ref}")


def write_jlc_positions(board, output):
    output = Path(output)
    footprints = {fp.GetReference(): fp for fp in board.GetFootprints()}
    origin = board.GetDesignSettings().GetAuxOrigin()
    expected = set(SOCKETS) | {"J7", "J8", "J9", "JS1", "R1", "R2"} | {
        f"SW{i}" for i in range(1, 5)}
    board_refs = {ref for ref in footprints if not ref.startswith("H")}
    if board_refs != (expected - {"J3A", "J3B"}) | {"J3"}:
        raise RuntimeError("Actual PCB physical references differ from the verified JLC mapping")
    with (output / "bom-all-review.csv").open(encoding="utf-8-sig", newline="") as stream:
        codes = {}
        for row in csv.DictReader(stream):
            for ref in row["Designator"].split(","):
                if ref in codes:
                    raise RuntimeError(f"Duplicate BOM reference: {ref}")
                codes[ref] = row["LCSC Part #"]
    if set(codes) != expected:
        raise RuntimeError("Full BOM and JLC placement references differ")
    rows, checks = [], []
    for ref in sorted(expected):
        code, local, tolerance = library_pads(ref)
        if codes[ref] != code:
            raise RuntimeError(f"{ref}: library mapping is not verified for BOM part {codes[ref]}")
        fp = footprints["J3" if ref in ("J3A", "J3B") else ref]
        pads = {pad.GetNumber(): (pcb.ToMM(pad.GetPosition().x - origin.x),
                                  pcb.ToMM(origin.y - pad.GetPosition().y))
                for pad in fp.Pads() if pad.GetNumber()}
        candidates = []
        for angle in (0, 90, 180, 270):
            c, s = round(cos(radians(angle))), round(sin(radians(angle)))
            pairs = [(pin, c*x-s*y, s*x+c*y, *pads[pin]) for pin, x, y in local]
            x = sum(tx-lx for _, lx, _, tx, _ in pairs) / len(pairs)
            y = sum(ty-ly for _, _, ly, _, ty in pairs) / len(pairs)
            errors = [hypot(x+lx-tx, y+ly-ty) for _, lx, ly, tx, ty in pairs]
            candidates.append((max(errors), angle, x, y, errors))
        error, angle, x, y, errors = min(candidates)
        if error > tolerance:
            raise RuntimeError(f"{ref}: library/PCB pad residual {error:.6f} mm exceeds {tolerance}")
        rows.append([ref, f"{x:.6f}", f"{y:.6f}", "Top", f"{angle:.2f}"])
        cad_points = [pads[pin] for pin, _, _ in local]
        cad_x = sum(point[0] for point in cad_points) / len(cad_points)
        cad_y = sum(point[1] for point in cad_points) / len(cad_points)
        cad_angle = fp.GetOrientationDegrees() % 360
        checks.append({
            "reference": ref, "lcsc_part": code, "rotation_ccw_deg": angle,
            "library_origin_mm": [round(x, 6), round(y, 6)],
            "kicad_pad_center_mm": [round(cad_x, 6), round(cad_y, 6)],
            "kicad_rotation_deg": cad_angle,
            "origin_correction_mm": [round(x-cad_x, 6), round(y-cad_y, 6)],
            "rotation_correction_deg": (angle-cad_angle+180) % 360-180,
            "maximum_pad_residual_mm": round(error, 6),
            "residual_limit_mm": tolerance,
            "pcb_pads": [pin for pin, _, _ in local],
            "pad_residuals_mm": [round(value, 6) for value in errors],
        })
    stream = io.StringIO(newline="")
    writer = csv.writer(stream)
    writer.writerow(["Designator", "Mid X", "Mid Y", "Layer", "Rotation"])
    writer.writerows(rows)
    cpl_data = stream.getvalue().encode("utf-8-sig")
    report = {
        "status": "ENGINEERING_REVIEW_NOT_PRODUCTION_APPROVED",
        "coordinate_axes": "X right, Y up; rotations counterclockwise in top view",
        "library_geometry_source": "Domestic JLC library numbered pads read 2026-10-09",
        "origin_kicad_mm": [pcb.ToMM(origin.x), pcb.ToMM(origin.y)],
        "board_sha256": hashlib.sha256(BOARD.read_bytes()).hexdigest(),
        "cpl_sha256": hashlib.sha256(cpl_data).hexdigest(),
        "physical_components": len(rows),
        "placements": checks,
        "needs_factory_review": [
            "Verify website rotation convention, saved values and actual Gerber pad overlay",
            "Joystick library has up to 0.03 mm geometric residual; confirm insertion fit",
            "0402 library pad pitch differs from the PCB; confirm SMT pad overlap",
            "J4/J6 have no supplied JLC 3D model; verify numbered pads, not rendered bodies",
            "Factory THT process, mechanical orientation and DFM still require approval",
        ],
    }
    saved_path = output / "jlc-saved-placement.json"
    if saved_path.exists():
        saved = json.loads(saved_path.read_text(encoding="utf-8"))
        same_board = saved["board_sha256"] == report["board_sha256"]
        report["factory_saved_snapshot"] = {
            "file": saved_path.name, "matches_current_board": same_board,
            "verified": False,
        }
        if same_board:
            persisted = {row["designator"]: row for row in saved["placements"]}
            if set(persisted) != expected:
                raise RuntimeError("Factory-saved snapshot has a different physical reference set")
            for row in checks:
                actual = persisted[row["reference"]]
                if (actual["lcsc_part"] != row["lcsc_part"] or
                        actual["rotation_deg"] != row["rotation_ccw_deg"] or
                        [actual["x_mm"], actual["y_mm"]] != [
                            round(value, 3) for value in row["library_origin_mm"]]):
                    raise RuntimeError(f"{row['reference']}: generated mapping differs from saved factory placement")
            report["factory_saved_snapshot"]["verified"] = True
    (output / "positions-jlc-review.csv").write_bytes(cpl_data)
    (output / "jlc-placement-review.json").write_text(
        json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return report


if __name__ == "__main__":
    report = write_jlc_positions(pcb.LoadBoard(str(BOARD)), OUT)
    print(f"Exported {report['physical_components']} JLC review placements; "
          "factory overlay and DFM approval remain required")
