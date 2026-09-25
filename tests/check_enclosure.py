"""Render the enclosure and check its meshes, mounting clearances and sketch layout."""

import argparse
from collections import Counter, defaultdict
from pathlib import Path
import re
import shutil
import struct
import subprocess
from tempfile import TemporaryDirectory
from xml.etree import ElementTree
from zipfile import ZIP_DEFLATED, ZipFile


SOURCE = Path(__file__).resolve().parents[1] / "minibox-enclosure.scad"


def render(executable, source, output, definitions=(), empty=False):
    output.unlink(missing_ok=True)
    command = [str(executable), "--export-format", "binstl", "-o", str(output)]
    for definition in definitions:
        command.extend(["-D", definition])
    result = subprocess.run(
        [*command, str(source)], capture_output=True, text=True, timeout=600
    )
    log = result.stdout + result.stderr
    if "WARNING:" in log or "ERROR:" in log:
        raise AssertionError(log)
    if empty:
        assert "Current top level object is empty" in log and not output.exists(), log
    else:
        assert result.returncode == 0 and output.is_file(), log
    return log


def read_triangles(path):
    if path.suffix == ".3mf":
        return read_3mf_triangles(path)
    data = path.read_bytes()
    if len(data) >= 84 and len(data) == 84 + struct.unpack_from("<I", data, 80)[0] * 50:
        triangles = []
        for offset in range(84, len(data), 50):
            values = struct.unpack_from("<12fH", data, offset)
            triangles.append([values[3:6], values[6:9], values[9:12]])
        return triangles
    vertices = [
        tuple(map(float, match))
        for match in re.findall(
            r"vertex\s+(\S+)\s+(\S+)\s+(\S+)", data.decode("ascii")
        )
    ]
    assert vertices and len(vertices) % 3 == 0, f"Invalid STL: {path}"
    return [vertices[i : i + 3] for i in range(0, len(vertices), 3)]


def read_3mf_triangles(path):
    ns = {"m": "http://schemas.microsoft.com/3dmanufacturing/core/2015/02"}
    with ZipFile(path) as archive:
        assert archive.testzip() is None, f"Corrupt 3MF archive: {path}"
        model = ElementTree.fromstring(archive.read("3D/3dmodel.model"))
    assert model.get("unit") == "millimeter", f"Incorrect 3MF units: {path}"
    objects = model.findall("m:resources/m:object", ns)
    items = model.findall("m:build/m:item", ns)
    assert len(objects) == len(items) == 1, f"Expected one independent 3MF part: {path}"
    assert items[0].get("objectid") == objects[0].get("id")
    identity = "1 0 0 0 1 0 0 0 1 0 0 0"
    assert list(map(float, items[0].get("transform", identity).split())) == list(map(float, identity.split())), (
        f"3MF must preserve original assembly placement: {path}"
    )
    assert objects[0].find("m:components", ns) is None, f"Unexpected component transform: {path}"
    vertices = [
        tuple(float(vertex.attrib[axis]) for axis in ("x", "y", "z"))
        for vertex in objects[0].findall("m:mesh/m:vertices/m:vertex", ns)
    ]
    faces = [
        tuple(int(face.attrib[key]) for key in ("v1", "v2", "v3"))
        for face in objects[0].findall("m:mesh/m:triangles/m:triangle", ns)
    ]
    assert vertices and faces, f"Empty 3MF mesh: {path}"
    assert all(0 <= index < len(vertices) for face in faces for index in face), (
        f"Invalid 3MF vertex index: {path}"
    )
    return [[vertices[index] for index in face] for face in faces]


def write_3mf(source, destination):
    # OpenSCAD 2021.01's native 3MF writer rounds nearby vertices together.
    # Round-trip the validated STL coordinates without decimal truncation.
    triangles = read_triangles(source)
    indices = {}
    faces = []
    for triangle in triangles:
        faces.append([indices.setdefault(vertex, len(indices)) for vertex in triangle])
    ns = "http://schemas.microsoft.com/3dmanufacturing/core/2015/02"
    model = ElementTree.Element("model", {"xmlns": ns, "unit": "millimeter"})
    resources = ElementTree.SubElement(model, "resources")
    obj = ElementTree.SubElement(resources, "object", {"id": "1", "type": "model", "name": source.stem})
    mesh = ElementTree.SubElement(obj, "mesh")
    vertices = ElementTree.SubElement(mesh, "vertices")
    for vertex in indices:
        ElementTree.SubElement(vertices, "vertex", dict(zip(("x", "y", "z"), map(repr, vertex))))
    elements = ElementTree.SubElement(mesh, "triangles")
    for face in faces:
        ElementTree.SubElement(elements, "triangle", dict(zip(("v1", "v2", "v3"), map(str, face))))
    build = ElementTree.SubElement(model, "build")
    ElementTree.SubElement(build, "item", {"objectid": "1"})
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
        archive.writestr("3D/3dmodel.model", ElementTree.tostring(model, encoding="utf-8", xml_declaration=True))


def check_3mf_export(path, source, reference, euler):
    bounds, volume = mesh_info(path, euler)
    expected_bounds, expected_volume = reference
    assert abs(volume - expected_volume) < 1, f"3MF export changed volume: {path}"
    for axis in range(3):
        assert all(abs(a - b) < 0.001 for a, b in zip(bounds[axis], expected_bounds[axis])), (
            f"3MF export changed original assembly coordinates: {path}"
        )
    assert read_triangles(path) == read_triangles(source), f"3MF changed mesh coordinates or winding: {path}"


def mesh_info(path, expected_euler):
    triangles = read_triangles(path)
    edges = Counter()
    directed = Counter()
    neighbors = defaultdict(set)
    volume = 0.0
    for triangle in triangles:
        a, b, c = map(tuple, triangle)
        assert len({a, b, c}) == 3, f"Degenerate triangle in {path}"
        volume += (
            a[0] * (b[1] * c[2] - b[2] * c[1])
            + a[1] * (b[2] * c[0] - b[0] * c[2])
            + a[2] * (b[0] * c[1] - b[1] * c[0])
        ) / 6
        for start, end in [(a, b), (b, c), (c, a)]:
            edges[tuple(sorted((start, end)))] += 1
            directed[(start, end)] += 1
            neighbors[start].add(end)
            neighbors[end].add(start)
    assert all(count == 2 for count in edges.values()), f"Non-manifold edges: {path}"
    assert all(directed[(b, a)] == count for (a, b), count in directed.items()), (
        f"Inconsistent face orientation: {path}"
    )
    visited = set()
    pending = [next(iter(neighbors))]
    while pending:
        vertex = pending.pop()
        if vertex not in visited:
            visited.add(vertex)
            pending.extend(neighbors[vertex] - visited)
    assert len(visited) == len(neighbors), f"Detached supports/components: {path}"
    assert len(neighbors) - len(edges) + len(triangles) == expected_euler, (
        f"Unexpected holes/handles in {path}"
    )
    assert volume > 0, f"Inverted or empty mesh: {path}"
    bounds = [
        (min(v[i] for v in neighbors), max(v[i] for v in neighbors)) for i in range(3)
    ]
    print(f"{path.name}: closed, connected; volume={volume:.1f} mm^3; bounds={bounds}")
    return bounds, volume


def planar_area(path, axis, coordinate):
    axes = [i for i in range(3) if i != axis]
    u, v = axes
    area = 0.0
    for a, b, c in read_triangles(path):
        if all(abs(p[axis] - coordinate) < 0.001 for p in (a, b, c)):
            area += abs((b[u] - a[u]) * (c[v] - a[v]) -
                        (b[v] - a[v]) * (c[u] - a[u])) / 2
    return area


def check_y_up_exports(executable, output, meshes):
    for part, original, euler in [
        ("bottom-y-up", "bottom", -2), ("lid-y-up", "lid-print", -22)
    ]:
        path = output / f"{part}.stl"
        original_path = output / f"{original}.stl"
        render(executable, SOURCE, path, [f'part="{part}"'])
        bounds, volume = mesh_info(path, euler)
        original_bounds, original_volume = meshes[original]
        depth = original_bounds[1][1]
        expected = [original_bounds[0], original_bounds[2],
                    (0, depth - original_bounds[1][0])]
        for axis in range(3):
            assert all(abs(a - b) < 0.001 for a, b in zip(bounds[axis], expected[axis])), (
                f"Incorrect Y-up bounds: {part}"
            )
        assert abs(volume - original_volume) < 1, f"Y-up rotation changed volume: {part}"
        assert abs(bounds[1][0]) < 0.001, f"Y-up export must rest on Y=0: {part}"
        # A -90-degree rotation maps the bottom to -Y and the rear to -Z.
        for old_axis, old_coordinate, new_axis, new_coordinate in [(2, 0, 1, 0), (1, depth, 2, 0)]:
            assert abs(planar_area(original_path, old_axis, old_coordinate) -
                       planar_area(path, new_axis, new_coordinate)) < 0.1, (
                f"Wrong bottom/rear orientation: {part}"
            )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--openscad", default=shutil.which("openscad"))
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--3mf-only", action="store_true", dest="three_mf_only",
                        help="Export and verify only the two original-coordinate 3MF parts.")
    args = parser.parse_args()
    assert args.openscad, "Pass --openscad with the path to openscad.com or openscad."
    args.output.mkdir(parents=True, exist_ok=True)
    if args.three_mf_only:
        with TemporaryDirectory(prefix="minibox-3mf-") as temporary:
            for part, euler in [("bottom", -2), ("lid", -22)]:
                source = Path(temporary) / f"{part}.stl"
                render(args.openscad, SOURCE, source, [f'part="{part}"'])
                reference = mesh_info(source, euler)
                output = args.output / f"{part}.3mf"
                write_3mf(source, output)
                check_3mf_export(output, source, reference, euler)
        print("PASS: original-coordinate 3MF exports exactly preserve both validated STL meshes.")
        return
    meshes = {}
    for part, euler in [("bottom", -2), ("lid", -22), ("lid-print", -22)]:
        output = args.output / f"{part}.stl"
        render(args.openscad, SOURCE, output, [f'part="{part}"'])
        meshes[part] = mesh_info(output, euler)
    assert abs(meshes["bottom"][0][0][1] - 120) < 0.01
    assert abs(meshes["bottom"][0][1][1] - 154) < 0.01
    assert abs(meshes["bottom"][0][2][1] - 124.129) < 0.01
    assert abs(meshes["bottom"][0][2][0]) < 0.001, "Spherical corners lifted the bottom off Z=0."
    assert abs(meshes["lid-print"][0][2][0]) < 0.001, "Print orientation is not on Z=0."
    assert abs(meshes["lid"][1] - meshes["lid-print"][1]) < 1, "Print transform changed volume."
    for part, euler in [("bottom", -2), ("lid", -22)]:
        output = args.output / f"{part}.3mf"
        source = args.output / f"{part}.stl"
        write_3mf(source, output)
        check_3mf_export(output, source, meshes[part], euler)
    check_y_up_exports(args.openscad, args.output, meshes)

    checks = args.output / "clearance-check.scad"
    checks.write_text(
        f"include <{SOURCE}>\n"
        """
expected_screen_stack = 3;
assert(slope_angle == 60);
assert(abs(corner_angle(3) - 120) < 0.001 && abs(corner_angle(4) - 120) < 0.001);
assert(bottom_radius == 12 && rim_radius == 6 && bend_radius == 10);
assert(wall == 2.4 && floor_thickness == 4 && panel_thickness == 3);
assert(slope_length == 111 && norm(tft_center - [60, 72.62]) < 0.001);
assert(screen_port_clearance == 8);
assert(tft_window_radius == 6 && screen_brace_clearance == 2.5);
assert(tft_view_side_margins == [10, 14.52] && tft_window_clearance == 2);
assert(norm(screen_window_offset - [-2.26, 0]) < 0.001);
assert(screen_pcb_thickness == 2 && pcb_thickness == 1.6);
assert(screen_stack_above_pcb == expected_screen_stack && screen_front_clearance == 1);
assert(abs(screen_mount_gap - (expected_screen_stack + 3)) < 0.001);
assert(abs(screen_mount_normal + expected_screen_stack + 6) < 0.001);
assert(abs(tft_center[0] + screen_window_offset[0] - tft_view[0] / 2 -
           (tft_center[0] - tft_size[0] / 2) - 10) < 0.001);
assert(abs(tft_center[0] + tft_size[0] / 2 -
           (tft_center[0] + screen_window_offset[0] + tft_view[0] / 2) - 14.52) < 0.001);
assert(abs(tft_center[1] - tft_size[1] / 2 - speaker_center[1] -
           speaker_hole_diameter / 2 - 8) < 0.001);
// Independently probe the shifted 85.52 x 57.68 mm aperture and retained R6 corners.
intersection() {
    lid();
    on_slope(14.98 + eps, 43.78 + 6, -3.1) cube([85.52 - 2 * eps, 45.68, 3.2]);
}
intersection() {
    lid();
    on_slope(14.98 + 6, 43.78 + eps, -3.1) cube([73.52, 57.68 - 2 * eps, 3.2]);
}
// Solid strips just outside each straight edge reject excessive or misplaced cuts.
difference() {
    union() {
        for (x = [14.88, 100.55])
            on_slope(x, 72.57, -2.9) cube([0.05, 0.1, 2.8]);
        for (v = [43.68, 101.51])
            on_slope(57.69, v, -2.9) cube([0.1, 0.05, 2.8]);
    }
    lid();
}
for (sx = [-1, 1])
    for (sy = [-1, 1]) {
        difference() {
            on_slope(57.74 + sx * (42.76 - 1) - 0.05,
                     72.62 + sy * (28.84 - 2) - 0.05, -2.9)
                cube([0.1, 0.1, 2.8]);
            lid();
        }
        intersection() {
            lid();
            on_slope(57.74 + sx * (42.76 - 1) - 0.05,
                     72.62 + sy * (28.84 - 3.5) - 0.05, -3.1)
                cube([0.1, 0.1, 3.2]);
        }
    }
assert(switch_hole_diameter == 21 && switch_center == [90, 22]);
assert(speaker_diameter == 23 && speaker_hole_diameter == 23.5);
assert(microphone_hole_diameter == 5);
assert(speaker_center == [30, 22] && microphone_center == [60, 22]);
for (port = [[[30, 22], 23.5], [[60, 22], 5]]) {
    intersection() {
        lid();
        on_slope(port[0][0], port[0][1], -3.1)
            cylinder(d = port[1] - 0.02, h = 3.2);
    }
    difference() {
        on_slope(port[0][0], port[0][1], -2.9)
            difference() {
                cylinder(d = port[1] + 4, h = 2.8);
                translate([0, 0, -eps]) cylinder(d = port[1] + 0.1, h = 2.8 + 2 * eps);
            }
        lid();
    }
}
intersection() { audio_envelopes(eps); bottom_shell(); }
intersection() { audio_envelopes(eps); lid(); }
intersection() { audio_envelopes(eps); module_envelopes(eps); }
intersection() { audio_envelopes(eps); typec_envelopes(eps); }
intersection() { audio_envelopes(eps); switch_envelope(eps); }
assert(tft_center[1] - tft_size[1] / 2 - switch_center[1] -
       switch_hole_diameter / 2 >= 8);
for (port = [[speaker_center, speaker_hole_diameter],
             [microphone_center, microphone_hole_diameter], [switch_center, switch_hole_diameter]])
    assert(tft_center[1] - tft_size[1] / 2 - port[0][1] - port[1] / 2 >= 8 &&
           port[0][1] - port[1] / 2 > tangent_length(4) + 2 &&
           port[0][1] + port[1] / 2 < slope_length - tangent_length(3) - 2);
intersection() {
    lid();
    on_slope(90, 22, -3.1) cylinder(d = 20.98, h = 3.2);
}
difference() {
    on_slope(90, 22, -2.9)
        difference() {
            cylinder(d = 25, h = 2.8);
            translate([0, 0, -eps]) cylinder(d = 21.1, h = 2.8 + 2 * eps);
        }
    lid();
}
intersection() { switch_envelope(eps); bottom_shell(); }
intersection() { switch_envelope(eps); lid(); }
intersection() { switch_envelope(eps); module_envelopes(eps); }
intersection() { switch_envelope(eps); typec_envelopes(eps); }
assert(typec_hole_diameter == 12 && typec_hole_spacing == 20);
assert(typec_thread_diameter == 10.5 && typec_thread_length == 14.5);
assert(typec_head_diameter == 13.8 && typec_head_thickness == 2);
assert(typec_nut_outer_size == 14 && typec_nut_thickness == 3);
// The old three-cylinder intersection retained these protruding corner volumes.
intersection() {
    bottom_shell();
    for (x = [4.8, case_width - 4.8])
        for (y = [4.8, case_depth - 4.8])
            translate([x - 0.1, y - 0.1, 4.7]) cube([0.2, 0.2, 0.2]);
}
// Sample the concentric R12/R9.6 spherical wall in four lower octants.
difference() {
    union()
        for (sx = [-1, 1])
            for (sy = [-1, 1])
                for (v = [[1, 1, 1], [2, 1, 1], [1, 2, 1], [1, 1, 2]])
                    let(center = [sx < 0 ? 12 : case_width - 12,
                                  sy < 0 ? 12 : case_depth - 12, 12],
                        p = center + 10.8 * unit([sx * v[0], sy * v[1], -v[2]]))
                        translate(p - [0.05, 0.05, 0.05]) cube([0.1, 0.1, 0.1]);
    difference() { case_volume(); case_volume(wall); }
}
// Two independent rear port coordinates; the bore remains cylindrical through the wall.
intersection() {
    bottom_shell();
    for (x = [50, 70])
        translate([x, case_depth + eps, rear_height / 2])
            rotate([90, 0, 0]) cylinder(d = 11.98, h = wall + 2 * eps);
}
difference() {
    rear_typec_positions()
        translate([0, 0, eps])
            difference() {
                cylinder(d = 13.8, h = wall - 2 * eps);
                translate([0, 0, -eps]) cylinder(d = 12.1, h = wall);
            }
    bottom_shell();
}
intersection() { typec_envelopes(eps); bottom_shell(); }
intersection() { typec_envelopes(eps); lid(); }
intersection() { typec_envelopes(eps); module_envelopes(eps); }
assert(joystick_center[0] < keyboard_center[0]);
assert(tft_size == [108.04, 61.74] && tft_hole_spacing == [101.9, 54.9]);
assert(len(shell_hole_x) * (len(shell_hole_y) + 1) == 6);
assert(joint_clearance == 0.5 && lid_corner_radius == 6);
assert(support_lip_width == 1.5 && support_lip_thickness == 1.5);
assert(abs((case_width - 2 * wall) - (case_width - 2 * (wall + joint_clearance)) - 1) < 0.001);
assert(middle_mount_y == 49.2 && middle_boss_diameter == 7);
assert(middle_mount_y > joystick_center[1] + joystick_size[1] / 2);
assert(middle_mount_y + 3 < deck_depth - tangent_length(4));
// Six heads plus 0.5 mm radial margin fit inside the rounded lid outline.
linear_extrude(height = 1)
    difference() {
        for (x = shell_hole_x)
            for (y = concat(shell_hole_y, [middle_mount_y]))
                translate([x, y]) circle(d = shell_head_diameter + 1);
        lid_plan();
    }
// R6 cutaways at all four corners, not an unchanged square lid.
intersection() {
    lid_blank();
    for (x = [wall + joint_clearance, case_width - wall - joint_clearance - 1])
        for (y = [rim_radius + joint_clearance,
                  case_depth - rim_radius - joint_clearance - 1])
            translate([x, y, 0]) cube([1, 1, rear_height + 1]);
}
// The complete perimeter clearance remains empty through the lid's thickness.
intersection() {
    bottom_shell();
    intersection() {
        extrude_along_x(case_width) panel_band(eps, panel_thickness - eps);
        plan_volume()
            difference() {
                opening_plan(eps);
                offset(delta = eps) lid_plan();
            }
    }
}
// Continuous bearing ring: 1.5 mm inward, top exactly 3 mm below the outer surface.
difference() {
    intersection() {
        extrude_along_x(case_width)
            panel_band(panel_thickness + eps, panel_thickness + support_lip_thickness - eps);
        plan_volume()
            difference() {
                opening_plan(eps);
                opening_plan(support_lip_width - eps);
            }
    }
    bottom_shell();
}
// Middle sockets and solid material around their openings must be present.
intersection() {
    bottom_shell();
    middle_fasteners()
        translate([0, 0, -insert_depth + eps])
            cylinder(d = insert_diameter - 0.1, h = insert_depth);
}
difference() {
    middle_fasteners() translate([3, 0, -1]) cube([0.3, 0.3, 0.3]);
    bottom_shell();
}
assert(lid_screw_diameter == 3);
assert(lid_counterbore_diameter == 4.5 && lid_counterbore_depth == 1.4);
assert(abs(panel_thickness - lid_counterbore_depth - 1.6) < 0.001);
assert(lid_min_edge == 2);
// Every actual opening, including the larger counterbores, stays inside a 2 mm inset.
difference() {
    intersection() { lid_openings(); lid_blank(); }
    plan_volume() offset(delta = -lid_min_edge) lid_plan();
}
// All six holes have the requested 1.4 mm recess and retain the lower 1.6 mm shoulder.
intersection() {
    lid();
    shell_fasteners()
        translate([0, 0, 1.6 + eps]) cylinder(d = 4.48, h = 1.4);
}
difference() {
    shell_fasteners()
        translate([0, 0, eps])
            difference() {
                cylinder(d = 4.48, h = 1.6 - 2 * eps);
                translate([0, 0, -eps]) cylinder(d = 3.02, h = 1.6);
            }
    lid();
}
// Added bases are connected to the side walls down to the floor.
difference() {
    union()
        for (x = [wall - 0.8, case_width - wall - 0.8])
            translate([x, middle_mount_y - 2, bottom_radius + 1])
                cube([1.6, 4, deck_height - panel_thickness - bottom_radius - 2]);
    bottom_shell();
}
assert($fn == 32 && len(top_path()) >= 12);
assert(insert_diameter == 4.7 && insert_depth == 5.2);
assert(esp32_size == [58, 68] && esp32_hole_spacing == [49, 58]);
assert(esp32_hole_diameter == 3 && esp32_standoff_height == 5);
assert(norm(esp32_center - [37.5, 96.7]) < 0.001 && esp32_mount_z == 9);
assert(breadboard_size == [35, 47] && norm(breadboard_center - [94, 86.2]) < 0.001);
assert(breadboard_mount_z == 4 && breadboard_height == 10);
assert(floor_module_clearance == 10 && floor_board_gap == 10);
assert(esp32_rear_clearance == 20 && support_angle == 75);
assert(breadboard_center[0] > esp32_center[0]);
assert(abs(breadboard_center[0] - breadboard_size[0] / 2 -
           esp32_center[0] - esp32_size[0] / 2 - 10) < 0.001);
assert(abs(breadboard_center[1] - breadboard_size[1] / 2 - esp32_front_y) < 0.001);
assert(abs(floor_side_inset - wall - 6.1) < 0.001 && floor_side_clearance == 1);
assert(abs(esp32_front_y - middle_mount_y - middle_boss_diameter / 2 - 10) < 0.001);
assert(esp32_front_y - keyboard_center[1] - keyboard_size[1] / 2 >= 10);
assert(esp32_front_y - joystick_center[1] - joystick_size[1] / 2 >= 10);
assert(case_depth - wall - (esp32_center[1] + esp32_size[1] / 2) >= 20);
assert(case_depth - wall - (esp32_center[1] + esp32_size[1] / 2) < 21);
// Independent measured footprints, expanded by 10 mm, not just collision checks.
// Each board's own floor/posts are intentional contacts; other parts are not exempt.
for (spec = [[[8.5, 62.7, 9], [58, 68, 11.6]],
             [[76.5, 62.7, 4], [35, 47, 10]]]) {
    intersection() {
        // Side walls and their rounded corner returns use the reduced clearance.
        translate(spec[0] + [-1 + eps, -1 + eps, eps])
            cube(spec[1] + [2 - 2 * eps, 2 - 2 * eps, -2 * eps]);
        union() { bottom_shell(); lid(); }
    }
    intersection() {
        translate(spec[0] + [eps, -10 + eps, eps])
            cube([spec[1][0] - 2 * eps, 10 - 2 * eps, spec[1][2] - 2 * eps]);
        bottom_shell();
    }
    intersection() {
        // Rear-panel distance is measured on its straight span, not a side corner's return.
        let(left = max(spec[0][0], bottom_radius),
            right = min(spec[0][0] + spec[1][0], case_width - bottom_radius))
            translate([left + eps, spec[0][1] + spec[1][1] + eps, spec[0][2] + eps])
                cube([right - left - 2 * eps, 20 - 2 * eps, spec[1][2] - 2 * eps]);
        bottom_shell();
    }
    intersection() {
        translate(spec[0] - [10 - eps, 10 - eps, 10 - eps])
            cube(spec[1] + [20 - 2 * eps, 20 - 2 * eps, 20 - 2 * eps]);
        union() {
            module_envelopes(eps, false);
            audio_envelopes(eps);
            switch_envelope(eps);
            typec_envelopes(eps);
            shell_mount_columns();
            // The perimeter lip is part of the side wall, checked with its reduced gap above.
            control_holes()
                translate([0, 0, floor_thickness])
                    linear_extrude(height = $control_z - floor_thickness)
                        control_mount_footprint($control_z);
        }
    }
    // Wall-hugging screen braces use the separately documented compact side clearance.
    intersection() {
        translate(spec[0] - [2.5 - eps, 2.5 - eps, 2.5 - eps])
            cube(spec[1] + [5 - 2 * eps, 5 - 2 * eps, 5 - 2 * eps]);
        screen_tabs();
    }
}
// The shared 10 mm lane must remain free of posts, braces and modules.
intersection() {
    translate([66.5 + eps, 62.7, 4 + eps]) cube([10 - 2 * eps, 47, 20.6 - 4]);
    union() { bottom_shell(); module_envelopes(eps); }
}
// Breadboard preview must represent the complete nominal reserved volume.
difference() {
    translate([76.5 + eps, 62.7 + eps, 4 + eps])
        cube([35 - 2 * eps, 47 - 2 * eps, 10 - 2 * eps]);
    breadboard_envelope();
}
// Check the ESP32 volume against the full screen stack independently of the shell.
intersection() {
    translate([8.5, 62.7, 9 + eps])
        cube([58, 68, pcb_thickness + esp32_component_height - eps]);
    on_slope(tft_center[0] - tft_size[0] / 2,
             tft_center[1] - tft_size[1] / 2,
             screen_mount_normal - screen_back_height)
        cube([tft_size[0], tft_size[1],
              screen_back_height + screen_pcb_thickness + screen_stack_above_pcb]);
}
// Verify all four low posts, glue pockets, and solid blind-hole floors.
intersection() {
    bottom_shell();
    for (x = [13, 62])
        for (y = [67.7, 125.7])
            translate([x, y, 3.82]) cylinder(d = 4.68, h = 5.2);
}
difference() {
    union()
        for (x = [13, 62])
            for (y = [67.7, 125.7]) {
                translate([x + 3, y, 8.5]) cube([0.3, 0.3, 0.4]);
                translate([x - 1, y - 1, 0.2]) cube([2, 2, 3.5]);
            }
    bottom_shell();
}
assert(joystick_standoff_height == 16);
assert(keyboard_mount_z - screw_tip_depth >= 1.5);
assert(abs(screen_board_gap - screen_stack_above_pcb - screen_front_clearance) < 0.001);
// Actual pad faces and the free gap follow the panel normal, not the world Z axis.
expected_mount_normal = -expected_screen_stack - 6;
for (x = [9.05, 110.95])
    for (v = [45.17, 100.07]) {
        difference() {
            on_slope(x + 3, v - 0.1, expected_mount_normal - 0.2)
                cube([0.2, 0.2, 0.18]);
            bottom_shell();
        }
        intersection() {
            bottom_shell();
            on_slope(x + 3, v - 0.1, expected_mount_normal + eps)
                cube([0.2, 0.2, -3 - expected_mount_normal - 2 * eps]);
        }
        difference() {
            on_slope(x + 3, v - 0.1, -3 + eps) cube([0.2, 0.2, 0.18]);
            lid();
        }
    }
// A 2 mm PCB and the measured display stack must occupy their full normal depths.
difference() {
    on_slope(59, 71.62, expected_mount_normal + eps) cube([2, 2, 2 - 2 * eps]);
    module_boards();
}
difference() {
    on_slope(59, 71.62, expected_mount_normal + 2 + eps)
        cube([2, 2, expected_screen_stack - 2 * eps]);
    module_envelopes();
}
intersection() {
    module_envelopes();
    on_slope(59, 71.62, -4 + eps) cube([2, 2, 1 - 2 * eps]);
}
assert(keyboard_window == [57.4, 20.2]);
assert(norm(keyboard_window_center - [84, 29.6]) < 0.001);
assert(abs((keyboard_center[1] + keyboard_size[1] / 2) -
           (keyboard_window_center[1] + keyboard_window[1] / 2) - 4) < 0.001);
assert(abs((keyboard_window_center[1] - keyboard_window[1] / 2) -
           (keyboard_center[1] - keyboard_size[1] / 2) - 3) < 0.001);
assert(abs(deck_height - panel_thickness - keyboard_mount_z - 5) < 0.001);
assert(keyboard_mount_z == 20 && keyboard_standoff_height == 16);
assert(joystick_mount_z == 20);
assert(abs(deck_height - panel_thickness - joystick_mount_z - 5) < 0.001);
assert(norm(joystick_hole_center - [31, 27.5]) < 0.001);
assert(norm(joystick_window_center - [29.8, 23.9]) < 0.001);
assert(joystick_window_diameter == 29);
assert(abs(joystick_hole_center[1] - 14 -
           (joystick_center[1] - joystick_size[1] / 2) - 4.5) < 0.001);
assert(abs(joystick_center[1] + joystick_size[1] / 2 -
           (joystick_hole_center[1] + 14) - 7.5) < 0.001);
// Independent coordinates check the actual circular cut, rim, sockets and raised posts.
intersection() {
    lid();
    translate([29.8, 23.9, 24.9]) cylinder(d = 28.98, h = 3.2);
}
difference() {
    translate([29.8, 23.9, 25.1])
        difference() {
            cylinder(d = 29.4, h = 2.8);
            translate([0, 0, -eps]) cylinder(d = 29.2, h = 2.8 + 2 * eps);
        }
    lid();
}
intersection() {
    bottom_shell();
    for (x = [20.5, 41.5])
        for (y = [13.5, 41.5])
            translate([x, y, 14.82]) cylinder(d = 4.68, h = 5.2);
}
difference() {
    union()
        for (x = [20.5, 41.5])
            for (y = [13.5, 41.5])
                translate([x + 3, y, 19.5]) cube([0.3, 0.3, 0.4]);
    bottom_shell();
}
// Probe the requested opening and retained strips to catch offset/sign regressions.
intersection() {
    lid();
    translate([55.3 + eps, 19.5 + eps, 25 - eps])
        cube([57.4 - 2 * eps, 20.2 - 2 * eps, 3 + 2 * eps]);
}
difference() {
    union() {
        translate([56, 19, 25.1]) cube([56, 0.4, 2.8]);
        translate([56, 39.8, 25.1]) cube([56, 0.4, 2.8]);
    }
    lid();
}
// Each raised keyboard post must actually reach Z=20 around its nut socket.
difference() {
    hole_pattern(keyboard_center, keyboard_hole_spacing)
        translate([3, 0, 19.5]) cube([0.3, 0.3, 0.4]);
    bottom_shell();
}
// Ignore nominal mating faces with a 0.02 mm tolerance, not real penetrations.
// Each solid below is an error region; their union must be empty.
intersection() { bottom_shell(); translate([0, 0, eps]) lid(); }
intersection() { module_envelopes(eps); bottom_shell(); }
intersection() { module_envelopes(eps); lid(); }
// Both side rails must remain solid across every bend and intervening panel segment.
lid_midline = inner_path(top_path(), 1.5);
difference() {
    union()
        for (x = [13.5, 114.5]) {
            for (i = [1 : len(lid_midline) - 2])
                translate([x - 0.05, lid_midline[i][0] - 0.05, lid_midline[i][1] - 0.05])
                    cube([0.1, 0.1, 0.1]);
            for (i = [0 : len(lid_midline) - 2])
                let(p = (lid_midline[i] + lid_midline[i + 1]) / 2)
                    translate([x - 0.05, p[0] - 0.05, p[1] - 0.05]) cube([0.1, 0.1, 0.1]);
        }
    lid();
}
// Entire module boxes plus 2 mm XY clearance must avoid the lid columns.
intersection() {
    shell_mount_columns();
    union()
        for (spec = [[keyboard_center, keyboard_size, keyboard_mount_z],
                     [joystick_center, concat(joystick_size, [joystick_total_height]),
                      joystick_mount_z]])
            translate([spec[0][0] - spec[1][0] / 2 - control_clearance + eps,
                       spec[0][1] - spec[1][1] / 2 - control_clearance + eps, spec[2]])
                cube([spec[1][0] + 2 * (control_clearance - eps),
                      spec[1][1] + 2 * (control_clearance - eps), spec[1][2]]);
}
difference() { bottom_shell(); case_volume(-eps); }
// The open roof must not retain even a thin solid membrane between the rails.
intersection() {
    bottom_shell();
    intersection() {
        translate([15, 0, 0])
            extrude_along_x(case_width - 30) panel_band(0, panel_thickness + 0.2);
        plan_volume() opening_plan(support_lip_width + eps);
    }
}
assert(rear_mount_depth == 10);
expected_rear_rise = 1.5 * 11.1 * tan(75);
for (x = shell_hole_x) {
    assert(abs(column_root_z(x, case_depth - 11) -
               (rear_height - 13 - expected_rear_rise)) < 0.001);
    assert(abs(rear_brace_rise(x, case_depth - 11) - expected_rear_rise) < 0.001);
    // Every sampled segment meets the requested 75-degree minimum from horizontal.
    for (i = [1 : max(8, $fn)])
        assert(11.1 * (brace_ease(i / max(8, $fn)) -
                       brace_ease((i - 1) / max(8, $fn))) * tan(75) <=
               expected_rear_rise / max(8, $fn) + 0.001);
    // Both mirrored braces must follow the curved profile, not a straight wedge or a box.
    for (t = [0.25, 0.5, 0.75])
        let(reach = wall + 11.1 * t * t * (3 - 2 * t),
            edge = x < case_width / 2 ? reach : case_width - reach,
            direction = x < case_width / 2 ? 1 : -1,
            z = rear_height - 13 - expected_rear_rise + expected_rear_rise * t) {
            difference() {
                translate([edge - direction * 0.25 - 0.05, case_depth - 12, z - 0.05])
                    cube([0.1, 1, 0.1]);
                shell_mount_columns();
            }
            intersection() {
                translate([edge + direction * 0.25 - 0.05, case_depth - 12, z - 0.05])
                    cube([0.1, 1, 0.1]);
                shell_mount_columns();
            }
        }
    // Neither rear seat may retain a long column below its short wedge.
    intersection() {
        shell_mount_columns();
        translate([x - 4.5, case_depth - 14.5, floor_thickness])
            cube([9, 14.5, rear_height - 13 - expected_rear_rise - floor_thickness - 2 * eps]);
    }
    // Material beneath the 8.8 mm blind screw pocket, in the full 10 mm seat.
    difference() {
        translate([x - 1, case_depth - 12, rear_height - panel_thickness - 9.8])
            cube([2, 2, 0.8]);
        bottom_shell();
    }
}
// Positive-width roots connect each column to the side wall above its designed root.
difference() {
    union()
        for (x = shell_hole_x)
            for (y = shell_hole_y)
                translate([x < case_width / 2 ? wall - 0.8 : case_width - wall - 0.8,
                           y - 2, max(column_root_z(x, y) +
                                      (y > deck_depth ? expected_rear_rise * 0.2 : 3), bottom_radius + 1)])
                    cube([1.6, 4, (y < deck_depth ? deck_height : rear_height) -
                                   panel_thickness - max(column_root_z(x, y) +
                                       (y > deck_depth ? expected_rear_rise * 0.2 : 3), bottom_radius + 1) - 1]);
    bottom_shell();
}
// Former narrow channels between the columns/posts and the walls must be solid.
difference() {
    union() {
        for (x = shell_hole_x)
            for (y = shell_hole_y)
                let(start = column_root_z(x, y) +
                            (column_root_z(x, y) > floor_thickness ?
                             rear_brace_rise(x, y) : 0) + 2)
                translate([x - 0.5, y < deck_depth ? wall + 0.5 : case_depth - wall - 2.5,
                           start])
                    cube([1, 2, (y < deck_depth ? deck_height : rear_height) -
                                 panel_thickness - start - 1]);
        for (dy = [-keyboard_hole_spacing[1] / 2, keyboard_hole_spacing[1] / 2])
            let(edge = keyboard_center[0] + keyboard_hole_spacing[0] / 2 +
                       module_boss_diameter / 2)
                if (case_width - wall - edge <= small_gap_fill)
                intersection() {
                    // The bridge ends at the spherical exterior, not beyond its lower fillet.
                    case_volume(eps);
                    translate([edge + eps, keyboard_center[1] + dy - 0.5, floor_thickness + 1])
                        cube([case_width - wall - edge - 2 * eps, 1, keyboard_standoff_height - 2]);
                }
    }
    bottom_shell();
}
// All four screen braces must extend to the root required by a 75-degree slope.
for (side = [-1, 1])
    for (dv = [-27.45, 27.45])
        let(along = 72.62 + dv,
            root = 28 + sin(60) * along + cos(60) * (screen_mount_normal - 10) -
                   4 * sin(60) - 10.65 * tan(75) - 2,
            front = 58 + cos(60) * along - sin(60) * screen_mount_normal - 2,
            x = side < 0 ? 2 : case_width - 2.2) {
            assert(root >= floor_thickness);
            difference() {
                translate([x, front + 1, root + 0.4]) cube([0.2, 0.2, 0.2]);
                screen_tabs();
            }
            intersection() {
                translate([x, front + 1, root - 0.4]) cube([0.2, 0.2, 0.2]);
                screen_tabs();
            }
        }
// Probe 5-6 mm vertically below each hole center, not along the tilted hole axis.
// A rearward-only gusset leaves these volumes unsupported.
difference() {
    union()
        for (dx = [-tft_hole_spacing[0] / 2, tft_hole_spacing[0] / 2])
            for (dv = [-tft_hole_spacing[1] / 2, tft_hole_spacing[1] / 2])
                translate([tft_center[0] + dx - 0.5,
                           deck_depth + cos(slope_angle) * (tft_center[1] + dv) -
                           sin(slope_angle) * screen_mount_normal - 0.5,
                           deck_height + sin(slope_angle) * (tft_center[1] + dv) +
                           cos(slope_angle) * screen_mount_normal - 6])
                    cube([1, 1, 1]);
    bottom_shell();
}
intersection() {
    lid();
    shell_fasteners()
        translate([0, 0, -eps])
            cylinder(d = lid_screw_diameter - 0.1, h = panel_thickness + 2 * eps);
}
intersection() {
    bottom_shell();
    union() {
        control_holes()
            translate([0, 0, $control_z - insert_depth + eps])
                cylinder(d = insert_diameter - 0.1, h = insert_depth);
        for (dx = [-tft_hole_spacing[0] / 2, tft_hole_spacing[0] / 2])
            for (dv = [-tft_hole_spacing[1] / 2, tft_hole_spacing[1] / 2])
                on_slope(tft_center[0] + dx, tft_center[1] + dv,
                         screen_mount_normal - insert_depth + eps)
                    cylinder(d = insert_diameter - 0.1, h = insert_depth);
        shell_fasteners()
            translate([0, 0, -insert_depth + eps])
                cylinder(d = insert_diameter - 0.1, h = insert_depth);
    }
}
""",
        encoding="utf-8",
    )
    render(args.openscad, checks, args.output / "interference.stl",
           ['part="none"', "expected_screen_stack=3"], empty=True)
    render(
        args.openscad,
        checks,
        args.output / "thicker-screen-interference.stl",
        ['part="none"', "screen_stack_above_pcb=12", "expected_screen_stack=12"],
        empty=True,
    )
    print("PASS: raised screen and 21 mm lower-right switch opening, "
          "spherical R12 base corners with R9.6 inner offset, two 12 mm rear Type-C ports, "
          "R6 lid corners, 0.5 mm perimeter gap, continuous 1.5 mm bearing rim, "
          "2 mm opening edge margins, 4.5/3 mm stepped counterbores, "
          "open roof, filled boss-wall gaps, connected column roots, sketch layout, 6 lid screw paths, "
          "16 module sockets, rotated ESP32 beside a 35x47 mm breadboard, "
          "10 mm board gap, compact side gaps, 10 mm front gap and 20-21 mm rear gap, "
          "75-degree braces, compact 2.5 mm side-brace clearance, R6 screen aperture, "
          "8 mm screen-PCB-to-speaker gap, 2.26 mm left-shifted window with 1 mm edge margins, "
          "2 mm screen PCB plus 3 mm display and 6 mm normal mounting gap, "
          "support directly below all 4 screen holes, "
          "no component-envelope or column interference at 3/12 mm display thickness "
          "(0.02 mm contact tolerance).")


if __name__ == "__main__":
    main()
