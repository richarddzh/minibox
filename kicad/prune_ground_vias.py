"""Remove redundant GND vias while retaining return paths and filled-copper connectivity."""

import hashlib
import json
from math import hypot
from pathlib import Path
import re
import tempfile

import pcbnew as pcb
from routing_checks import verify_routing


HERE = Path(__file__).resolve().parent
FILE = HERE / "minibox-carrier.kicad_pcb"


def distance(a, b):
    return hypot(pcb.ToMM(a.x-b.x), pcb.ToMM(a.y-b.y))


def prune_ground_vias():
    original = FILE.read_bytes()
    board = pcb.LoadBoard(str(FILE))
    verify_routing(board)
    board.BuildConnectivity()
    if board.GetConnectivity().GetUnconnectedCount(False):
        raise RuntimeError("Only a connected baseline can be pruned")
    vias = [item for item in board.GetTracks() if isinstance(item, pcb.PCB_VIA)]
    signal_positions = [item.GetPosition() for item in vias if item.GetNetname() != "GND"]
    grounds = {item.m_Uuid.AsString(): item.GetPosition()
               for item in vias if item.GetNetname() == "GND"}
    remaining = dict(grounds)
    source = original.decode("utf-8")
    removed, retained_for_copper = [], []
    order = sorted(grounds, key=lambda uid: (
        sum(distance(grounds[uid], s) <= 3 for s in signal_positions),
        grounds[uid].x, grounds[uid].y))
    with tempfile.TemporaryDirectory(prefix="minibox-gnd-") as directory:
        candidate_file = Path(directory) / FILE.name
        for uid in order:
            other = {key: value for key, value in remaining.items() if key != uid}
            if not other or any(
                    min(distance(s, p) for p in other.values()) > 3 for s in signal_positions):
                continue
            blocks = [match for match in re.finditer(r"(?ms)^\t\(via\b.*?^\t\)", source)
                      if f'(uuid "{uid}")' in match.group()]
            if len(blocks) != 1 or '(net "GND")' not in blocks[0].group():
                raise RuntimeError(f"Cannot safely identify GND via block {uid}")
            block = blocks[0]
            candidate_file.write_text(source[:block.start()] + source[block.end():],
                                      encoding="utf-8")
            candidate = pcb.LoadBoard(str(candidate_file))
            candidate.BuildConnectivity()
            if not pcb.ZONE_FILLER(candidate).Fill(candidate.Zones()):
                raise RuntimeError("Ground pruning candidate fill failed")
            candidate.BuildConnectivity()
            if not pcb.ZONE_FILLER(candidate).Fill(candidate.Zones()):
                raise RuntimeError("Ground pruning connectivity refill failed")
            candidate.BuildConnectivity()
            if candidate.GetConnectivity().GetUnconnectedCount(False):
                retained_for_copper.append(uid)
                continue
            try:
                verify_routing(candidate)
            except RuntimeError as error:
                # A removed via may disconnect reference copper; keep that via and record why.
                retained_for_copper.append({"uuid": uid, "reason": str(error)})
                continue
            pcb.SaveBoard(str(candidate_file), candidate)
            source = candidate_file.read_text(encoding="utf-8")
            remaining = other
            removed.append([pcb.ToMM(grounds[uid].x), pcb.ToMM(grounds[uid].y)])
    if FILE.read_bytes() != original:
        raise RuntimeError("PCB changed concurrently; refusing to overwrite")
    FILE.write_text(source, encoding="utf-8")
    report = {
        "policy": "No periodic same-layer stitching; retain <=3mm via return paths, connected nets and filled I2S references",
        "baseline_board_sha256": hashlib.sha256(original).hexdigest(),
        "board_sha256": hashlib.sha256(FILE.read_bytes()).hexdigest(),
        "ground_vias_before": len(grounds), "ground_vias_after": len(remaining),
        "removed_ground_vias": len(removed), "removed_positions_kicad_mm": removed,
        "maximum_signal_via_return_distance_mm": max((
            min(distance(s, p) for p in remaining.values()) for s in signal_positions), default=0),
        "retained_for_copper_connectivity": retained_for_copper,
        "needs_final_drc": True,
    }
    (HERE / "assembly" / "ground-stitching-review.json").write_text(
        json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"GND vias: {len(grounds)} -> {len(remaining)}; removed {len(removed)}; final DRC/export required")


if __name__ == "__main__":
    prune_ground_vias()
