"""Validate saved carrier bends, signal layers and filled I2S reference copper."""

from collections import defaultdict
from math import ceil, hypot

import pcbnew as pcb


def verify_routing(board):
    pads = [pad for footprint in board.GetFootprints() for pad in footprint.Pads()]
    tracks = list(board.GetTracks())
    planes = {}
    for layer in (pcb.In1_Cu, pcb.In2_Cu):
        zones = [z for z in board.Zones() if z.GetLayer() == layer and z.GetNetname() == "GND"]
        if len(zones) != 1:
            raise RuntimeError("Expected one GND zone on each reference layer")
        planes[layer] = zones[0].GetFilledPolysList(layer)
    if planes[pcb.In1_Cu].OutlineCount() != 1:
        raise RuntimeError("The inner GND plane must remain a single connected region")
    i2s = {"I2S_WS", "I2S_BCLK", "AUDIO_DIN", "MIC_SD"}
    endpoints = {name: [pad.GetBoundingBox() for pad in pads if pad.GetNetname() == name]
                 for name in i2s}
    for item in tracks:
        if isinstance(item, pcb.PCB_VIA) and item.GetNetname() in i2s:
            endpoints[item.GetNetname()].append(item.GetBoundingBox())
    junctions = defaultdict(list)
    samples_checked = 0
    for item in tracks:
        if isinstance(item, pcb.PCB_VIA):
            continue
        name, layer = item.GetNetname(), item.GetLayer()
        if name in {"3V3", "5V_IN", "5V_SW"}:
            if layer not in (pcb.F_Cu, pcb.In2_Cu):
                raise RuntimeError("Power routing must stay on F.Cu/In2.Cu")
        elif layer not in (pcb.F_Cu, pcb.B_Cu):
            raise RuntimeError("Signal routing must stay on the outer layers")
        start, end = item.GetStart(), item.GetEnd()
        for point in (start, end):
            junctions[(layer, item.GetNetCode(), point.x, point.y)].append(item)
        if name not in i2s:
            continue
        plane = planes[pcb.In1_Cu if layer == pcb.F_Cu else pcb.In2_Cu]
        samples = max(1, ceil(pcb.ToMM(item.GetLength()) / 0.1))
        for index in range(samples):
            ratio = (index + 0.5) / samples
            point = pcb.VECTOR2I(round(start.x + ratio * (end.x - start.x)),
                                 round(start.y + ratio * (end.y - start.y)))
            samples_checked += 1
            if plane.Contains(point):
                continue
            margin = pcb.FromMM(0.4)
            if any(box.GetLeft()-margin <= point.x <= box.GetRight()+margin and
                   box.GetTop()-margin <= point.y <= box.GetBottom()+margin
                   for box in endpoints[name]):
                continue
            raise RuntimeError(f"{name} loses its GND reference at "
                               f"({pcb.ToMM(point.x):.3f}, {pcb.ToMM(point.y):.3f})")
    for (_, _, x, y), pair in junctions.items():
        if len(pair) != 2:
            continue
        vectors = []
        for item in pair:
            other = item.GetEnd() if item.GetStart().x == x and item.GetStart().y == y else item.GetStart()
            vectors.append((other.x-x, other.y-y))
        inside_pad = any(
            pad.GetNetCode() == pair[0].GetNetCode() and
            hypot(pcb.ToMM(pad.GetPosition().x-x), pcb.ToMM(pad.GetPosition().y-y)) +
            pcb.ToMM(pair[0].GetWidth())/2 <=
            min(pcb.ToMM(pad.GetSize().x), pcb.ToMM(pad.GetSize().y))/2 for pad in pads)
        dot = vectors[0][0]*vectors[1][0] + vectors[0][1]*vectors[1][1]
        if not inside_pad and all(vector != (0, 0) for vector in vectors) and dot >= 0:
            raise RuntimeError(f"Unresolved right/acute bend: {pair[0].GetNetname()} "
                               f"at ({pcb.ToMM(x)}, {pcb.ToMM(y)})")
    return {"in1_connected_regions": 1, "i2s_reference_samples": samples_checked,
            "two_segment_bends_checked": sum(len(pair) == 2 for pair in junctions.values())}
