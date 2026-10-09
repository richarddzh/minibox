# Minibox carrier v1.1 — 2026-10-09

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
| Joystick JS1 | Vertical, center (20.75,97.5); **30 mm diameter**; 0.75 mm to edge, 0.77 mm to screw keepout |
| Microphone | Center (46.5,90.25), Ø19.5 mm envelope; top Y80.5 aligned to keycaps; ≥1.22 mm to keys, ≥2.00 mm to joystick |
| Four keys | Standard 19.05 mm pitch both ways; lower row staggered 9.525 mm left |
| Keycaps | Maximum accepted envelope 18 × 18 mm; neighboring gap 1.05 mm |
| Wire entries | J8/J9 left, J7 right; external wire/tool clearance still requires a fit check |

SW1–SW4 use **HanElectricity CPG151101D13 / C49234235**; JS1 uses
**YTL YV13S-L7.85-B10Ka(60)-0-DL01 / C37323747**.
Their local footprints are traced to the exact linked drawings, not generic
MX/joystick substitutes. The joystick has a press mechanism but **it is intentionally
unused**; switch and mounting leads have no net, and **GPIO42 is free**.
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

## GPIO and Type-C

Keys → GPIO4/5/6/7 (active-low, internal pulls); RTC SDA/SCL →16/15;
MIC_SD →17; amplifier GAIN/SD_MODE →21/47. I2S WS/BCLK/DIN →39/40/41;
joystick VR1/VR2 wipers →1/2; LCD remains GPIO9–14.
Actual front silkscreen identifies each module/terminal signal and its GPIO
using digits only, for example `SDA16`, `BCLK40`, `DIN41`, and `KEY1 4`.
Power pins retain `3V3`/`5V`/`GND`; disconnected pins retain `NC`.
Labels are on `F.SilkS`, not just the non-printing fabrication layer.
See [the authoritative wiring table](../docs/hardware-connections.md).
Octal-memory GPIO35/36/37 are unused, as are USB19/20 and UART43/44.
GPIO39–41 cannot simultaneously be used for external JTAG.
Actual clone RGB firmware remains GPIO48; official v1.1 uses38, requiring
physical board identification, not an automatic firmware change.

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
Ground stitching was reduced from 62 to **23 vias**: retain return paths within
3 mm of every signal/power via and connections to separate ground-copper regions.
Do not add periodic stitching along same-layer I2S traces merely for their length;
the continuous reference plane carries their return current.

Signals/clearance **0.20/0.20 mm**, 3V3 feeders0.65 mm, 5V feeders0.80 mm;
ordinary vias **0.60/0.30 mm**. Copper-edge0.50 mm; PTH hole spacing0.45 mm;
Track-to-other-net pad clearance is **0.30 mm**, or **0.50 mm** at 5V pads.
The four keys moved right by1.60 mm together; rightmost keycap-to-edge gap0.60 mm.
These tighter mechanical edge margins require an enclosure/cap tolerance check.
silk ≥1.0 mm with0.15 mm strokes. Header holes1.0/pads1.7 mm;
terminal holes1.3/pads2.6 mm; switch holes1.5/pads2.1 mm.
Do not treat signal width or these power feeders as a 3 A qualification.
Actual factory stackup/copper thickness, temperature rise and power bottlenecks
remain review items. [Domestic JLC capability rules](../docs/jlc-pcb-design-spec.md)
distinguish limits from recommendations.

Actual latest DRC/mechanical metrics and hashes belong in
`assembly\revision-20261009\manifest.json` and `drc.json`;
an absent report or failed gate means export is incomplete.
Only a clean saved board may pass `export_revision.py`.
The exporter checks all plated-pad/via centers on all four copper CAM layers
against the saved PCB, and checks PTH/NPTH drill coordinates and diameters using
the same lower-left origin. This is digital alignment, not factory registration certification.
R1/R2 use verified UNI-ROYAL 0402WGF5101TCE/C25905, 5.1 kΩ ±1%;
domestic factory availability is not confirmed. Unconfirmed connector MPNs
leave the full assembly BOM **review-only**, even if DRC is clean.

## Current outputs versus legacy files

| Current revision | Purpose |
|---|---|
| `renders\assembly-layout.svg` | Reservation layout, not exact assembled geometry |
| `renders\minibox-carrier-front.png` / `back.png` | Genuine saved-board renders |
| `renders\minibox-carrier-isometric.png` | Genuine saved-board perspective |
| `renders\minibox-carrier-front-legend.png` / `back-legend.png` | Current Chinese material legends and board/hole dimensions |
| `assembly\revision-20261009` | DRC, manifest, SMT BOM/CPL, separate THT positions, pin map |
| `fabrication\revision-20261009` | Four-copper-layer Gerbers and separate PTH/NPTH drills |
| `fabrication\minibox-v1.1-gerber-review.zip` | PCB CAM review archive |

Old top-level `assembly` quote CSV/ZIP/PNG and `fabrication\minibox-carrier-jlcpcb.zip`
describe **v1.0 and are obsolete for this board**.
They are retained for historical records, not current submission.
No order, upload, purchase or payment is authorized by these files.

## Reproducible commands

From `C:\gitroot\minibox`, after explicitly backing up the routed board:

```powershell
& 'C:\Program Files\KiCad\10.0\bin\python.exe' .\kicad\generate_board.py --replace-routed
& 'C:\Program Files\KiCad\10.0\bin\python.exe' .\kicad\route_board.py
& 'C:\Program Files\KiCad\10.0\bin\python.exe' .\kicad\export_revision.py
```

Generation replaces routing, so its default guard refuses to overwrite tracks.
Inspect every command's result; do not continue after a failure.
For PNGs, use KiCad `pcb render --side top|bottom --quality basic` (high is optional).
Then run `python .\kicad\add_render_legends.py`; its dimensions are read
from the saved PCB, and the legends now correctly identify four copper layers.
