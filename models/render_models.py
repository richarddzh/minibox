"""Render the exported meshes from several angles, without re-triangulating the enclosure."""

import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from math import sqrt
import os
from pathlib import Path
import shutil
import struct
import subprocess
from tempfile import TemporaryDirectory

ROOT = Path(__file__).resolve().parent
VIEWS = {
    "bottom": {"isometric": (55, 0, 35), "top": (0, 0, 0),
               "rear": (90, 0, 180), "side": (90, 0, 90)},
    "lid": {"isometric": (55, 0, 35), "top": (0, 0, 0), "front": (90, 0, 0),
            "inside": (135, 0, 215), "side": (90, 0, 90),
            "controls": (55, 0, 20), "controls-inside": (125, 0, 200),
            "speaker": (145, 0, 195), "speaker-pockets": (135, 0, 215)},
    "assembly": {"isometric": (55, 0, 35), "top": (0, 0, 0),
                 "front": (90, 0, 0), "side": (90, 0, 90)},
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--openscad", default=shutil.which("openscad"))
    args = parser.parse_args()
    assert args.openscad, "Pass --openscad with the path to openscad.com."
    meshes = [ROOT / f"{part}.stl" for part in ("bottom", "lid")]
    inputs = [*meshes, ROOT / "minibox-enclosure.scad"]
    hashes = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}
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
module bottom_mesh() {{ color([0.66,0.73,0.79]) import({model_path("bottom.stl")}); }}
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
            center = [26, 60, 60] if speaker else [60, 40, 25] if controls else centers[part]
            distance = 105 if speaker else 205 if controls else distances[part]
            camera = ",".join(map(str, (0, 0, 0, *rotation, distance)))
            command = [str(args.openscad), "-o", str(image), "--imgsize=1200,900",
                       "--projection=o", "--camera", camera,
                       "--colorscheme=Tomorrow", "-D", 'part="none"', "-D", f'preview_part="{part}"',
                       "-D", f"preview_center={json.dumps(center)}", str(source)]
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
        assert all(hashlib.sha256(p.read_bytes()).hexdigest() == hashes[p.name] for p in inputs), (
            "Models changed during rendering; rerun after the export finishes.")
        output = ROOT / "renders"
        output.mkdir(exist_ok=True)
        for name in names:
            shutil.copy2(work / name, output / name)
        manifest = {
            "description": "Exported meshes only. Four hollow speaker sleeves are integral to the lid and use the same color. No metal nuts or other hardware are rendered.",
            "source_sha256": hashes,
            "speaker_mount_mm": {"metal_nut_diameter": 4.5, "metal_nut_length": 5,
                                 "printed_sleeve_height": 3.4, "printed_sleeve_outer_diameter": 7.1,
                                 "nut_pocket_depth": 5.2, "pocket_diameter": 4.7,
                                 "speaker_front_clearance": 0.2, "frame_relief_clearance": 0.1,
                                 "sleeve_relief_sector_angle": 100,
                                 "blind_floor": 1.2, "hole_spacing": [23, 23],
                                 "ear_thickness_estimate": 3.2, "screw_length": 6},
            "images": {name: hashlib.sha256((output / name).read_bytes()).hexdigest() for name in names},
        }
        (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"PASS: {len(jobs)} views saved in {output}.")


if __name__ == "__main__":
    main()
