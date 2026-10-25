"""Render the exported meshes from several angles, without re-triangulating the enclosure."""

import argparse
import ast
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from math import cos, radians, sin, sqrt
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
from tempfile import TemporaryDirectory

ROOT = Path(__file__).resolve().parent


def file_digest(path):
    data = path.read_bytes()
    if path.suffix in {".scad", ".svg"}:
        data = data.replace(b"\r\n", b"\n")
    return hashlib.sha256(data).hexdigest()


VIEWS = {
    "bottom": {"isometric": (55, 0, 35), "top": (0, 0, 0),
               "rear": (90, 0, 180), "side": (90, 0, 90),
               "left": (90, 0, 270),
               "speaker": (90, 0, 90), "speaker-pockets": (90, 0, 270)},
    "lid": {"isometric": (55, 0, 35), "top": (0, 0, 0), "front": (90, 0, 0),
            "inside": (135, 0, 215), "side": (90, 0, 90),
            "left": (90, 0, 270),
            "controls": (55, 0, 20), "controls-inside": (125, 0, 200)},
    "assembly": {"isometric": (55, 0, 35), "top": (0, 0, 0),
                 "front": (90, 0, 0), "side": (90, 0, 90), "rear": (90, 0, 180),
                 "left": (90, 0, 270)},
}

SECTIONS = {
    "center": ("X = 60", [[0, 1, 0, 0], [0, 0, 1, 0], [1, 0, 0, -60], [0, 0, 0, 1]],
               [-4, -116, 156, 120]),
    "right": ("X = 110", [[0, 1, 0, 0], [0, 0, 1, 0], [1, 0, 0, -110], [0, 0, 0, 1]],
              [-4, -116, 156, 120]),
    "left": ("X = 10", [[0, 1, 0, 0], [0, 0, 1, 0], [1, 0, 0, -10], [0, 0, 0, 1]],
             [-4, -116, 156, 120]),
    "speaker": ("Y = 104.8", [[1, 0, 0, 0], [0, 0, 1, 0], [0, -1, 0, 104.8], [0, 0, 0, 1]],
                [-4, -116, 128, 120]),
    "rear": ("Y = 134.8", [[1, 0, 0, 0], [0, 0, 1, 0], [0, -1, 0, 134.8], [0, 0, 0, 1]],
             [-4, -116, 128, 120]),
    "horizontal": ("Z = 55", [[1, 0, 0, 0], [0, 1, 0, 0], [0, 0, 1, -55], [0, 0, 0, 1]],
                   [-4, -140, 128, 144]),
    "switch": ("X = 48", [[0, 1, 0, 0], [0, 0, 1, 0], [1, 0, 0, -48], [0, 0, 0, 1]],
               [-4, -116, 156, 120]),
    "rtc": ("Y = 118", [[1, 0, 0, 0], [0, 0, 1, 0], [0, -1, 0, 118], [0, 0, 0, 1]],
            [-4, -116, 128, 120]),
    "shared-height": ("Z = 42", [[1, 0, 0, 0], [0, 1, 0, 0], [0, 0, 1, -42], [0, 0, 0, 1]],
                      [-4, -140, 128, 144]),
    "terminals": ("Y = 76", [[1, 0, 0, 0], [0, 0, 1, 0], [0, -1, 0, 76], [0, 0, 0, 1]],
                  [-4, -116, 128, 120]),
    "usb": ("Z = 26.6", [[1, 0, 0, 0], [0, 1, 0, 0], [0, 0, 1, -26.6], [0, 0, 0, 1]],
            [-4, -140, 128, 144]),
    "tools": ("Z = 61.8", [[1, 0, 0, 0], [0, 1, 0, 0], [0, 0, 1, -61.8], [0, 0, 0, 1]],
              [-4, -140, 128, 144]),
}


def render_sections(executable, work, environment, hashes):
    """Cut the delivered meshes, not a hand-drawn approximation of the shell."""
    source = work / "section.scad"
    source.write_text(f"""
include <minibox-enclosure.scad>
cut_matrix = [[1,0,0,0],[0,1,0,0],[0,0,1,0],[0,0,0,1]];
cut_layer = 0;
projection(cut=true) multmatrix(cut_matrix) {{
    if (cut_layer == 0) {{
        import({json.dumps(str(ROOT / "bottom.stl"))});
        import({json.dumps(str(ROOT / "lid.stl"))});
    }}
    if (cut_layer == 1) module_envelopes();
    if (cut_layer == 2) {{
        speaker_envelope(); speaker_posts();
        switch_envelope(); switch_outer_envelope();
        typec_envelopes();
        // Bodies through panel bores, excluded from the original collision envelopes.
        on_switch() cylinder(d=switch_hole_diameter-0.4,h=wall+eps);
    }}
    if (cut_layer == 3) speaker_tool_envelope();
}}
""", encoding="utf-8")
    results = {}
    for name, (label, matrix, viewbox) in SECTIONS.items():
        layers = []
        layers_to_render = [(0, "#52657c"), (1, "#13a77a"), (2, "#e69c32")]
        if name == "tools":
            layers_to_render.append((3, "#8475b9"))
        for index, color in layers_to_render:
            # These planes (or the cropped mouth view) do not show these component groups.
            # OpenSCAD 2021 reports "Projection() failed" for an off-plane, nonempty solid.
            if (name, index) in {("right", 2), ("rear", 1), ("terminals", 2), ("usb", 2)}:
                continue
            output = work / f"section-{name}-{index}.svg"
            command = [str(executable), "-o", str(output), "-D", 'part="none"',
                       "-D", f"cut_layer={index}", "-D", f"cut_matrix={json.dumps(matrix)}",
                       str(source)]
            run = subprocess.run(command, capture_output=True, text=True, timeout=300, env=environment)
            log = run.stdout + run.stderr
            assert "ERROR:" not in log and "WARNING:" not in log, f"{name}, layer {index}:\n{log}"
            if "Current top level object is empty" in log:
                assert not output.exists(), log
                continue
            assert run.returncode == 0 and output.exists(), log
            svg = output.read_text(encoding="utf-8")
            paths = re.findall(r"<path\b[^>]*?/>", svg, flags=re.S)
            assert paths, f"No section contours: {output}"
            for path in paths:
                path = re.sub(r'\s(?:stroke|fill|stroke-width)="[^"]*"', "", path)
                layers.append(f'<g fill="{color}" fill-opacity="0.8" stroke="{color}" '
                              f'stroke-width="0.18">{path}</g>')
        filename = f"section-{name}.svg"
        grid = "".join(
            f'<path d="M {x},0 V -150" stroke="#d8e2ec" stroke-width="0.12"/>'
            for x in range(0, 141, 10))
        grid += "".join(
            f'<path d="M 0,{-y} H 140" stroke="#d8e2ec" stroke-width="0.12"/>'
            for y in range(0, 151, 10))
        ruler = "".join(
            f'<text x="{x}" y="3" font-family="sans-serif" font-size="2.5" '
            f'text-anchor="middle" fill="#52657c">{x}</text>'
            for x in range(0, int(viewbox[2] - 4), 20))
        ruler += "".join(
            f'<text x="1" y="{-y - 1}" font-family="sans-serif" font-size="2.5" '
            f'fill="#52657c">{y}</text>' for y in range(20, 151, 20))
        text = (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="{" ".join(map(str, viewbox))}" '
                f'role="img" aria-label="{label}: actual mesh section in millimeters">'
                f'<rect x="{viewbox[0]}" y="{viewbox[1]}" width="{viewbox[2]}" height="{viewbox[3]}" fill="#f7f9fc"/>'
                + grid + "".join(layers) + ruler + "</svg>\n")
        (work / filename).write_text(text, encoding="utf-8")
        results[filename] = file_digest(work / filename)
    assert all(file_digest(ROOT / name) == digest
               for name, digest in hashes.items()), "Models changed during section rendering."
    for filename in results:
        shutil.copy2(work / filename, ROOT / "renders" / filename)
    return results


def report_data():
    text = (ROOT / "minibox-enclosure.scad").read_text(encoding="utf-8")
    names = ("case_width", "case_depth", "deck_depth", "deck_height", "slope_base_height",
             "slope_angle", "slope_length", "wall", "floor_thickness", "panel_thickness",
             "screen_shift_along", "tft_size", "tft_hole_spacing", "speaker_center",
             "switch_center", "speaker_size", "speaker_tolerance", "typec_center_x",
             "screen_back_edge_margin", "screen_back_height", "screen_header_size",
             "esp32_stack_height", "rtc_stack_height", "socket_height", "amplifier_above_socket",
             "amplifier_wire_clearance", "joystick_total_height", "microphone_above_socket",
             "deck_bend_radius", "bend_radius", "support_angle", "speaker_wall_x",
             "upper_screen_support_angle", "usb_occupied_x", "usb_occupied_y", "usb_occupied_z",
             "joystick_window_diameter", "joystick_cap_base_diameter", "joystick_motion_diameter",
             "typec_center_z", "typec_inner_depth", "typec_inner_diameter")
    parameters = {}
    for name in names:
        match = re.search(rf"^{name}\s*=\s*([^;]+);", text, re.M)
        assert match, f"Missing report parameter: {name}"
        parameters[name] = ast.literal_eval(match[1])
    contact = re.search(r"^usb_wall_contact_confirmed\s*=\s*(true|false);", text, re.M)
    assert contact, "Missing USB wall-contact confirmation."
    parameters["usb_wall_contact_confirmed"] = contact[1] == "true"
    angle = radians(parameters["slope_angle"])
    center = 46 + parameters["screen_shift_along"]
    # The amplifier sets the lower-screen height; RTC separates in Y, not in Z.
    screen_low = min(
        parameters["slope_base_height"] + sin(angle) * along + cos(angle) * normal
        for along, normal in (
            (center - parameters["tft_size"][1] / 2, -9),
            (center - parameters["tft_size"][1] / 2 + parameters["screen_back_edge_margin"],
             -9 - parameters["screen_back_height"]),
            (center + parameters["tft_size"][1] / 2 - parameters["screen_header_size"][1],
             -9 - parameters["screen_header_size"][2])))
    amplifier_high = (10.6 + parameters["socket_height"] + parameters["amplifier_above_socket"] +
                      parameters["amplifier_wire_clearance"])
    assert screen_low - amplifier_high >= 1, "Screen hardware lacks amplifier clearance."
    verification = json.loads((ROOT / "verification.json").read_text(encoding="utf-8"))
    assert verification["source_sha256"]["minibox-enclosure.scad"] == file_digest(
        ROOT / "minibox-enclosure.scad"), "Stale verification."
    for filename, digest in verification["exports_sha256"].items():
        assert hashlib.sha256((ROOT / filename).read_bytes()).hexdigest() == digest, (
            f"Stale verified export: {filename}")
    data = {"parameters": parameters, "verification": verification,
            "screen_amplifier_z_gap_mm": screen_low - amplifier_high}
    (ROOT / "report-data.js").write_text(
        "window.MODEL_REPORT = " + json.dumps(data, indent=2) + ";\n", encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--openscad", default=shutil.which("openscad"))
    args = parser.parse_args()
    assert args.openscad, "Pass --openscad with the path to openscad.com."
    meshes = [ROOT / f"{part}.stl" for part in ("bottom", "lid")]
    inputs = [*meshes, ROOT / "minibox-enclosure.scad"]
    hashes = {p.name: file_digest(p) for p in inputs}
    distances = {}
    centers = {}
    for mesh in meshes:
        data = mesh.read_bytes()
        count = struct.unpack_from("<I", data, 80)[0]
        assert len(data) == 84 + 50 * count, f"Expected validated binary STL: {mesh}"
        vertices = [point for offset in range(84, len(data), 50)
                    for point in (struct.unpack_from("<3f", data, offset + n) for n in (12, 24, 36))]
        bounds = [(min(v[i] for v in vertices), max(v[i] for v in vertices)) for i in range(3)]
        diagonal = sqrt(sum((b - a) ** 2 for a, b in bounds))
        distances[mesh.stem] = diagonal * 2.8
        centers[mesh.stem] = [(a + b) / 2 for a, b in bounds]
    distances["assembly"] = max(distances.values())
    centers["assembly"] = centers["bottom"]
    model_path = lambda name: json.dumps(str(ROOT / name))
    scene = f"""
include <minibox-enclosure.scad>
preview_part = "assembly";
preview_center = [0,0,0];
preview_cutaway = false;
module bottom_mesh() {{
    color([0.66,0.73,0.79]) intersection() {{
        import({model_path("bottom.stl")});
        if (preview_cutaway) translate([-1,-1,-1]) cube([21,138,151]);
    }}
}}
module lid_mesh() {{ color([0.88,0.9,0.91]) import({model_path("lid.stl")}); }}
translate(-preview_center) {{
    if (preview_part == "bottom") bottom_mesh();
    else if (preview_part == "lid") {{
        lid_mesh();
    }}
    else {{
        bottom_mesh();
        lid_mesh();
    }}
}}
"""
    with TemporaryDirectory(prefix="minibox-renders-") as directory:
        work = Path(directory)
        source = work / "scene.scad"
        source.write_text(scene, encoding="utf-8")
        environment = dict(os.environ)
        environment["OPENSCADPATH"] = os.pathsep.join(
            [str(ROOT), environment.get("OPENSCADPATH", "")])

        def render(job):
            part, view, rotation = job
            name = f"{part}-{view}.png"
            image = work / name
            controls = view.startswith("controls")
            speaker = view.startswith("speaker")
            center = [4, 104.8, 50.3] if speaker else [60, 40, 25] if controls else centers[part]
            distance = 105 if speaker else 205 if controls else distances[part]
            camera = ",".join(map(str, (0, 0, 0, *rotation, distance)))
            if speaker:
                camera = "150,-60,45,0,0,0" if view == "speaker-pockets" else "-180,-60,45,0,0,0"
            elif view == "side":
                camera = f"{distance},{-distance/6},{distance/8},0,0,0"
            elif view == "left":
                camera = f"{-distance},{-distance/6},{distance/8},0,0,0"
            elif view == "isometric":
                camera = f"{-distance*.6},{-distance*.7},{distance*.55},0,0,0"
            command = [str(args.openscad), "-o", str(image), "--imgsize=1200,900",
                       "--projection=o", "--camera", camera,
                       "--colorscheme=Tomorrow", "-D", 'part="none"', "-D", f'preview_part="{part}"',
                       "-D", f"preview_cutaway={'true' if view == 'speaker-pockets' else 'false'}",
                       "-D", f"preview_center={json.dumps(center)}", str(source)]
            if speaker:
                command.insert(1, "--render")
            result = subprocess.run(command, capture_output=True, text=True,
                                    timeout=300, env=environment)
            log = result.stdout + result.stderr
            assert result.returncode == 0 and "ERROR:" not in log and "WARNING:" not in log, log
            data = image.read_bytes()
            assert data[:8] == b"\x89PNG\r\n\x1a\n" and struct.unpack(">II", data[16:24]) == (1200, 900)
            print(f"Rendered {name}", flush=True)
            return name

        jobs = [(p, v, rotation) for p, views in VIEWS.items() for v, rotation in views.items()]
        with ThreadPoolExecutor(max_workers=2) as pool:
            names = list(pool.map(render, jobs))
        assert all(file_digest(p) == hashes[p.name] for p in inputs), (
            "Models changed during rendering; rerun after the export finishes.")
        output = ROOT / "renders"
        output.mkdir(exist_ok=True)
        for name in names:
            shutil.copy2(work / name, output / name)
        sections = render_sections(args.openscad, work, environment, hashes)
        manifest = {
            "text_hash_normalization": "SCAD and SVG use LF line endings; binary meshes and images use raw bytes.",
            "description": "V2 two-part enclosure with a direct display aperture, left speaker and rear triangle. Speaker-pockets clips X>20 to show the inner mounting sleeves; other PNGs are uncut. All PNGs show only the exported printing meshes.",
            "source_sha256": hashes,
            "speaker_mount_mm": {"metal_nut_diameter": 4.5, "metal_nut_length": 5,
                                 "printed_sleeve_height": 3.4, "printed_sleeve_outer_diameter": 7.1,
                                 "nut_pocket_depth": 5.2, "pocket_diameter": 4.7,
                                 "speaker_front_clearance": 0.2, "frame_relief_clearance": 0.1,
                                 "sleeve_relief_sector_angle": 100,
                                 "blind_floor": 1.2, "hole_spacing": [23, 23],
                                 "ear_thickness_estimate": 3.2, "screw_length": 6},
            "images": {name: hashlib.sha256((output / name).read_bytes()).hexdigest() for name in names},
            "sections": sections,
        }
        (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
        report_data()
    print(f"PASS: {len(jobs)} views saved in {output}.")


if __name__ == "__main__":
    main()
