"""Route the carrier's assigned nets with grid clearance.

This is a deterministic layout aid, not a substitute for KiCad DRC or
measuring the actual mating modules.
"""

import json
from collections import defaultdict
from heapq import heappop, heappush
from math import ceil, hypot
from pathlib import Path

import pcbnew as pcb


FILE = Path(__file__).with_name("minibox-carrier.kicad_pcb")
board = pcb.LoadBoard(str(FILE))
STEP = 0.5
X0, Y0 = 5.0, 5.0
NX, NY = 233, 217
WIDTH = 0.35
CLEARANCE = 0.3
VIA_DIAMETER = 0.8
VIA_DRILL = 0.4
ROUTE_LAYERS = (pcb.F_Cu, pcb.B_Cu)
POWER_WIDTHS = {"3V3": 0.65, "5V_IN": 0.8, "5V_SW": 0.8}


def width_for(name):
    return POWER_WIDTHS.get(name, WIDTH)


def mm(value):
    return pcb.ToMM(value)


def point(x, y):
    return pcb.VECTOR2I(pcb.FromMM(x), pcb.FromMM(y))


def cell(position):
    return (round((mm(position.x) - X0) / STEP),
            round((mm(position.y) - Y0) / STEP))


def xy(x, y):
    return X0 + x * STEP, Y0 + y * STEP


def neighbors(x, y):
    for dx, dy in ((0, 1), (1, 0), (0, -1), (-1, 0)):
        a, b = x + dx, y + dy
        if 0 <= a < NX and 0 <= b < NY:
            yield a, b


def disk(x, y, radius):
    reach = ceil(radius / STEP)
    for a in range(max(0, x - reach), min(NX, x + reach + 1)):
        for b in range(max(0, y - reach), min(NY, y + reach + 1)):
            if hypot(a - x, b - y) * STEP <= radius:
                yield a, b


pads = defaultdict(list)
all_pads = []
for footprint in board.GetFootprints():
    for pad in footprint.Pads():
        if pad.GetNetname():
            pads[pad.GetNetname()].append(pad)
        all_pads.append(pad)

blocked = [defaultdict(set) for _ in ROUTE_LAYERS]
via_pad_blocks = defaultdict(set)
for pad in all_pads:
    px, py = cell(pad.GetPosition())
    pad_clearance = max(CLEARANCE, mm(pad.GetLocalClearance() or 0))
    radius = max(mm(pad.GetSize().x), mm(pad.GetSize().y)) / 2
    if pad.GetParentFootprint().GetReference().startswith("H"):
        radius = 3.5
    radius += WIDTH / 2 + pad_clearance + STEP / 2
    for layer in range(len(ROUTE_LAYERS)):
        if not pad.IsOnLayer(ROUTE_LAYERS[layer]):
            continue
        for coordinate in disk(px, py, radius):
            blocked[layer][coordinate].add(pad.GetNetname() or "UNASSIGNED")
    via_radius = 3.5 if pad.GetParentFootprint().GetReference().startswith("H") else (
        max(mm(pad.GetSize().x), mm(pad.GetSize().y)) / 2)
    for coordinate in disk(px, py, via_radius +
                           VIA_DIAMETER / 2 + pad_clearance + STEP / 2):
        via_pad_blocks[coordinate].add("PAD")

# Match the board's two-sided antenna rule area, including trace radius.
for x in range(NX):
    for y in range(NY):
        px, py = xy(x, y)
        if 52 - WIDTH / 2 - CLEARANCE <= px <= 73.5 + WIDTH / 2 + CLEARANCE and \
                67 - WIDTH / 2 - CLEARANCE <= py <= 84 + WIDTH / 2 + CLEARANCE:
            for layer in blocked:
                layer[(x, y)].add("ANTENNA")
        if 52 - VIA_DIAMETER / 2 - CLEARANCE <= px <= 73.5 + VIA_DIAMETER / 2 + CLEARANCE and \
                67 - VIA_DIAMETER / 2 - CLEARANCE <= py <= 84 + VIA_DIAMETER / 2 + CLEARANCE:
            via_pad_blocks[(x, y)].add("ANTENNA")


def free(layer, x, y, name):
    return not (blocked[layer][(x, y)] - {name})


def via_free(x, y, name):
    if via_pad_blocks[(x, y)] - {name}:
        return False
    for layer in range(len(ROUTE_LAYERS)):
        for a, b in disk(x, y, VIA_DIAMETER / 2 + WIDTH / 2 + CLEARANCE):
            if not free(layer, a, b, name):
                return False
    return True


def search(start, targets, name, start_layers=range(len(ROUTE_LAYERS))):
    sx, sy = start
    goals = set(targets)
    positions = {(x, y) for _, x, y in goals}
    bounds = (min(x for x, _ in positions), max(x for x, _ in positions),
              min(y for _, y in positions), max(y for _, y in positions))

    def heuristic(x, y):
        left, right, top, bottom = bounds
        return max(left - x, 0, x - right) + max(top - y, 0, y - bottom)

    queue = []
    previous = {}
    costs = {}
    for layer in start_layers:
        state = (layer, sx, sy)
        costs[state] = 0
        heappush(queue, (heuristic(sx, sy), 0, state))
    while queue:
        _, cost, state = heappop(queue)
        if cost != costs.get(state):
            continue
        layer, x, y = state
        if state in goals:
            route = [state]
            while state in previous:
                state = previous[state]
                route.append(state)
            return list(reversed(route))
        for nx, ny in neighbors(x, y):
            if not free(layer, nx, ny, name):
                continue
            # The extra turn cost makes routes less jagged.
            parent = previous.get(state)
            turn = 1 if parent and (parent[1] - x, parent[2] - y) != (x - nx, y - ny) else 0
            nxt = (layer, nx, ny)
            new_cost = cost + 10 + turn
            if new_cost < costs.get(nxt, 10**12):
                costs[nxt] = new_cost
                previous[nxt] = state
                heappush(queue, (new_cost + 10 * heuristic(nx, ny), new_cost, nxt))
        if via_free(x, y, name):
            for next_layer in range(len(ROUTE_LAYERS)):
                if next_layer == layer:
                    continue
                other = (next_layer, x, y)
                if cost + 50 < costs.get(other, 10**12):
                    costs[other] = cost + 50
                    previous[other] = state
                    heappush(queue, (cost + 50 + 10 * heuristic(x, y), cost + 50, other))
    raise RuntimeError(
        f"No path for {name} starting at {start}; explored {len(costs)} cells; "
        f"goals {len(goals)}")


def track(start, end, layer, name):
    if start == end:
        return
    item = pcb.PCB_TRACK(board)
    item.SetStart(point(*start))
    item.SetEnd(point(*end))
    item.SetWidth(pcb.FromMM(width_for(name)))
    item.SetLayer(ROUTE_LAYERS[layer])
    item.SetNet(board.FindNet(name))
    board.Add(item)


def via(x, y, name):
    item = pcb.PCB_VIA(board)
    item.SetPosition(point(*xy(x, y)))
    item.SetWidth(pcb.FromMM(VIA_DIAMETER))
    item.SetDrill(pcb.FromMM(VIA_DRILL))
    item.SetLayerPair(pcb.F_Cu, pcb.B_Cu)
    item.SetNet(board.FindNet(name))
    board.Add(item)


def mark(route, name):
    for layer, x, y in route:
        radius = max(WIDTH + CLEARANCE + STEP / 2,
                     (width_for(name) + WIDTH) / 2 + CLEARANCE + STEP / 2)
        for a, b in disk(x, y, radius):
            blocked[layer][(a, b)].add(name)
    for before, after in zip(route, route[1:]):
        if before[0] != after[0]:
            x, y = before[1:]
            for layer in range(len(ROUTE_LAYERS)):
                for a, b in disk(x, y, VIA_DIAMETER / 2 + WIDTH / 2 +
                                  CLEARANCE + STEP / 2):
                    blocked[layer][(a, b)].add(name)


def apply(route, source, name, from_position=None):
    start = xy(*route[0][1:])
    pad = from_position or source.GetPosition()
    track((mm(pad.x), mm(pad.y)), start, route[0][0], name)
    run_start = route[0]
    direction = None
    for before, after in zip(route, route[1:]):
        if before[0] != after[0]:
            track(xy(*run_start[1:]), xy(*before[1:]), before[0], name)
            via(before[1], before[2], name)
            run_start = after
            direction = None
            continue
        step = (after[1] - before[1], after[2] - before[2])
        if direction is not None and step != direction:
            track(xy(*run_start[1:]), xy(*before[1:]), before[0], name)
            run_start = before
        direction = step
    track(xy(*run_start[1:]), xy(*route[-1][1:]), route[-1][0], name)
    mark(route, name)
    return route[-1][1:]


priority = [
    "GND",
    "LCD_BL", "LCD_CS", "LCD_RST", "LCD_DC", "LCD_MOSI", "LCD_SCK",
    "GPIO1", "GPIO2", "GPIO42",
    "RTC_SDA", "RTC_SCL",
    "I2S_WS", "I2S_BCLK", "AUDIO_DIN", "MIC_SD",
    "BUTTON1", "RECORD", "BUTTON3",
    "AMP_SD", "AMP_GAIN",
    "5V_IN", "5V_SW",
    "3V3",
]
assert set(priority) == {
    name for name, group in pads.items() if len(group) > 1}
escapes = {}
for pad in all_pads:
    reference = pad.GetParentFootprint().GetReference()
    number = int(pad.GetNumber()) if pad.GetNumber().isdigit() else 0
    if reference == "J4" and pad.GetNetname() != "GND":
        dx, dy = (5.5 if number == 7 else 5), 0
    elif reference == "J12" and number >= 3:
        dx, dy = 0, -5
    elif reference == "J1" and number in (4, 5, 6):
        dx, dy = 5, 0
    elif reference == "J2" and number in (6, 7, 8):
        dx, dy = -5, 0
    elif reference == "J11" and number in (1, 2, 3):
        dx, dy = 0, -5
    elif reference == "J1" and 15 <= number <= 20:
        dx, dy = 5, 0
    elif reference == "J6" and 3 <= number <= 8:
        dx, dy = -5, 0
    else:
        continue
    position = pad.GetPosition()
    x, y = mm(position.x), mm(position.y)
    end = (x + dx, y + dy)
    track((x, y), end, 0, pad.GetNetname())
    escapes[id(pad)] = point(*end)
    sx, sy = cell(position)
    ex, ey = cell(escapes[id(pad)])
    if dx:
        cells = [(0, ix, sy) for ix in range(min(sx, ex), max(sx, ex) + 1)]
    else:
        cells = [(0, sx, iy) for iy in range(min(sy, ey), max(sy, ey) + 1)]
    mark(cells, pad.GetNetname())

for name in priority:
    group = sorted(pads[name], key=lambda pad: (
        pad.GetParentFootprint().GetReference() not in ("J1", "J2"),
        pad.GetParentFootprint().GetReference(),
        int(pad.GetNumber())))
    first = group.pop(0)
    first_escape = escapes.get(id(first))
    px, py = cell(first_escape or first.GetPosition())
    tree = {(layer, px, py) for layer in (
        (0,) if first_escape else range(len(ROUTE_LAYERS)))}
    while group:
        source = min(group, key=lambda p: min(
            abs(cell(escapes.get(id(p), p.GetPosition()))[0] - x) +
            abs(cell(escapes.get(id(p), p.GetPosition()))[1] - y)
            for _, x, y in tree))
        group.remove(source)
        escape = escapes.get(id(source))
        layers = (0,) if escape or source.GetAttribute() == pcb.PAD_ATTRIB_SMD else range(len(ROUTE_LAYERS))
        route = search(cell(escape or source.GetPosition()), tree, name,
                       start_layers=layers)
        tip = apply(route, source, name, from_position=escape)
        tree.update(route)
        if tip == cell(first_escape or first.GetPosition()):
            end = first_escape or first.GetPosition()
            track(xy(*tip), (mm(end.x), mm(end.y)), route[-1][0], name)
        print(name, source.GetParentFootprint().GetReference(),
              source.GetNumber(), "->", len(route), "cells",
              sum(a[0] != b[0] for a, b in zip(route, route[1:])), "vias",
              flush=True)


def segment_distance(a, b, c, d):
    def point_distance(p, start, end):
        dx, dy = end[0] - start[0], end[1] - start[1]
        length_squared = dx * dx + dy * dy
        t = 0 if not length_squared else max(0, min(1, (
            (p[0] - start[0]) * dx + (p[1] - start[1]) * dy) / length_squared))
        return hypot(p[0] - start[0] - t * dx, p[1] - start[1] - t * dy)

    def cross(p, q, r):
        return (q[0] - p[0]) * (r[1] - p[1]) - (q[1] - p[1]) * (r[0] - p[0])

    if cross(a, b, c) * cross(a, b, d) < 0 and cross(c, d, a) * cross(c, d, b) < 0:
        return 0
    return min(point_distance(a, c, d), point_distance(b, c, d),
               point_distance(c, a, b), point_distance(d, a, b))


def rectangle_distance(start, end, left, top, right, bottom):
    if any(left <= x <= right and top <= y <= bottom for x, y in (start, end)):
        return 0
    corners = ((left, top), (right, top), (right, bottom), (left, bottom))
    return min(segment_distance(start, end, a, b)
               for a, b in zip(corners, corners[1:] + corners[:1]))


def diagonal_clear(start, end, original):
    def coords(position):
        return mm(position.x), mm(position.y)

    start, end = coords(start), coords(end)
    radius = mm(original.GetWidth()) / 2
    clearance = radius + CLEARANCE + 0.02
    if rectangle_distance(start, end, 52, 67, 73.5, 84) <= clearance:
        return False
    for pad in all_pads:
        if pad.GetParentFootprint().GetReference().startswith("H"):
            center = coords(pad.GetPosition())
            if segment_distance(start, end, center, center) <= 3.5 + radius:
                return False
        elif pad.GetNetCode() != original.GetNetCode() and pad.IsOnLayer(original.GetLayer()):
            box = pad.GetBoundingBox()
            if rectangle_distance(start, end, mm(box.GetLeft()), mm(box.GetTop()),
                                  mm(box.GetRight()), mm(box.GetBottom())) <= (
                    radius + max(CLEARANCE, mm(pad.GetLocalClearance() or 0)) + 0.02):
                return False
    for item in board.GetTracks():
        if item.GetNetCode() == original.GetNetCode():
            continue
        if isinstance(item, pcb.PCB_VIA):
            center = coords(item.GetPosition())
            distance = segment_distance(start, end, center, center)
            item_width = item.GetWidth(original.GetLayer())
        elif item.GetLayer() == original.GetLayer():
            distance = segment_distance(start, end, coords(item.GetStart()), coords(item.GetEnd()))
            item_width = item.GetWidth()
        else:
            continue
        if distance <= clearance + mm(item_width) / 2:
            return False
    return True


def chamfer_corners():
    tracks = [item for item in board.GetTracks()
              if isinstance(item, pcb.PCB_TRACK) and not isinstance(item, pcb.PCB_VIA)]
    junctions = defaultdict(list)
    for item in tracks:
        for endpoint in (item.GetStart(), item.GetEnd()):
            junctions[(item.GetLayer(), item.GetNetCode(), endpoint.x, endpoint.y)].append(item)
    fixed = {(item.GetPosition().x, item.GetPosition().y)
             for item in board.GetTracks() if isinstance(item, pcb.PCB_VIA)}
    fixed.update((pad.GetPosition().x, pad.GetPosition().y) for pad in all_pads)
    changed = 0
    for (_, _, x, y), pair in list(junctions.items()):
        if len(pair) != 2 or (x, y) in fixed:
            continue
        a, b = pair
        if a.GetWidth() != b.GetWidth():
            continue
        def vector(item):
            other = item.GetEnd() if item.GetStart().x == x and item.GetStart().y == y else item.GetStart()
            return other.x - x, other.y - y

        ax, ay = vector(a)
        bx, by = vector(b)
        if not ((ax == 0 and by == 0 and ay * bx != 0) or
                (ay == 0 and bx == 0 and ax * by != 0)):
            continue
        shortest = min(abs(ax) + abs(ay), abs(bx) + abs(by))
        if shortest < pcb.FromMM(0.5):
            continue
        cut = shortest // 2
        while cut >= pcb.FromMM(0.05):
            p1 = pcb.VECTOR2I(x + (cut if ax > 0 else -cut if ax < 0 else 0),
                              y + (cut if ay > 0 else -cut if ay < 0 else 0))
            p2 = pcb.VECTOR2I(x + (cut if bx > 0 else -cut if bx < 0 else 0),
                              y + (cut if by > 0 else -cut if by < 0 else 0))
            if diagonal_clear(p1, p2, a):
                break
            cut //= 2
        else:
            continue
        if a.GetStart().x == x and a.GetStart().y == y:
            a.SetStart(p1)
        else:
            a.SetEnd(p1)
        if b.GetStart().x == x and b.GetStart().y == y:
            b.SetStart(p2)
        else:
            b.SetEnd(p2)
        diagonal = pcb.PCB_TRACK(board)
        diagonal.SetStart(p1)
        diagonal.SetEnd(p2)
        diagonal.SetLayer(a.GetLayer())
        diagonal.SetWidth(a.GetWidth())
        diagonal.SetNet(a.GetNet())
        board.Add(diagonal)
        changed += 1
    return changed


print("45-degree corners:", chamfer_corners(), flush=True)

for layer in (pcb.F_Cu, pcb.B_Cu):
    zone = pcb.ZONE(board)
    zone.SetLayer(layer)
    zone.SetNet(board.FindNet("GND"))
    zone.SetPadConnection(pcb.ZONE_CONNECTION_THERMAL)
    zone.SetThermalReliefGap(pcb.FromMM(0.3))
    zone.SetThermalReliefSpokeWidth(pcb.FromMM(0.35))
    zone.SetLocalClearance(pcb.FromMM(0.35))
    zone.SetMinThickness(pcb.FromMM(0.25))
    zone.SetIslandRemovalMode(pcb.ISLAND_REMOVAL_MODE_ALWAYS)
    zone.Outline().NewOutline()
    for x, y in ((4, 4), (122, 4), (122, 114), (4, 114)):
        zone.Outline().Append(pcb.FromMM(x), pcb.FromMM(y))
    board.Add(zone)

# This pad's bottom-side pour is an isolated sliver; use its routed GND
# connection rather than retaining a starved thermal island.
for pad in all_pads:
    if pad.GetParentFootprint().GetReference() == "J1" and pad.GetNumber() == "22":
        pad.SetLocalZoneConnection(pcb.ZONE_CONNECTION_NONE)

if not pcb.ZONE_FILLER(board).Fill(board.Zones()):
    raise RuntimeError("KiCad failed to fill the ground zones")
pcb.SaveBoard(str(FILE), board)

project_file = FILE.with_suffix(".kicad_pro")
project = json.loads(project_file.read_text(encoding="utf-8"))
settings = project["board"]["design_settings"]
settings["defaults"].update({
    "copper_line_width": 0.35,
    "silk_line_width": 0.15,
    "silk_text_thickness": 0.15,
})
settings["rules"].update({
    "min_clearance": 0.3,
    "min_hole_to_hole": 0.45,
    "min_silk_clearance": 0.15,
    "min_text_height": 1.0,
    "min_text_thickness": 0.15,
    "min_track_width": 0.3,
    "min_via_annular_width": 0.18,
})
settings["track_widths"] = [0.35, 0.65, 0.8]
settings["via_dimensions"] = [{"diameter": 0.8, "drill": 0.4}]
project["net_settings"]["classes"][0].update({
    "clearance": 0.3,
    "track_width": 0.35,
    "via_diameter": 0.8,
    "via_drill": 0.4,
})
project_file.write_text(json.dumps(project, indent=2) + "\n", encoding="utf-8")
