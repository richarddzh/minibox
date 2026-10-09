"""Package current review outputs only after their PCB/CAM/CPL hashes agree."""

import hashlib
import json
from pathlib import Path
import zipfile


HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
ASSEMBLY = HERE / "assembly"
FABRICATION = HERE / "fabrication"


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def package_review():
    manifest_path = ASSEMBLY / "manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    board = HERE / "minibox-carrier.kicad_pcb"
    cam = FABRICATION / "minibox-gerber-review.zip"
    if manifest["board_sha256"] != digest(board):
        raise RuntimeError("PCB changed: run export_revision.py before packaging")
    if manifest["drc_violations"] or manifest["unconnected_items"]:
        raise RuntimeError("Cannot package a board with DRC or connectivity failures")
    if manifest["gerber_sha256"] != digest(cam):
        raise RuntimeError("CAM differs from the verified export")
    placement = json.loads((ASSEMBLY / "jlc-placement-review.json").read_text(encoding="utf-8"))
    if (placement["board_sha256"] != manifest["board_sha256"] or
            placement["cpl_sha256"] != digest(ASSEMBLY / "positions-jlc-review.csv")):
        raise RuntimeError("JLC placements differ from the saved PCB or verified CPL")
    with zipfile.ZipFile(cam) as archive:
        if archive.testzip() is not None:
            raise RuntimeError("Corrupt CAM archive")
        for name in archive.namelist():
            if Path(name).name != name or archive.read(name) != (FABRICATION / name).read_bytes():
                raise RuntimeError(f"CAM archive entry differs from current file: {name}")
    paths = set()
    for directory in (ASSEMBLY, FABRICATION, HERE / "renders", HERE / "Minibox.pretty"):
        paths.update(path for path in directory.iterdir()
                     if path.is_file() and path != manifest_path and path.suffix != ".zip")
    paths.update([
        board, HERE / "minibox-carrier.kicad_pro", HERE / "fp-lib-table",
        HERE / "README.md", HERE / "carrier-revision.html", HERE / "layout-revision-plan.md",
        HERE / "generate_board.py", HERE / "export_revision.py",
        HERE / "export_jlc_positions.py", HERE / "package_review.py", cam,
        ROOT / "docs" / "hardware-connections.md",
        ROOT / "docs" / "jlc-pcb-design-spec.md",
        ROOT / "hardware_references" / "carrier-revision-references.md",
        ROOT / ".github" / "instructions.md",
        ROOT / ".github" / "skills" / "design-route-minibox-pcb" / "SKILL.md",
        ROOT / ".github" / "skills" / "order-jlc-minibox" / "SKILL.md",
    ])
    datasheets = ROOT / "hardware_references" / "datasheets"
    paths.update(datasheets / name for name in (
        "cpg151101d13.pdf", "yv13s-l7.85-b10ka-60-0-dl01.pdf",
        "wj500v-5.08-series-candidate.pdf",
        *[f"lail-pm2.54-{count}p-l.pdf" for count in (3, 6, 7, 14, 22)]))
    missing = [str(path) for path in paths if not path.is_file()]
    if missing:
        raise RuntimeError(f"Missing review files: {missing}")
    manifest.pop("revision", None)
    manifest["version_control"] = "Git history; current outputs use fixed paths"
    manifest["jlc_placement"] = {
        "file": "positions-jlc-review.csv", "report": "jlc-placement-review.json",
        "physical_components": placement["physical_components"],
        "cpl_sha256": placement["cpl_sha256"], "status": placement["status"],
    }
    manifest["artifact_sha256"] = {
        path.relative_to(ROOT).as_posix(): digest(path) for path in sorted(paths)}
    manifest_path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n",
                             encoding="utf-8")
    paths.add(manifest_path)
    destination = ASSEMBLY / "minibox-assembly-review.zip"
    temporary = destination.with_suffix(".tmp")
    try:
        with zipfile.ZipFile(temporary, "w", zipfile.ZIP_DEFLATED) as archive:
            for path in sorted(paths):
                archive.write(path, path.relative_to(ROOT).as_posix())
        with zipfile.ZipFile(temporary) as archive:
            if archive.testzip() is not None:
                raise RuntimeError("Corrupt review archive")
            for path in paths:
                if archive.read(path.relative_to(ROOT).as_posix()) != path.read_bytes():
                    raise RuntimeError(f"Review archive entry differs: {path}")
        temporary.replace(destination)
    finally:
        if temporary.exists():
            temporary.unlink()
    print(f"Verified {len(paths)} current review files")


if __name__ == "__main__":
    package_review()
