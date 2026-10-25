"""Check carrier alignment, clearances and printable meshes; export STL and lossless 3MF."""

import argparse
from collections import Counter, defaultdict
import hashlib
import json
from math import ceil, dist
from pathlib import Path
import re
import shutil
import struct
import subprocess
from tempfile import TemporaryDirectory
from xml.etree import ElementTree as ET
from zipfile import ZIP_DEFLATED, ZipFile


SOURCE = Path(__file__).resolve().parents[1] / "minibox-enclosure.scad"
CHECKS = Path(__file__).with_name("carrier-clearance.scad")
INSTALLATION_CHECKS = Path(__file__).with_name("installation-clearance.scad")
BOARD = SOURCE.parents[1] / "kicad" / "minibox-carrier.kicad_pcb"
EULER = {"bottom": -32, "lid": -14, "lid-print": -14}


def render(executable, source, output, definitions=(), empty=False):
    output.unlink(missing_ok=True)
    command = [str(executable), "--export-format", "binstl", "-o", str(output)]
    for definition in definitions:
        command.extend(["-D", definition])
    result = subprocess.run(
        [*command, str(source)], capture_output=True, text=True, timeout=900
    )
    log = result.stdout + result.stderr
    assert "WARNING:" not in log and "ERROR:" not in log, log
    if empty:
        if output.exists():
            vertices = [v for triangle in read_triangles(output) for v in triangle]
            bounds = [(min(v[i] for v in vertices), max(v[i] for v in vertices)) for i in range(3)]
            log += f"\nUnexpected solid bounds (mm): {bounds}"
        assert "Current top level object is empty" in log and not output.exists(), log
    else:
        assert result.returncode == 0 and output.is_file(), log
    return log


def read_triangles(path):
    if path.suffix == ".3mf":
        return read_3mf_triangles(path)
    data = path.read_bytes()
    if len(data) >= 84 and len(data) == 84 + struct.unpack_from("<I", data, 80)[0] * 50:
        return [[values[3:6], values[6:9], values[9:12]]
                for values in (struct.unpack_from("<12fH", data, offset)
                               for offset in range(84, len(data), 50))]
    vertices = [tuple(map(float, match)) for match in
                re.findall(r"vertex\s+(\S+)\s+(\S+)\s+(\S+)", data.decode("ascii"))]
    assert vertices and len(vertices) % 3 == 0, f"Invalid STL: {path}"
    return [vertices[i:i + 3] for i in range(0, len(vertices), 3)]


def read_3mf_triangles(path):
    ns = {"m": "http://schemas.microsoft.com/3dmanufacturing/core/2015/02"}
    with ZipFile(path) as archive:
        assert archive.testzip() is None, f"Corrupt 3MF archive: {path}"
        model = ET.fromstring(archive.read("3D/3dmodel.model"))
    assert model.get("unit") == "millimeter", f"Incorrect 3MF units: {path}"
    objects = model.findall("m:resources/m:object", ns)
    items = model.findall("m:build/m:item", ns)
    assert len(objects) == len(items) == 1, f"Expected one independent 3MF part: {path}"
    assert items[0].get("objectid") == objects[0].get("id")
    identity = "1 0 0 0 1 0 0 0 1 0 0 0"
    assert list(map(float, items[0].get("transform", identity).split())) == list(map(float, identity.split()))
    assert objects[0].find("m:components", ns) is None, f"Unexpected component transform: {path}"
    vertices = [tuple(float(v.attrib[axis]) for axis in ("x", "y", "z"))
                for v in objects[0].findall("m:mesh/m:vertices/m:vertex", ns)]
    faces = [tuple(int(f.attrib[key]) for key in ("v1", "v2", "v3"))
             for f in objects[0].findall("m:mesh/m:triangles/m:triangle", ns)]
    assert vertices and faces, f"Empty 3MF mesh: {path}"
    assert all(0 <= i < len(vertices) for f in faces for i in f), f"Invalid 3MF indices: {path}"
    return [[vertices[i] for i in face] for face in faces]


def write_3mf(source, destination):
    # OpenSCAD 2021.01's native 3MF writer rounds nearby vertices together.
    indices = {}
    faces = [[indices.setdefault(vertex, len(indices)) for vertex in triangle]
             for triangle in read_triangles(source)]
    ns = "http://schemas.microsoft.com/3dmanufacturing/core/2015/02"
    model = ET.Element("model", {"xmlns": ns, "unit": "millimeter"})
    resources = ET.SubElement(model, "resources")
    obj = ET.SubElement(resources, "object", {"id": "1", "type": "model", "name": source.stem})
    mesh = ET.SubElement(obj, "mesh")
    vertices = ET.SubElement(mesh, "vertices")
    for vertex in indices:
        ET.SubElement(vertices, "vertex", dict(zip(("x", "y", "z"), map(repr, vertex))))
    elements = ET.SubElement(mesh, "triangles")
    for face in faces:
        ET.SubElement(elements, "triangle", dict(zip(("v1", "v2", "v3"), map(str, face))))
    build = ET.SubElement(model, "build")
    ET.SubElement(build, "item", {"objectid": "1"})
    content_types = b'''<?xml version="1.0" encoding="UTF-8"?>
<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">
<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>
<Default Extension="model" ContentType="application/vnd.ms-package.3dmanufacturing-3dmodel+xml"/>
</Types>'''
    relationships = b'''<?xml version="1.0" encoding="UTF-8"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
<Relationship Id="rel0" Type="http://schemas.microsoft.com/3dmanufacturing/2013/01/3dmodel" Target="/3D/3dmodel.model"/>
</Relationships>'''
    with ZipFile(destination, "w", compression=ZIP_DEFLATED) as archive:
        archive.writestr("[Content_Types].xml", content_types)
        archive.writestr("_rels/.rels", relationships)
        archive.writestr("3D/3dmodel.model", ET.tostring(model, encoding="utf-8", xml_declaration=True))


def triangle_normal(triangle):
    a, b, c = triangle
    u = [b[i] - a[i] for i in range(3)]
    v = [c[i] - a[i] for i in range(3)]
    return (u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2],
            u[0] * v[1] - u[1] * v[0])

def write_transformed_stl(source, destination, transform):
    triangles = read_triangles(source)
    with destination.open("wb") as stream:
        stream.write(b"Minibox rigid mesh transform".ljust(80, b"\0"))
        stream.write(struct.pack("<I", len(triangles)))
        for triangle in triangles:
            rotated = [transform(x, y, z) for x, y, z in triangle]
            normal = triangle_normal(rotated)
            stream.write(struct.pack("<12fH", *normal, *(v for p in rotated for v in p), 0))


def write_y_up(source, destination, depth):
    write_transformed_stl(source, destination, lambda x, y, z: (x, z, depth - y))


def write_lid_print(source, destination, height):
    write_transformed_stl(source, destination, lambda x, y, z: (ceil(height) - z, y - 6.5, x - 2.9))


def mesh_measurements(triangles, expected_euler, label):
    edges, directed = Counter(), Counter()
    neighbors, edge_faces, vertex_links = defaultdict(set), defaultdict(list), defaultdict(list)
    faces = set()
    volume = 0.0
    for face_index, triangle in enumerate(triangles):
        a, b, c = map(tuple, triangle)
        assert len({a, b, c}) == 3, f"Collapsed triangle in {label}"
        assert sum(v * v for v in triangle_normal(triangle)) > 1e-24, f"Zero-area triangle in {label}"
        key = tuple(sorted((a, b, c)))
        assert key not in faces, f"Duplicate triangle in {label}"
        faces.add(key)
        for vertex, pair in [(a, (b, c)), (b, (c, a)), (c, (a, b))]:
            vertex_links[vertex].append(pair)
        volume += (a[0] * (b[1] * c[2] - b[2] * c[1]) +
                   a[1] * (b[2] * c[0] - b[0] * c[2]) +
                   a[2] * (b[0] * c[1] - b[1] * c[0])) / 6
        for start, end in [(a, b), (b, c), (c, a)]:
            edges[tuple(sorted((start, end)))] += 1
            edge_faces[tuple(sorted((start, end)))].append(face_index)
            directed[(start, end)] += 1
            neighbors[start].add(end)
            neighbors[end].add(start)
    assert all(count == 2 for count in edges.values()), f"Non-manifold/open edges: {label}"
    assert all(directed[(b, a)] == n for (a, b), n in directed.items()), f"Inconsistent face orientation: {label}"
    face_neighbors = defaultdict(set)
    for first, second in edge_faces.values():
        face_neighbors[first].add(second)
        face_neighbors[second].add(first)
    visited, pending = set(), [0]
    while pending:
        face = pending.pop()
        if face not in visited:
            visited.add(face)
            pending.extend(face_neighbors[face] - visited)
    assert len(visited) == len(triangles), f"Multiple edge-connected shells: {label}"
    for links in vertex_links.values():
        ring = defaultdict(set)
        for a, b in links:
            ring[a].add(b)
            ring[b].add(a)
        assert all(len(adjacent) == 2 for adjacent in ring.values()), f"Non-manifold vertex: {label}"
        seen, pending = set(), [next(iter(ring))]
        while pending:
            vertex = pending.pop()
            if vertex not in seen:
                seen.add(vertex)
                pending.extend(ring[vertex] - seen)
        assert len(seen) == len(ring), f"Pinched vertex: {label}"
    assert len(neighbors) - len(edges) + len(triangles) == expected_euler, f"Unexpected holes/handles in {label}"
    assert volume > 0, f"Inverted or empty mesh: {label}"
    return [(min(v[i] for v in neighbors), max(v[i] for v in neighbors))
            for i in range(3)], volume


def mesh_info(path, expected_euler):
    triangles = read_triangles(path)
    bounds, volume = mesh_measurements(triangles, expected_euler, path.name)
    minimum_edge = min(dist(t[i], t[(i + 1) % 3]) for t in triangles for i in range(3))
    assert minimum_edge >= 0.001, f"Submicron edge ({minimum_edge} mm): {path}"
    normals = [triangle_normal(t) for t in triangles]
    if path.suffix == ".stl":
        data = path.read_bytes()
        if len(data) == 84 + len(triangles) * 50:
            for offset, normal in zip(range(84, len(data), 50), normals):
                stored = struct.unpack_from("<3f", data, offset)
                assert sum(a * b for a, b in zip(stored, normal)) > 0, f"Stored STL normal disagrees: {path}"
    for digits in (6, 4, 3):
        rounded = [[tuple(round(v, digits) for v in vertex) for vertex in t] for t in triangles]
        mesh_measurements(rounded, expected_euler, f"{path.name}, rounded to {digits} decimals")
        for original, triangle in zip(normals, rounded):
            assert sum(a * b for a, b in zip(original, triangle_normal(triangle))) > 0, (
                f"Triangle flips after rounding to {digits} decimals: {path}")
    print(f"{path.name}: closed, connected, precision-safe; volume={volume:.1f}; bounds={bounds}", flush=True)
    return bounds, volume


def check_board():
    text = BOARD.read_text(encoding="utf-8")
    # KiCad's canonical indentation separates top-level footprints from nested properties.
    footprints = re.findall(r'^\t\(footprint .*?(?=^\t\(|^\)$)', text, re.M | re.S)
    actual = {}
    for block in footprints:
        reference = re.search(r'\(property "Reference" "([^"]+)"', block)
        at = re.search(r'^\t\t\(at ([\d.-]+) ([\d.-]+)(?: ([\d.-]+))?\)', block, re.M)
        if reference and at:
            actual[reference[1]] = (float(at[1]), float(at[2]), float(at[3] or 0))
    expected = {
        "H1": (10, 8, 0), "H2": (94.5, 8, 0), "H3": (10, 115, 0), "H4": (94.5, 115, 0),
        "SW1": (70.85, 89.5, 0), "SW2": (89.9, 89.5, 0),
        "SW3": (62.75, 108.55, 0), "SW4": (81.8, 108.55, 0),
        "JS1": (16.8, 95.5, -90), "J3": (48.265, 87.71, 0),
        "J1": (80, 19.3, -90), "J2": (80, 44.7, -90), "J4": (40, 52, 0),
        "J5": (63, 10, 90), "J6": (23, 10, 90), "J7": (88, 65, 90),
        "J8": (11.5, 48.56, -90), "J9": (11.5, 61.06, -90),
    }
    for ref, position in expected.items():
        assert ref in actual and dist(actual[ref], position) < 0.001, (
            f"PCB {ref} moved: {actual.get(ref)}; update enclosure and its independent probes.")
    assert re.search(r'\(thickness 1\.6\)', text), "PCB thickness changed."
    for block in footprints:
        if re.search(r'\(property "Reference" "H[1-4]"', block):
            assert '(drill 3)' in block and 'np_thru_hole' in block, "Carrier hole changed."
    drawings = re.findall(r'^\t\(gr_\w+\n.*?(?=^\t\(|^\)$)', text, re.M | re.S)
    edges = [b for b in drawings if '(layer "Edge.Cuts")' in b]
    assert len(edges) == 8 and sum('(gr_arc' in b for b in edges) == 4, "PCB outline changed."
    points = []
    midpoints = []
    for block in edges:
        points.extend(tuple(map(float, p)) for p in
                      re.findall(r'\((?:start|end) ([\d.-]+) ([\d.-]+)\)', block))
        midpoints.extend(tuple(map(float, p)) for p in
                         re.findall(r'\(mid ([\d.-]+) ([\d.-]+)\)', block))
    assert set(points) == {(5, 8), (10, 3), (94.5, 3), (99.5, 8),
                           (99.5, 115), (94.5, 120), (10, 120), (5, 115)}, "PCB edge coordinates changed."
    for expected_mid in [(6.464466, 4.464466), (98.035533, 4.464466),
                         (6.464466, 118.535533), (98.035533, 118.535533)]:
        assert any(dist(p, expected_mid) < 0.001 for p in midpoints), "PCB corner radius changed."
    print("PASS: saved PCB outline, footprint positions, rotations and mounting drills.", flush=True)


def check_clearances(executable, output):
    text = CHECKS.read_text(encoding="utf-8")
    sections = re.split(r"^// CHECK_GROUP ([a-z-]+)\n", text, flags=re.M)
    assert len(sections) == 9
    print("Checking openings, mounts, complete assembly, purchased-cap envelope and shell...", flush=True)
    # One invocation shares the expensive shell CSG cache across all independent probes.
    render(executable, CHECKS, output / "errors-clearance.stl", ['part="none"'], empty=True)
    print("PASS: all enclosure clearances and positive material probes.", flush=True)


def check_installation(executable, output, bottom_mesh):
    print("Checking continuous PCB insertion, RTC insertion and front-seat walls...", flush=True)
    render(executable, INSTALLATION_CHECKS, output / "errors-installation.stl",
           ['part="none"', f"mesh_path={json.dumps(str(bottom_mesh))}"], empty=True)
    print("PASS: continuous insertion route and retained front-seat material.", flush=True)


def check_speaker_mounts(executable, output, bottom_mesh):
    print("Checking exported integral speaker sleeves and nut cavities...", flush=True)
    probes = output / "speaker-mounts.scad"
    probes.write_text(f"""
include <{SOURCE.as_posix()}>
module reference_mounts() {{
    for (y = [93.3, 116.3], z = [38.8, 61.8])
        multmatrix([[0,0,-1,0],[1,0,0,y],[0,1,0,z],[0,0,0,1]]) children();
}}
module shell_mesh() {{ import({json.dumps(str(bottom_mesh))}); }}
module nut_ring(d, bore, h) {{
    difference() {{
        cylinder(d = d, h = h, $fn = 32);
        translate([0,0,-0.02]) cylinder(d = bore, h = h + 0.04, $fn = 32);
    }}
}}
// Independent nominal coordinates; 0.02 mm avoids float32/contact noise.
intersection() {{
    shell_mesh();
    reference_mounts() translate([0,0,-2.98]) cylinder(d = 4.66, h = 1.76, $fn = 32);
}}
intersection() {{
    shell_mesh();
    reference_mounts() translate([0,0,-6.38]) cylinder(d = 4.66, h = 5.16, $fn = 32);
}}
intersection() {{
    shell_mesh();
    reference_mounts() {{
        translate([0,0,-6.52]) cylinder(d=8.2,h=0.1,$fn=32);
        translate([0,0,-6.38]) nut_ring(8.2,7.16,3.36);
    }}
}}
difference() {{
    reference_mounts() {{
        translate([0,0,-1.18]) cylinder(d = 4.4, h = 1.16, $fn = 32);
        translate([0,0,-2.98]) nut_ring(5.7, 4.9, 1.76);
    }}
    shell_mesh();
}}
difference() {{
    difference() {{
        reference_mounts() translate([0,0,-6.38]) nut_ring(6.94, 4.9, 3.36);
        for (y=[93.3,116.3],z=[38.8,61.8])
            multmatrix([[0,0,-1,0],[1,0,0,y],[0,1,0,z],[0,0,0,1]])
                translate([0,0,-6.5])
                    rotate([0,0,atan2(50.3-z,104.8-y)])
                        linear_extrude(height=3.7)
                            polygon([[0,0],[10*cos(51),10*sin(51)],[10*cos(51),-10*sin(51)]]);
    }}
    shell_mesh();
}}
intersection() {{
    shell_mesh();
    multmatrix([[0,0,-1,0],[1,0,0,104.8],[0,1,0,50.3],[0,0,0,1]])
        translate([0,0,-6.38])
            cylinder(d=28.1,h=3.16,$fn=128);
}}
difference() {{
    reference_mounts() translate([0,0,-6.38]) nut_ring(4.46, 2.54, 4.96);
    speaker_posts();
}}
difference() {{
    speaker_posts();
    reference_mounts() translate([0,0,-6.42]) nut_ring(4.54, 2.46, 5.04);
}}
""", encoding="utf-8")
    render(executable, probes, output / "errors-speaker-mounts.stl",
           ['part="none"'], empty=True)
    print("PASS: four integral 7.1 x 3.4 hollow sleeves, coaxial 4.7 x 5.2 nut cavities, "
          "1.2 blind floors, ear-seat height and round-frame relief (0.02 mm probe tolerance).",
          flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--openscad", default=shutil.which("openscad"))
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--checks-only", action="store_true")
    parser.add_argument("--export-only", action="store_true",
                        help="Skip assembly/insertion checks; keep all printable-mesh checks.")
    parser.add_argument("--3mf-only", action="store_true", dest="three_mf_only")
    args = parser.parse_args()
    assert not (args.checks_only and args.export_only), "Choose checks-only or export-only."
    assert args.openscad, "Pass --openscad with the path to openscad.com."
    args.output.mkdir(parents=True, exist_ok=True)
    sources = (SOURCE, CHECKS, INSTALLATION_CHECKS, BOARD)
    source_hashes = {p: hashlib.sha256(p.read_bytes().replace(b"\r\n", b"\n")).hexdigest()
                     for p in sources}
    check_board()
    with TemporaryDirectory(prefix="minibox-models-") as temporary:
        work = Path(temporary)
        if args.export_only:
            print("Export-only: skipping assembly/insertion checks; printable-mesh checks remain enabled.", flush=True)
        if not args.three_mf_only and not args.export_only:
            check_clearances(args.openscad, work)
        if args.checks_only:
            bottom = work / "bottom.stl"
            render(args.openscad, SOURCE, bottom, ['part="bottom"'])
            mesh_info(bottom, EULER["bottom"])
            check_installation(args.openscad, work, bottom)
            lid = work / "lid.stl"
            render(args.openscad, SOURCE, lid, ['part="lid"'])
            mesh_info(lid, EULER["lid"])
            check_speaker_mounts(args.openscad, work, bottom)
            return
        measurements = {}
        for part, euler in EULER.items():
            definitions = [f'part="{part}"']
            path = work / f"{part}.stl"
            if part == "lid-print":
                write_lid_print(work / "lid.stl", path, measurements["lid"][0][2][1])
            else:
                render(args.openscad, SOURCE, path, definitions)
            measurements[part] = mesh_info(path, euler)
            if part == "bottom":
                check_speaker_mounts(args.openscad, work, path)
            if part == "bottom" and not args.three_mf_only and not args.export_only:
                check_installation(args.openscad, work, path)
            if part != "lid-print":
                dest = work / f"{part}.3mf"
                write_3mf(path, dest)
                mesh_info(dest, euler)
                assert read_triangles(dest) == read_triangles(path), "3MF altered the mesh."
        bounds = measurements["bottom"][0]
        for maximum, size in zip((b[1] for b in bounds), (120, 136, 109.804)):
            assert abs(maximum - size) < 0.01, f"Unexpected case dimensions: {bounds}"
        assert abs(bounds[2][0]) < 0.001
        assert abs(measurements["lid-print"][0][2][0]) < 0.001
        assert abs(measurements["lid"][1] - measurements["lid-print"][1]) < 1
        for part, original in [("bottom-y-up", "bottom"), ("lid-y-up", "lid-print")]:
            path = work / f"{part}.stl"
            old_bounds, old_volume = measurements[original]
            write_y_up(work / f"{original}.stl", path, old_bounds[1][1])
            new_bounds, volume = mesh_info(path, EULER[original])
            expected = [old_bounds[0], old_bounds[2], (0, old_bounds[1][1] - old_bounds[1][0])]
            assert all(dist(a, b) < 0.001 for a, b in zip(new_bounds, expected)), "Wrong Y-up transform."
            assert abs(volume - old_volume) < 1 and abs(new_bounds[1][0]) < 0.001
        # Do not overwrite usable deliveries until all checks and all exports succeed.
        assert all(hashlib.sha256(p.read_bytes().replace(b"\r\n", b"\n")).hexdigest() == source_hashes[p]
                   for p in sources), (
            "Geometry or PCB changed during verification; rerun before publishing.")
        extensions = (".3mf",) if args.three_mf_only else (".stl", ".3mf")
        for path in work.iterdir():
            if path.suffix in extensions and path.stem != "lid-print":
                shutil.copy2(path, args.output / path.name)
        exports = [p for p in work.iterdir() if p.suffix in extensions and p.stem != "lid-print"]
        verification = {
            "text_hash_normalization": "Source text uses LF line endings; binary exports use raw bytes.",
            "full_assembly_checks": not (args.export_only or args.three_mf_only),
            "compatibility_limits": {
                "usb_normal_straight_cable_confirmed": False,
                "usb_current_cable_insertion_user_confirmed": True,
                "usb_left_wall_contact_user_confirmed": True,
                "usb_note": "User confirmed their current cable inserts and touches the left wall. Reserve the complete X=2.4..27.75 occupied band; zero wall clearance is an intentional contact, not a guaranteed air gap. Do not infer their cable dimensions from a different manufacturer's drawing.",
                "panel_typec_note": "30 mm x 12 mm inner body/tail is a design acceptance envelope, not a verified product drawing.",
                "joystick_note": "User measured the cap bottom approximately 26.4 mm and requested a 30 mm bore. 29 mm motion is a design acceptance limit, not a measured sweep."
            },
            "source_sha256": {p.name: digest for p, digest in source_hashes.items()},
            "exports_sha256": {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in exports},
            "measurements": {part: {"bounds_mm": bounds, "volume_mm3": volume}
                             for part, (bounds, volume) in measurements.items()},
            "checks": ["carrier alignment", "static assembly and key travel",
                       "uncut vertical riser and no inclined control cuts",
                       "screen thickness, rear switch depth and upright RTC",
                       "speaker and screen pillar/brace clearance to sockets and terminals",
                       "screen connector clearance to rear wall, speaker and RTC",
                       "1 mm component envelopes", "continuous PCB and RTC insertion",
                       "left-wall speaker seats and blind pockets; former right wall closed",
                       "rear triangle, Type-C nuts vs speaker/wire, switch and rear mounts",
                       "user-confirmed USB wall-contact band; 1 mm clearance to internal hardware, not to the contacted wall",
                       "four 4 mm driver tips plus 8 x 40 mm working corridors",
                       "30 mm joystick bore and reclaimed former 35 mm bore material",
                       "recentered triangle and full 30 mm panel Type-C inner-body reservation",
                       "direct display aperture in the original 3 mm lid; no added rails or outward frame",
                       "closed connected meshes, 0.001 mm precision", "lossless STL/3MF identity"]
                      if not (args.export_only or args.three_mf_only) else ["mesh export only"],
        }
        (args.output / "verification.json").write_text(
            json.dumps(verification, indent=2) + "\n", encoding="utf-8")
    print("PASS: two-part enclosure exported. Proportional speaker-ear dimensions and unselected hardware still need first-sample fit checks.")


if __name__ == "__main__":
    main()
