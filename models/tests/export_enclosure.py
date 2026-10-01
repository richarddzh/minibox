"""Export and validate the four independent enclosure parts."""

import argparse
from pathlib import Path
import shutil
import struct
from tempfile import TemporaryDirectory

from check_enclosure import (
    SOURCE,
    check_3mf_export,
    mesh_info,
    mesh_measurements,
    read_triangles,
    render,
    write_3mf,
)


PARTS = {
    "bottom": -4,
    "lid": -18,
    "rear-lid": -6,
    "screen-bracket": 2,
}
PRINT_PARTS = {
    "bottom": "bottom",
    "lid": "lid-print",
    "rear-lid": "rear-lid-print",
    "screen-bracket": "screen-bracket-print",
}


def validate(path, euler, curved=False):
    if not curved:
        return mesh_info(path, euler)
    triangles = read_triangles(path)
    result = mesh_measurements(triangles, euler, path.name)
    for digits in (6, 4, 3):
        rounded = [[tuple(round(value, digits) for value in point) for point in face]
                   for face in triangles]
        mesh_measurements(rounded, euler, f"{path.name} at {digits} decimals")
    print(f"{path.name}: closed, connected, and manifold at 6/4/3 decimals")
    return result


def y_up(source, destination):
    triangles = read_triangles(source)
    depth = max(point[1] for face in triangles for point in face)
    with destination.open("wb") as output:
        output.write(b"Minibox Y-up" + bytes(80 - len(b"Minibox Y-up")))
        output.write(struct.pack("<I", len(triangles)))
        for face in triangles:
            transformed = [(x, z, depth - y) for x, y, z in face]
            a, b, c = transformed
            u = [b[i] - a[i] for i in range(3)]
            v = [c[i] - a[i] for i in range(3)]
            normal = (u[1] * v[2] - u[2] * v[1],
                      u[2] * v[0] - u[0] * v[2],
                      u[0] * v[1] - u[1] * v[0])
            output.write(struct.pack("<12fH", *normal, *a, *b, *c, 0))


def check_assembly_fit(executable):
    probes = {
        "bracket-tracks": "intersection() { screen_bracket_tracks(); translate([0, 0, 0.05]) screen_bracket(); }",
        "bracket-shell": "intersection() { bottom_shell(); translate([0, 0, 0.05]) screen_bracket(); }",
        "bracket-insertion": "intersection() { bottom_shell(); translate([0, 0, 15]) screen_bracket(); }",
        "cover-seam": "intersection() { lid(); rear_lid(); }",
        "header-cable": """
            intersection() {
                union() { screen_bracket(); screen_bracket_tracks(); }
                translate([103, screen_bracket_front_y, 95]) cube([8, 8, 15]);
            }
        """,
    }
    with TemporaryDirectory(prefix="minibox-fit-") as directory:
        folder = Path(directory)
        for name, body in probes.items():
            fixture = folder / f"{name}.scad"
            fixture.write_text(f"include <{SOURCE}>\n{body}\n", encoding="utf-8")
            render(executable, fixture, folder / f"{name}.stl",
                   ['part="none"'], empty=True)
            print(f"{name}: no solid interference")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--openscad", default=shutil.which("openscad"))
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--3mf-only", action="store_true", dest="three_mf_only")
    args = parser.parse_args()
    if not args.openscad:
        parser.error("Pass --openscad with the path to openscad.com.")
    args.output.mkdir(parents=True, exist_ok=True)
    for part, euler in PARTS.items():
        assembly = args.output / f"{part}-assembly.stl"
        render(args.openscad, SOURCE, assembly, [f'part="{part}"'])
        reference = validate(assembly, euler, part == "rear-lid")
        destination = args.output / f"{part}.3mf"
        write_3mf(assembly, destination)
        if part == "rear-lid":
            bounds, volume = validate(destination, euler, curved=True)
            assert abs(volume - reference[1]) < 1
            assert all(abs(a - b) < 0.001 for old, new in zip(bounds, reference[0])
                       for a, b in zip(old, new))
            assert read_triangles(destination) == read_triangles(assembly)
        else:
            check_3mf_export(destination, assembly, reference, euler)
        if not args.three_mf_only:
            printable = args.output / f"{part}.stl"
            if PRINT_PARTS[part] != part:
                render(args.openscad, SOURCE, printable,
                       [f'part="{PRINT_PARTS[part]}"'])
            else:
                printable.write_bytes(assembly.read_bytes())
            printable_bounds, printable_volume = validate(
                printable, euler, part == "rear-lid"
            )
            assert abs(printable_volume - reference[1]) < 1, (
                f"Print transform changed {part} volume."
            )
            assert abs(printable_bounds[2][0]) < 0.001, (
                f"{part} does not rest on the Z=0 print bed."
            )
            rotated = args.output / f"{part}-y-up.stl"
            y_up(printable, rotated)
            validate(rotated, euler, part == "rear-lid")
        assembly.unlink()
    check_assembly_fit(args.openscad)
    print("PASS: four independent, manifold parts exported.")


if __name__ == "__main__":
    main()
