"""Route the carrier's assigned nets with grid clearance.

This is a deterministic layout aid, not a substitute for KiCad DRC or
measuring the actual mating modules.
"""

import json
from collections import defaultdict
from heapq import heappop, heappush
from functools import lru_cache
from math import ceil, floor, hypot
from pathlib import Path

import pcbnew as pcb


FILE = Path(__file__).with_name("minibox-carrier.kicad_pcb")
board = pcb.LoadBoard(str(FILE))
STEP = 0.25
edge_points = [
    position for item in board.GetDrawings() if item.GetLayer() == pcb.Edge_Cuts
    for position in (item.GetStart(), item.GetEnd())
]
LEFT = min(pcb.ToMM(position.x) for position in edge_points)
TOP = min(pcb.ToMM(position.y) for position in edge_points)
RIGHT = max(pcb.ToMM(position.x) for position in edge_points)
BOTTOM = max(pcb.ToMM(position.y) for position in edge_points)
X0, Y0 = LEFT + 2, TOP + 2
NX = floor((RIGHT - 2 - X0) / STEP + 1e-6) + 1
NY = floor((BOTTOM - 2 - Y0) / STEP + 1e-6) + 1
WIDTH = 0.2
CLEARANCE = 0.2
VIA_DIAMETER = 0.6
VIA_DRILL = 0.3
VIA_COST = 75
ROUTE_LAYERS = (pcb.F_Cu, pcb.B_Cu, pcb.In2_Cu)
POWER_WIDTHS = {"3V3": 0.65, "5V_IN": 0.8, "5V_SW": 0.8}
I2S_NETS = {"I2S_WS", "I2S_BCLK", "AUDIO_DIN", "MIC_SD"}
KEEP_OUTS = []
for area in board.Zones():
    if area.GetIsRuleArea() and area.GetDoNotAllowTracks():
        box = area.GetBoundingBox()
        bounds = tuple(pcb.ToMM(v) for v in (
            box.GetLeft(), box.GetTop(), box.GetRight(), box.GetBottom()))
        if bounds not in KEEP_OUTS:
            KEEP_OUTS.append(bounds)
if board.GetCopperLayerCount() != 4 or list(board.GetTracks()):
    raise RuntimeError("Run generate_board.py before routing the four-layer board")


def layers_for(name):
    if name in POWER_WIDTHS:
        return (0, 2)
    if name in I2S_NETS:
        return (0, 1)
    return (0, 1)


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
pad_blocks = {
    key: [defaultdict(set) for _ in ROUTE_LAYERS]
    for key in (WIDTH, *set(POWER_WIDTHS.values()), "I2S")
}
via_pad_blocks = defaultdict(set)


def pad_cells(pad, margin):
    center = pad.GetPosition()
    cx, cy = mm(center.x), mm(center.y)
    px, py = cell(center)
    box = pad.GetBoundingBox()
    radius = max(mm(pad.GetSize().x), mm(pad.GetSize().y)) / 2
    reach = ceil((radius + margin) / STEP) + 1
    for a in range(max(0, px - reach), min(NX, px + reach + 1)):
        for b in range(max(0, py - reach), min(NY, py + reach + 1)):
            x, y = xy(a, b)
            if pad.GetShape() == pcb.PAD_SHAPE_CIRCLE:
                distance = max(0, hypot(x - cx, y - cy) - radius)
            else:
                distance = hypot(
                    max(mm(box.GetLeft()) - x, 0, x - mm(box.GetRight())),
                    max(mm(box.GetTop()) - y, 0, y - mm(box.GetBottom())))
            if distance <= margin:
                yield a, b


for pad in all_pads:
    px, py = cell(pad.GetPosition())
    pad_clearance = max(CLEARANCE, mm(pad.GetLocalClearance() or 0))
    radius = max(mm(pad.GetSize().x), mm(pad.GetSize().y)) / 2
    if pad.GetParentFootprint().GetReference().startswith("H"):
        radius = 3.5
    for key, layers in pad_blocks.items():
        signal_width = WIDTH if key == "I2S" else key
        reference_clearance = 0.4 if key == "I2S" else pad_clearance
        clearance = max(pad_clearance, reference_clearance)
        if pad.GetAttribute() == pcb.PAD_ATTRIB_NPTH:
            clearance = max(clearance, 0.5)
        for layer in range(len(ROUTE_LAYERS)):
            if not pad.IsOnLayer(ROUTE_LAYERS[layer]):
                continue
            for coordinate in pad_cells(pad, signal_width / 2 + clearance + 0.025):
                layers[layer][coordinate].add(pad.GetNetname() or "UNASSIGNED")
    via_radius = 3.5 if pad.GetParentFootprint().GetReference().startswith("H") else (
        max(mm(pad.GetSize().x), mm(pad.GetSize().y)) / 2)
    for coordinate in disk(px, py, via_radius +
                           VIA_DIAMETER / 2 + pad_clearance + STEP / 2):
        via_pad_blocks[coordinate].add("PAD")

# Use the saved rule areas rather than the previous layout's coordinates.
for x in range(NX):
    for y in range(NY):
        px, py = xy(x, y)
        if any(a - WIDTH / 2 - CLEARANCE <= px <= c + WIDTH / 2 + CLEARANCE and
               b - WIDTH / 2 - CLEARANCE <= py <= d + WIDTH / 2 + CLEARANCE
               for a, b, c, d in KEEP_OUTS):
            for layer in blocked:
                layer[(x, y)].add("ANTENNA")
        if any(a - VIA_DIAMETER / 2 - CLEARANCE <= px <= c + VIA_DIAMETER / 2 + CLEARANCE and
               b - VIA_DIAMETER / 2 - CLEARANCE <= py <= d + VIA_DIAMETER / 2 + CLEARANCE
               for a, b, c, d in KEEP_OUTS):
            via_pad_blocks[(x, y)].add("ANTENNA")


def free(layer, x, y, name):
    key = "I2S" if name in I2S_NETS else width_for(name)
    if pad_blocks[key][layer][(x, y)] - {name}:
        return False
    extra = max(0, (width_for(name) - WIDTH) / 2)
    coordinates = disk(x, y, extra + STEP / 2) if extra else ((x, y),)
    return not any(blocked[layer][coordinate] - {name} for coordinate in coordinates)


def via_free(x, y, name):
    if via_pad_blocks[(x, y)] - {name}:
        return False
    for layer in range(len(ROUTE_LAYERS)):
        for a, b in disk(x, y, VIA_DIAMETER / 2 + WIDTH / 2 + CLEARANCE):
            if not free(layer, a, b, name):
                return False
    return True


def search(start, targets, name, start_layers=None):
    @lru_cache(maxsize=None)
    def available(layer, x, y):
        return free(layer, x, y, name)

    @lru_cache(maxsize=None)
    def available_via(x, y):
        return via_free(x, y, name)

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
    allowed_layers = layers_for(name)
    for layer in allowed_layers if start_layers is None else start_layers:
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
            if not available(layer, nx, ny):
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
        if available_via(x, y):
            for next_layer in allowed_layers:
                if next_layer == layer:
                    continue
                other = (next_layer, x, y)
                if cost + VIA_COST < costs.get(other, 10**12):
                    costs[other] = cost + VIA_COST
                    previous[other] = state
                    heappush(queue, (cost + VIA_COST + 10 * heuristic(x, y),
                                     cost + VIA_COST, other))
    raise RuntimeError(
        f"No path for {name} starting at {start}; explored {len(costs)} cells; "
        f"goals {[(g, sorted(blocked[g[0]][g[1:]] - {name})) for g in goals]}")


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
        if name in I2S_NETS and layer == 1:
            # Preserve a GND reference corridor on In2 under bottom I2S.
            for a, b in disk(x, y, 1.75):
                blocked[2][(a, b)].add("I2S_REFERENCE_GND")
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
    "AUDIO_DIN", "MIC_SD", "I2S_BCLK", "I2S_WS",
    "BUTTON4", "BUTTON3", "RECORD", "BUTTON1",
    "USB_CC1", "USB_CC2",
    "LCD_BL", "LCD_CS", "LCD_RST", "LCD_DC", "LCD_MOSI", "LCD_SCK",
    "5V_IN",
    "GPIO1", "GPIO2",
    "RTC_SDA", "RTC_SCL",
    "AMP_GAIN", "AMP_SD",
    "5V_SW",
    "3V3",
]
assert set(priority) == {
    name for name, group in pads.items() if len(group) > 1 and name != "GND"}
escapes = {}
for pad in all_pads:
    reference = pad.GetParentFootprint().GetReference()
    number = int(pad.GetNumber()) if pad.GetNumber().isdigit() else 0
    if pad.GetNetname() in POWER_WIDTHS or pad.GetNetname() == "GND":
        continue
    if reference == "J4" and pad.GetNetname() in I2S_NETS:
        dx, dy = 5, 0
    elif reference == "J1" and pad.GetNetname() in I2S_NETS:
        dx, dy = 0, 4
    elif reference == "J2" and pad.GetNetname() in I2S_NETS:
        dx, dy = 0, 4
    elif reference in ("R1", "R2") and number == 1:
        dx, dy = -2, 0
    else:
        continue
    position = pad.GetPosition()
    x, y = mm(position.x), mm(position.y)
    end = xy(*cell(point(x + dx, y + dy)))
    if dx:
        turn = (x + dx - (0.5 if dx > 0 else -0.5), y)
        diagonal = (turn[0] + (1 if dx > 0 else -1) * abs(end[1] - y), end[1])
    else:
        turn = (x, y + dy - (0.5 if dy > 0 else -0.5))
        diagonal = (end[0], turn[1] + (1 if dy > 0 else -1) * abs(end[0] - x))
    for a, b in zip(((x, y), turn, diagonal), (turn, diagonal, end)):
        track(a, b, 0, pad.GetNetname())
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
        pad.GetNumber()))
    first = group.pop(0)
    first_escape = escapes.get(id(first))
    px, py = cell(first_escape or first.GetPosition())
    tree = {(layer, px, py) for layer in (
        (0,) if first_escape else layers_for(name))}
    while group:
        source = min(group, key=lambda p: min(
            abs(cell(escapes.get(id(p), p.GetPosition()))[0] - x) +
            abs(cell(escapes.get(id(p), p.GetPosition()))[1] - y)
            for _, x, y in tree))
        group.remove(source)
        escape = escapes.get(id(source))
        layers = (0,) if escape or source.GetAttribute() == pcb.PAD_ATTRIB_SMD else layers_for(name)
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
    if any(rectangle_distance(start, end, *bounds) <= clearance for bounds in KEEP_OUTS):
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
    def add_segment(start, end, original):
        item = pcb.PCB_TRACK(board)
        item.SetStart(start)
        item.SetEnd(end)
        item.SetLayer(original.GetLayer())
        item.SetWidth(original.GetWidth())
        item.SetNet(original.GetNet())
        board.Add(item)

    def on_segment(item, x, y):
        start, end = item.GetStart(), item.GetEnd()
        return ((end.x - start.x) * (y - start.y) ==
                (end.y - start.y) * (x - start.x) and
                min(start.x, end.x) <= x <= max(start.x, end.x) and
                min(start.y, end.y) <= y <= max(start.y, end.y))

    tracks = [item for item in board.GetTracks()
              if isinstance(item, pcb.PCB_TRACK) and not isinstance(item, pcb.PCB_VIA)]
    junctions = defaultdict(list)
    for item in tracks:
        for endpoint in (item.GetStart(), item.GetEnd()):
            junctions[(item.GetLayer(), item.GetNetCode(), endpoint.x, endpoint.y)].append(item)
    fixed = {(item.GetPosition().x, item.GetPosition().y)
             for item in board.GetTracks() if isinstance(item, pcb.PCB_VIA)}
    fixed.update((pad.GetPosition().x, pad.GetPosition().y) for pad in all_pads)
    anchors = defaultdict(set)
    for layer, net_code, x, y in junctions:
        anchors[(layer, net_code)].add((x, y))
    changed = 0
    for (layer, net_code, x, y), pair in list(junctions.items()):
        if len(pair) != 2:
            continue
        a, b = pair
        if a.GetWidth() != b.GetWidth():
            continue
        anchored = (x, y) in fixed or any(
            item not in pair and not isinstance(item, pcb.PCB_VIA) and
            item.GetLayer() == layer and item.GetNetCode() == net_code and
            on_segment(item, x, y) for item in board.GetTracks())
        def vector(item):
            other = item.GetEnd() if item.GetStart().x == x and item.GetStart().y == y else item.GetStart()
            return other.x - x, other.y - y

        ax, ay = vector(a)
        bx, by = vector(b)
        if ax * bx + ay * by != 0:
            continue
        if not all(vx == 0 or vy == 0 or abs(vx) == abs(vy)
                   for vx, vy in ((ax, ay), (bx, by))):
            continue
        shortest = min(max(abs(ax), abs(ay)), max(abs(bx), abs(by)))
        if shortest < 2:
            continue
        if anchored:
            original, u, v = ((a, (ax, ay), (bx, by))
                              if max(abs(ax), abs(ay)) >= max(abs(bx), abs(by))
                              else (b, (bx, by), (ax, ay)))
            jog = min(pcb.FromMM(0.25), max(abs(u[0]), abs(u[1])) // 4)
            for ox, oy in fixed | anchors[(layer, net_code)]:
                dx, dy = ox - x, oy - y
                if (u[0] * dy == u[1] * dx and
                        0 < u[0] * dx + u[1] * dy < u[0] * u[0] + u[1] * u[1]):
                    jog = min(jog, max(abs(dx), abs(dy)) // 3)
            ux, uy = (1 if n > 0 else -1 if n < 0 else 0 for n in u)
            vx, vy = (1 if n > 0 else -1 if n < 0 else 0 for n in v)
            while jog > 0:
                points = (
                    pcb.VECTOR2I(x, y),
                    pcb.VECTOR2I(x + jog * (ux - vx), y + jog * (uy - vy)),
                    pcb.VECTOR2I(x + jog * (2 * ux - vx), y + jog * (2 * uy - vy)),
                    pcb.VECTOR2I(x + 3 * jog * ux, y + 3 * jog * uy),
                )
                if all(diagonal_clear(start, end, original)
                       for start, end in zip(points, points[1:])):
                    break
                jog //= 2
            else:
                continue
            # Keep the via/pad/tee anchor and ease one outgoing leg instead.
            if original.GetStart().x == x and original.GetStart().y == y:
                original.SetStart(points[-1])
            else:
                original.SetEnd(points[-1])
            for start, end in zip(points, points[1:]):
                add_segment(start, end, original)
            changed += 1
            continue
        cut = shortest * 3 // 4
        # Tree branches and vias can join a leg between its endpoints.
        for ox, oy in fixed | anchors[(layer, net_code)]:
            dx, dy = ox - x, oy - y
            for vx, vy in ((ax, ay), (bx, by)):
                if (vx * dy == vy * dx and
                        0 < vx * dx + vy * dy < vx * vx + vy * vy):
                    cut = min(cut, max(abs(dx), abs(dy)))
        while cut > 0:
            p1 = pcb.VECTOR2I(x + (cut if ax > 0 else -cut if ax < 0 else 0),
                              y + (cut if ay > 0 else -cut if ay < 0 else 0))
            p2 = pcb.VECTOR2I(x + (cut if bx > 0 else -cut if bx < 0 else 0),
                              y + (cut if by > 0 else -cut if by < 0 else 0))
            if diagonal_clear(p1, p2, a):
                break
            cut -= min(pcb.FromMM(0.25), max(1, cut // 2))
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
        add_segment(p1, p2, a)
        changed += 1
    return changed


changed_corners = 0
for _ in range(8):
    changed = chamfer_corners()
    changed_corners += changed
    if not changed:
        break
print("45-degree corners:", changed_corners, flush=True)
pcb.SaveBoard(str(FILE), board)

remaining_junctions = defaultdict(list)
for item in board.GetTracks():
    if isinstance(item, pcb.PCB_VIA):
        continue
    for endpoint in (item.GetStart(), item.GetEnd()):
        remaining_junctions[(item.GetLayer(), item.GetNetCode(),
                             endpoint.x, endpoint.y)].append(item)
for (_, _, x, y), pair in remaining_junctions.items():
    if len(pair) != 2:
        continue
    vectors = []
    for item in pair:
        other = item.GetEnd() if item.GetStart().x == x and item.GetStart().y == y else item.GetStart()
        vectors.append((other.x - x, other.y - y))
    inside_pad = any(
        pad.GetNetCode() == pair[0].GetNetCode() and
        hypot(mm(pad.GetPosition().x - x), mm(pad.GetPosition().y - y)) +
        mm(pair[0].GetWidth()) / 2 <= min(mm(pad.GetSize().x), mm(pad.GetSize().y)) / 2
        for pad in all_pads)
    dot = vectors[0][0] * vectors[1][0] + vectors[0][1] * vectors[1][1]
    if not inside_pad and all(vector != (0, 0) for vector in vectors) and dot >= 0:
        raise RuntimeError(f"Unresolved right/acute bend: {pair[0].GetNetname()} at "
                           f"({mm(x)}, {mm(y)})")

tracks = list(board.GetTracks())
print("Signal vias:", sum(isinstance(item, pcb.PCB_VIA) for item in tracks), flush=True)
print("Track length / mm:", round(sum(mm(item.GetLength()) for item in tracks
                                     if not isinstance(item, pcb.PCB_VIA)), 1), flush=True)


def add_zone(layer, name, outline=None, priority=0):
    zone = pcb.ZONE(board)
    zone.SetLayer(layer)
    zone.SetNet(board.FindNet(name))
    zone.SetAssignedPriority(priority)
    zone.SetPadConnection(pcb.ZONE_CONNECTION_THERMAL)
    zone.SetThermalReliefGap(pcb.FromMM(0.3))
    zone.SetThermalReliefSpokeWidth(pcb.FromMM(0.35))
    zone.SetLocalClearance(pcb.FromMM(0.35))
    zone.SetMinThickness(pcb.FromMM(0.25))
    zone.SetIslandRemovalMode(pcb.ISLAND_REMOVAL_MODE_ALWAYS)
    if outline is None:
        zone.Outline().NewOutline()
        for x, y in ((LEFT + 1, TOP + 1), (RIGHT - 1, TOP + 1),
                     (RIGHT - 1, BOTTOM - 1), (LEFT + 1, BOTTOM - 1)):
            zone.Outline().Append(pcb.FromMM(x), pcb.FromMM(y))
    else:
        zone.SetOutline(outline)
        # SetOutline transfers ownership to the KiCad zone.
        outline.thisown = False
    board.Add(zone)


for layer in (pcb.F_Cu, pcb.In1_Cu, pcb.B_Cu):
    add_zone(layer, "GND")
add_zone(pcb.In2_Cu, "GND")
for name in ("3V3", "5V_IN", "5V_SW"):
    outline = pcb.SHAPE_POLY_SET()
    for item in tracks:
        if item.GetNetname() != name or item.GetLayer() != pcb.In2_Cu:
            continue
        shape = pcb.SHAPE_POLY_SET()
        item.TransformShapeToPolygon(
            shape, pcb.In2_Cu, pcb.FromMM(0.7), pcb.FromMM(0.01),
            pcb.ERROR_INSIDE)
        outline.BooleanAdd(shape)
    if outline.OutlineCount():
        add_zone(pcb.In2_Cu, name, outline, priority=1)


def ground_site_clear(x, y):
    if not (LEFT + 2 <= x <= RIGHT - 2 and TOP + 2 <= y <= BOTTOM - 2):
        return False
    radius = VIA_DIAMETER / 2
    if any(rectangle_distance((x, y), (x, y), *bounds) <= radius + CLEARANCE
           for bounds in KEEP_OUTS):
        return False
    for pad in all_pads:
        if pad.GetParentFootprint().GetReference().startswith("H"):
            if hypot(x - mm(pad.GetPosition().x), y - mm(pad.GetPosition().y)) <= 3.5 + radius:
                return False
        else:
            box = pad.GetBoundingBox()
            if rectangle_distance(
                    (x, y), (x, y), mm(box.GetLeft()), mm(box.GetTop()),
                    mm(box.GetRight()), mm(box.GetBottom())) <= radius + max(
                        CLEARANCE, mm(pad.GetLocalClearance() or 0)) + 0.02:
                return False
    for item in board.GetTracks():
        if isinstance(item, pcb.PCB_VIA):
            if hypot(x - mm(item.GetPosition().x), y - mm(item.GetPosition().y)) <= (
                    radius + mm(item.GetWidth(pcb.F_Cu)) / 2 + CLEARANCE + 0.02):
                return False
        elif item.GetNetname() != "GND" and segment_distance(
                (x, y), (x, y),
                (mm(item.GetStart().x), mm(item.GetStart().y)),
                (mm(item.GetEnd().x), mm(item.GetEnd().y))) <= (
                    radius + mm(item.GetWidth()) / 2 + CLEARANCE + 0.02):
            return False
    return True


def stitch_near(position, max_distance=3):
    if any(isinstance(t, pcb.PCB_VIA) and t.GetNetname() == "GND" and
           hypot(mm(t.GetPosition().x - position.x),
                 mm(t.GetPosition().y - position.y)) <= max_distance
           for t in board.GetTracks()):
        return
    x, y = cell(position)
    candidates = sorted(disk(x, y, max_distance),
                        key=lambda c: hypot(c[0] - x, c[1] - y))
    for a, b in candidates:
        px, py = xy(a, b)
        if hypot(px - mm(position.x), py - mm(position.y)) > max_distance:
            continue
        if not ground_site_clear(px, py):
            continue
        via(a, b, "GND")
        return
    raise RuntimeError(f"No ground stitching site near {xy(x, y)}")


for item in tracks:
    if isinstance(item, pcb.PCB_VIA):
        stitch_near(item.GetPosition())
    elif item.GetNetname() in I2S_NETS:
        length = mm(item.GetLength())
        for index in range(1, ceil(length / 10)):
            ratio = index / ceil(length / 10)
            start, end = item.GetStart(), item.GetEnd()
            stitch_near(point(
                mm(start.x) + ratio * mm(end.x - start.x),
                mm(start.y) + ratio * mm(end.y - start.y)))

if not pcb.ZONE_FILLER(board).Fill(board.Zones()):
    raise RuntimeError("KiCad failed to fill the ground zones")
pcb.SaveBoard(str(FILE), board)

ground_plane = next(
    zone.GetFilledPolysList(pcb.In1_Cu) for zone in board.Zones()
    if zone.GetLayer() == pcb.In1_Cu and zone.GetNetname() == "GND")
if ground_plane.OutlineCount() != 1:
    raise RuntimeError("The inner GND plane must remain a single connected region")
endpoint_boxes = {
    name: [pad.GetBoundingBox() for pad in pads[name]] for name in I2S_NETS
}
for item in board.GetTracks():
    if isinstance(item, pcb.PCB_VIA) and item.GetNetname() in I2S_NETS:
        endpoint_boxes[item.GetNetname()].append(item.GetBoundingBox())
bottom_ground_plane = next(
    zone.GetFilledPolysList(pcb.In2_Cu) for zone in board.Zones()
    if zone.GetLayer() == pcb.In2_Cu and zone.GetNetname() == "GND")
for item in board.GetTracks():
    if isinstance(item, pcb.PCB_VIA):
        continue
    if item.GetNetname() in POWER_WIDTHS:
        if item.GetLayer() not in (pcb.F_Cu, pcb.In2_Cu):
            raise RuntimeError("Power routing must stay on F.Cu/In2.Cu")
    elif item.GetLayer() not in (pcb.F_Cu, pcb.B_Cu):
        raise RuntimeError("Signal routing must stay on the outer layers")
    if item.GetNetname() not in I2S_NETS:
        continue
    reference_plane = ground_plane if item.GetLayer() == pcb.F_Cu else bottom_ground_plane
    start, end = item.GetStart(), item.GetEnd()
    samples = max(1, ceil(mm(item.GetLength()) / 0.1))
    for index in range(samples):
        ratio = (index + 0.5) / samples
        position = pcb.VECTOR2I(
            round(start.x + ratio * (end.x - start.x)),
            round(start.y + ratio * (end.y - start.y)))
        if reference_plane.Contains(position):
            continue
        # A plated signal pad necessarily has an antipad in the GND plane.
        margin = pcb.FromMM(0.4)
        if any(
                box.GetLeft() - margin <= position.x <= box.GetRight() + margin and
                box.GetTop() - margin <= position.y <= box.GetBottom() + margin
                for box in endpoint_boxes[item.GetNetname()]):
            continue
        raise RuntimeError(
            f"{item.GetNetname()} loses its GND reference at "
            f"({mm(position.x):.3f}, {mm(position.y):.3f})")
print("I2S reference: verified filled In1/In2 GND outside connector antipads", flush=True)
pcb.SaveBoard(str(FILE), board)

project_file = FILE.with_suffix(".kicad_pro")
project = json.loads(project_file.read_text(encoding="utf-8"))
settings = project["board"]["design_settings"]
settings["defaults"].update({
    "copper_line_width": 0.2,
    "silk_line_width": 0.15,
    "silk_text_thickness": 0.15,
})
settings["rules"].update({
    "min_clearance": 0.2,
    "min_hole_to_hole": 0.45,
    "min_silk_clearance": 0.15,
    "min_text_height": 1.0,
    "min_text_thickness": 0.15,
    "min_track_width": 0.2,
    "min_via_annular_width": 0.15,
})
settings["track_widths"] = [0.2, 0.65, 0.8]
settings["via_dimensions"] = [{"diameter": 0.6, "drill": 0.3}]
project["net_settings"]["classes"][0].update({
    "clearance": 0.2,
    "track_width": 0.2,
    "via_diameter": 0.6,
    "via_drill": 0.3,
})
project_file.write_text(json.dumps(project, indent=2) + "\n", encoding="utf-8")
