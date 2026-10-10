# Minibox carrier v1.2 — 2026-10-10

This is the user's actual `kicad` carrier, not the separate integrated design.
Open `minibox-carrier.kicad_pro` in KiCad 10. There is **no schematic**:
PCB DRC/net checks do not constitute ERC or schematic-parity verification.
The direct-solder revision is implemented; final review is documented in
[the Chinese HTML guide](carrier-revision.html) and [revision checklist](layout-revision-plan.md).
**Engineering-review files are not permission to manufacture or order.**

## Layout and parts

The board is **94.5 × 117 mm, R5**, four copper layers, nominal 1.6 mm.
Four Ø3.0 mm NPTH holes have **84.5 × 107 mm** center spacing;
KiCad centers are (10,8), (94.5,8), (10,115), (94.5,115) mm.
Each hole has a 3.5 mm-radius/square conservative all-layer copper keepout.
Ø3.0 mm is a tight M3 fit: check screw size/tolerance and the new enclosure.
Bounding-rectangle area is only 1.28% smaller than the old 100 × 112 mm board.

| Placement | Required reservation |
|---|---|
| ESP32 | USB left, antenna right; estimated body stays inside PCB |
| RTC | Upright, battery included in 27.5 × 12 mm reservation; 3.03 mm to ESP32 estimate |
| MAX98357 | Flat 27 × 24 mm reservation; bottom Y=72, above keycap top Y=80.5 |
| Red boundary | Y=76; upper bodies ≤74, lower operation envelopes ≥78 |
| Joystick JS1 | Vertical, center (16.8,95.5); **30 mm diameter** motion may overhang 3.20 mm; body/pad copper retain ≥1 mm to the left copper boundary |
| Microphone | Center (44.455,90.25), Ø19.5 mm envelope; top Y80.5; horizontal maximin clearance to all keycaps and joystick motion is approximately 3.40 mm |
| Four keys | Standard 19.05 mm pitch both ways; lower row moves right 1.425 mm, retaining 8.10 mm left stagger and 0.20 mm to the screw square |
| Keycaps | Maximum accepted envelope 18 × 18 mm; neighboring gap 1.05 mm |
| Wire entries | J8/J9 left, J7 right; external wire/tool clearance still requires a fit check |

SW1–SW4 use **HanElectricity CPG151101D13 / C49234235**; JS1 uses
**YTL YV13S-L7.85-B10Ka(60)-0-DL01 / C37323747**.
Their local footprints are traced to the exact linked drawings, not generic
MX/joystick substitutes. The joystick has a press mechanism but **it is intentionally
unused**; switch and mounting leads have no net; **GPIO42 now controls amplifier GAIN**.
Reference PDFs and layout images are in [`hardware_references`](../hardware_references/carrier-revision-references.md).
The exact parts lack precise supplied 3D models; renders must not be mistaken
for complete assembled-product models. Module envelopes and keycap size are
acceptance reservations, not measured/verified models.

J11/J12 cable sockets are removed. Remaining female strips: 1×22 two,
1×14 one, 1×7 one, 1×6 one, **1×3 two**, totaling seven strips/77 contacts.
J3 is two separate 1×3 sockets, row spacing 7.62 mm, **not** a normal 2×3 socket.
Three 5.08 mm screw terminals, four direct-solder switches, one direct-solder
joystick, and two 0402 resistors complete the assembly. All components install
from the front; THT leads solder from the back.
J7 is **KANGNEX WJ500V-5.08-03P-14-00A / C72334**; J8/J9 are
**WJ500V-5.08-2P / C8465**. Exact local footprints include the 0.60 mm joining
lug, 10 mm body depth, 14.07 mm height, and a tolerance-aware courtyard.
J8/J9 move left2.5/up3.94 mm from the prior version, with anchors
(11.5,48.56)/(11.5,61.06). J8's actual body top aligns with the estimated
ESP32 body bottom at Y45.97; their wire entries and electrical order are unchanged.
The selected terminal holes are **Ø1.50 mm**, with unchanged Ø2.60 mm pads
and 0.55 mm radial annular rings. No original Phoenix 3D model is reused
to impersonate the selected WJ500V; current renders show its holes/outline,
not an exact terminal body.

## GPIO and Type-C

Keys → GPIO4/5/6/7 (active-low, internal pulls); RTC SDA/SCL →15/16;
MIC_SD →21; amplifier GAIN/SD_MODE →42/47; GPIO17 is NC. I2S WS/BCLK/DIN →39/40/41;
joystick VR1/VR2 wipers →1/2; LCD remains GPIO9–14.
Actual front silkscreen identifies each module/terminal signal and its GPIO
using digits only, for example `SDA15`, `BCLK40`, `DIN41`, and `KEY1 4`.
Power pins retain `3V3`/`5V`/`GND`; disconnected pins retain `NC`.
Labels are on `F.SilkS`, not just the non-printing fabrication layer.
See [the authoritative wiring table](../docs/hardware-connections.md).
RTC front-view order is CLK/INT/SCL/SDA/3V3/GND; only functional net names
and labels changed, retaining both original copper paths. INMP441 front-view
left top-to-bottom is L/R(GND), WS39, SCK40; right is GND, 3V3, SD21.
The lower-row centre reaches approximately58% of the upper-left key width:
the requested approximate2/3 position would overlap the lower-right screw square
with18 mm keycaps. The screw exclusion takes priority without moving the hole.
The joystick's leftmost pad copper is X7.22 mm:1.22 mm from the X6 mm
copper-pour boundary and2.22 mm from the board edge. The body gap is1.05 mm.
Octal-memory GPIO35/36/37 are unused, as are USB19/20 and UART43/44.
GPIO39–42 cannot simultaneously be used for external four-wire JTAG.
Actual clone RGB firmware remains GPIO48; official v1.1 uses38, requiring
physical board identification, not an automatic firmware change.
The requested independent GPIO audit checked all44 J1/J2 pads against the
[official v1.1 J1/J3 table](https://docs.espressif.com/projects/esp-dev-kits/zh_CN/latest/esp32s3/esp32-s3-devkitc-1/user_guide_v1.1.html#j3):
no mapping errors or peripheral conflicts were found. Verify the clone's
actual pinout/RGB and whether its INMP441 module includes the required SD
100 kΩ pull-down before production; neither module assumption is proven by DRC.

External Type-C VBUS/GND →J8; independent CC1/CC2 →J9 pads1/2.
R1/R2 each pull their own CC to GND through **5.1 kΩ, 0402, 1%**.
Do not short CC1/CC2 or parallel Rd already present on a Type-C breakout.
This identifies a ordinary **5 V sink**, not USB-PD or a 3 A guarantee.
No USB data traces are routed. Check regulator current and USB/external-5V
backfeeding before connecting two sources.

## Routing and manufacturing review

F.Cu signals/GND; In1 continuous GND; In2 separated power corridors/GND;
B.Cu signals/GND. I2S prefers F.Cu/In1, with limited B.Cu/In2-ground-reference
branches. The router checks actual filled reference copper every ≤0.1 mm
outside necessary same-net antipads and requires one connected In1 region.
Signal vias receive nearby GND stitching. This is not a signal-integrity test.
Ground stitching was pruned from41 to23 vias after rerouting;18 redundant
vias were removed, with candidate-by-candidate connectivity/refill checks.
The current manifest records the final counts: retain return paths within
3 mm of every signal/power via and connections to separate ground-copper regions.
Do not add periodic stitching along same-layer I2S traces merely for their length;
the continuous reference plane carries their return current.

Signals/clearance **0.20/0.20 mm**, 3V3 feeders0.65 mm, 5V feeders0.80 mm;
ordinary vias **0.60/0.30 mm**. Copper-edge0.50 mm; PTH hole spacing0.45 mm;
Track-to-other-net pad clearance is **0.30 mm**, or **0.50 mm** at 5V pads.
The upper keys retain their previous positions; rightmost keycap-to-edge gap0.60 mm.
These tighter mechanical edge margins require an enclosure/cap tolerance check.
silk ≥1.0 mm with0.15 mm strokes. Header holes1.0/pads1.7 mm;
terminal holes1.5/pads2.6 mm; switch holes1.5/pads2.1 mm.
Do not treat signal width or these power feeders as a 3 A qualification.
Actual factory stackup/copper thickness, temperature rise and power bottlenecks
remain review items. [Domestic JLC capability rules](../docs/jlc-pcb-design-spec.md)
distinguish limits from recommendations.

Actual latest DRC/mechanical metrics and hashes belong in
`assembly\manifest.json` and `drc.json`;
an absent report or failed gate means export is incomplete.
Only a clean saved board may pass `export_revision.py`.
The exporter checks all plated-pad/via centers on all four copper CAM layers
against the saved PCB, and checks PTH/NPTH drill coordinates and diameters using
the same lower-left origin. This is digital alignment, not factory registration certification.
R1/R2 use verified UNI-ROYAL 0402WGF5101TCE/C25905, 5.1 kΩ ±1%;
domestic catalog stock is not reserved. Full-assembly BOM now identifies all
17 physical pieces with C numbers, including the five LAIL female-strip sizes.
Factory insertion/soldering approval, actual fit and final supply leave the BOM
**review-only**, even if DRC is clean.

### JLC placement-library mapping

Use `assembly\positions-jlc-review.csv` for the selected
domestic JLC library, together with the full BOM and the unchanged Gerber ZIP.
`positions-all-review.csv` records KiCad pad centres/footprint angles; it is
**not directly interchangeable** with the JLC file. In the factory library,
the seven female strips have a different zero angle and JS1 has a different
head datum. The exporter fits numbered library pads to actual saved PCB pads,
including the two separate J3 strips, and rejects mismatched parts or geometry.
`jlc-placement-review.json` records each angle, origin and pad-fit residual.
It also records the KiCad centre/angle and the factory origin/angle corrections.
`jlc-saved-placement.json` preserves the actual factory-saved, refreshed readback
for this board hash. Generated placements are checked against that snapshot at
the website's coordinate precision. After changing the layout, regenerate from
the new PCB: reuse library datum definitions, never copy old absolute positions.
The old snapshot is then historical evidence, not approval of the new layout.

This corrects assembly instructions, not copper or holes. Gerber has no
component-library rotation/model mapping; do not rotate PCB footprints or
change Gerber geometry to repair a factory preview. Missing J4/J6 3D models
are not missing parts. Verify the website's rotation convention and saved
positions against the actual Gerber pads before approving factory DFM.
The files remain engineering review inputs, not production approval.

Regenerate only these placement files without rerouting or re-exporting CAM:

```powershell
& 'C:\Program Files\KiCad\10.0\bin\python.exe' .\kicad\export_jlc_positions.py
```

## Current outputs and Git history

| Current revision | Purpose |
|---|---|
| `renders\assembly-layout.svg` | Reservation layout, not exact assembled geometry |
| `renders\minibox-carrier-front.png` / `back.png` | Genuine saved-board renders |
| `renders\minibox-carrier-isometric.png` | Genuine saved-board perspective |
| `renders\minibox-carrier-front-legend.png` / `back-legend.png` | Current Chinese material legends and board/hole dimensions |
| `assembly` | DRC, manifest, SMT BOM/CPL, separate THT positions, pin map |
| `fabrication` | Four-copper-layer Gerbers and separate PTH/NPTH drills |
| `fabrication\minibox-gerber-review.zip` | PCB CAM review archive |

Only fixed current paths are retained. Previous quote CSVs, dated revision
directories and older archives are recoverable from Git history, not alternate
submission inputs. The manifest records PCB/artifact hashes rather than a date
as a version identifier. Do not embed the current commit SHA in generated files:
it would describe the pre-export commit and change again when those files commit.
No order, upload, purchase or payment is authorized by these files.

## Reproducible commands

From `C:\gitroot\minibox`, after explicitly backing up the routed board:

```powershell
& 'C:\Program Files\KiCad\10.0\bin\python.exe' .\kicad\generate_board.py --replace-routed
& 'C:\Program Files\KiCad\10.0\bin\python.exe' .\kicad\route_board.py
& 'C:\Program Files\KiCad\10.0\bin\python.exe' .\kicad\prune_ground_vias.py
& 'C:\Program Files\KiCad\10.0\bin\python.exe' .\kicad\export_revision.py
```

Generation replaces routing, so its default guard refuses to overwrite tracks.
Inspect every command's result; do not continue after a failure.
For PNGs, use KiCad `pcb render --side top|bottom --quality basic` (high is optional).
Then run `python .\kicad\add_render_legends.py`; its dimensions are read
from the saved PCB, and the legends now correctly identify four copper layers.
After updating renders or documentation, run `python .\kicad\package_review.py`
to refresh `assembly\minibox-assembly-review.zip` and all artifact hashes.
Every PCB/part change requires the full export, not only the coordinate command.
The full exporter now also renders the saved board's front/back/isometric PNGs
and regenerates both Chinese dimension/legend images before packaging.
`render_source_board_sha256` binds that render batch to the exported PCB.
After partial rerouting, the pruning command removes only redundant GND vias,
refilling and checking each candidate's connectivity and actual I2S reference
copper. It preserves a GND return via within3 mm of every signal/power via.
Its reduction report is in `assembly\ground-stitching-review.json`; run the
full export afterwards to renew DRC, renders, CAM, placements and hashes.
