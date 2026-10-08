# Minibox KiCad carrier (provisional)

Open `minibox-carrier.kicad_pro` in KiCad 10. This PCB-only project (no
schematic) has a **100 x 112 mm, R5 rounded** outline, 4 copper layers,
routed female sockets and two wire terminals. The four **3.0 mm
non-plated** M3 holes are at (10,8),
(100,8), (10,110) and (100,110) mm in KiCad board coordinates: spacing
**90 x 102 mm**, measured center to center. All four copper layers exclude tracks and copper fill
within 3.5 mm of each hole center. A 3.0 mm drill is a tight fit for M3,
not the usual 3.2 mm clearance hole; confirm screw tolerance.

The ESP32 sits at upper center, physically **rotated 180 degrees (not
mirrored)** so its USB connectors face the top edge. As a result its
original left header J1 is on the right and its original right header
J2 is on the left; pin 1 of each now sits at the **bottom**. Both rows
were shifted 12 mm upward relative to the first rotated layout. Beneath
their antenna end is a **21.5 x 17 mm all-four-layer no-copper/no-track/no-via
area** at X=52–73.5, Y=67–84 mm (also outlined on `Dwgs.User`).
The microphone was moved near the lower edge to avoid this area. The supplied
[`ESP32 dimensions image`](../assets/esp32s3_devkit_01.jpg) shows a
57.15 mm PCB body and 63.611 mm overall length including the antenna
(~6.46 mm overhang); the antenna pattern is about 18 mm across based
on the image's scaled 25.4 mm pin-row spacing. Relative to the nearest
header pad at Y=66, the antenna tip is approximately Y=74.4.
The keepout continues to Y=84, roughly 9.6 mm past that estimate.
These image-derived dimensions are **not a mechanical drawing**: verify
the physical antenna location and required RF clearance before
manufacture. Do not put a metal enclosure, cable or another module
over the antenna. The RTC and
amplifier sit left of it, the display cable on the far right, the 5V
input beside the RTC at the upper left. J12 joystick and J11 keyboard pad rows are
aligned at **Y=73 mm**, just above the estimated antenna tip near Y=74.4:
J12 stays at X=12 mm, while J11 is at X=82 mm.
J6 LCD is at **(96,14) mm** and J7 switch is at **(96,55.5) mm**
(pin 1). The right-side interfaces now stack LCD, switch and horizontal
keyboard from top to bottom. The switch's F.Fab body has **4.67 mm**
clearance below the LCD socket and **3.53 mm** above the keyboard socket.
The joystick and keyboard **F.Fab body lower edges remain at Y=74.27 mm**;
the switch lower edge is raised to **Y=68.2 mm**, not moved downward.
These compare physical body outlines, not courtyard padding.
The switch's stock footprint extent ends at X=101.735, leaving about
3.265 mm to the X=105 board edge. Its wire-entry side faces right:
verify cable bend and screwdriver access with the actual terminal.
The antenna keepout itself is unchanged, and both socket rows sit outside
its X=52–73.5 span. The microphone remains at **(50,100) mm**.
This compact revision reduces width from 120 to 100 mm (16.7%) without
increasing the 112 mm height. J1/J2, J3 and J4 retain their exact pad
positions and orientations, preserving microphone/ESP32 and
amplifier/ESP32 spacing. Relative to the preceding 108 mm-wide version,
the left edge moves from X=3 to X=5 and the right edge from X=111 to X=105.
The amplifier module's reserved left extent at X=7 retains **2 mm**
to the left edge; its exact body must still be measured.
J6 moves 4 mm left and 2 mm up; J7 moves 6 mm left and 6.07 mm up.
J11 remains at (82,73), still horizontal.
J8 moved from (97,12) to **(28,11) mm** beside J5 (16,12).
Connector orientations, net assignments and firmware GPIOs are unchanged.
The left mounting holes move 2 mm right and the right holes 6 mm left
relative to the 108 mm-wide version. Update enclosures or mounting plates
made for either the preceding 98 mm or original 110 mm horizontal spacing.
The microphone's estimated 19.5 mm
courtyard begins at Y=92.79 mm; all other connectors and components
sit above that line, except for the four mounting holes. J11 and J12 are
horizontal 1x5 sockets, numbered **left to right** on the component side.
Every connector position, including electrically unused positions, has a
silkscreen pin name. Connected module signal pins also show their ESP32
GPIO number, derived from the DevKit pin mapping; power and ground labels
remain unchanged, and unused signals are marked NC. J3 L/R is labeled GND
because it is strapped to ground, not a GPIO.
J3's circular courtyard and J4's module outline
reserve their estimated body footprints; the RTC stands vertically and
the screen/joystick/keyboard connect by cable. The J4 body reservation
is X=7–34 mm; against the image-derived DevKit left edge near X=48.13 mm
this leaves approximately 14 mm of horizontal separation, pending
physical measurement. **Only J7 and J8** use
5.08 mm screw terminals for the switch and external 5V wires; all
module headers remain 2.54 mm female sockets.

## Four-layer stackup and I2S

The nominal 1.6 mm stackup is:

| Layer | Function |
|---|---|
| L1 / F.Cu | Signals, all four I2S networks, and GND fill |
| L2 / In1.Cu | Single connected GND reference plane; no tracks |
| L3 / In2.Cu | Power: 3V3 plane with separate 5V_IN and 5V_SW corridor zones and feeders |
| L4 / B.Cu | Other signals and GND fill; no power tracks |

The saved stackup specifies 0.035 mm copper on each layer, 0.18 mm
outer-to-inner prepregs, a 1.08 mm inner core and 0.01 mm masks on each
side, totaling 1.6 mm. These are **nominal design values, not a
factory-confirmed stackup**; have the manufacturer confirm the finished
thickness, dielectric properties and copper thickness before ordering.
This is not an impedance-controlled release.

The routed traces use 0.35 mm signals, 0.65 mm 3V3 feeders and 0.8 mm
5V feeders. Power tracks are confined to In2.Cu; the 5V corridor zone
outlines extend 0.7 mm beyond the routed copper before filling and
clearance trimming. Different power nets are not joined.
All GND connector pads connect to the internal plane with thermal relief.
The antenna and screw-area keepouts apply to **all four copper layers**.
The reference plane is continuous under I2S outside the necessary
connector antipads; it is intentionally absent from the antenna keepout.

`AUDIO_DIN`, `I2S_BCLK`, `I2S_WS` and `MIC_SD` are routed first,
entirely on F.Cu with **zero I2S vias**, eliminating the old long
top/bottom BCLK/WS overlap. Their routed network lengths are
42.4, 105.0, 129.8 and 78.0 mm respectively; shared-clock lengths include
both module branches, not a single source-to-load distance.
WS is longer than the previous two-layer route to keep it on the same
reference layer without crossings. The router checks the actual filled
In1.Cu plane at intervals no greater than 0.1 mm along each I2S centerline,
excluding only the same-net connector antipad envelopes, and rejects gaps.
Plane continuity and DRC do not replace an electrical signal-integrity test.

There are **11 signal vias and 28 GND stitching vias**, all ordinary
through-vias, with no blind or buried vias. Each signal via has a GND
stitch within 3 mm; additional stitches are placed alongside long I2S legs.
The bottom layer's adjacent layer is split power, not a uniform GND
plane, so future fast signals should also prefer F.Cu/In1.Cu.
Total routed track length is **1507.9 mm**, excluding zone copper.
To reduce sharp bends, the routing step eases 105 right-angle corners
into 45-degree transitions.
Each ordinary corner starts with a cut up to three quarters of the shorter
adjoining leg, retaining a connecting leg rather than consuming it entirely.
Cuts are reduced in at most 0.25 mm steps to clear other nets, pads,
vias and keepouts; very short cuts are reduced proportionally.
Repeated passes handle corners exposed by adjoining changes.
Cuts also preserve connections to vias and tree branches inside the
original legs, not only at their endpoints.
After I2S, the router prioritizes local SPI, external 5V input,
joystick, RTC, amplifier controls, buttons and the remaining power nets.
The via cost is 75 to discourage unnecessary layer changes.
The router rejects more than 32 signal vias or more than 1975.819 mm
of routed track length; GND stitching is added separately.
This is a clearance-checked layout, not a globally shortest solution.
Short connector-to-grid elbows are included, rather than leaving small
right angles. Pad/via anchors remain fixed; a short 45-degree dogleg eases
the outgoing leg where moving a corner would disconnect an anchor.
The router rejects remaining two-track 90-degree elbows before saving.
Electrical multiway junctions and rectangular pads are not trace elbows.
The routing step leaves silkscreen and board outline unchanged.
Verify screen SPI reliability
at the configured clock and with the actual cable, and lower the clock
if required.
**KiCad DRC has no violations or unconnected pads**, but DRC cannot
verify signal integrity at the screen's configured SPI speed, or real
module dimensions, polarity or regulator current.
Do not order this PCB until those measurements and power-path checks are
complete. To regenerate and check it on Windows from this directory, run:

```powershell
& "C:\Program Files\KiCad\10.0\bin\python.exe" .\generate_board.py
& "C:\Program Files\KiCad\10.0\bin\python.exe" .\route_board.py
& "C:\Program Files\KiCad\10.0\bin\kicad-cli.exe" pcb drc --refill-zones --severity-all --exit-code-violations .\minibox-carrier.kicad_pcb
```

## Fabrication rule check

The [EasyEDA Pro design-rule guide](https://prodocs.lceda.cn/cn/pcb/design-design-rule/)
explains how to configure and run DRC; it is **not** the PCB factory's
numeric capability table. Against the separate
[JLCPCB manufacturing requirements](https://www.jlc.com/portal/1/serviceGuide),
this KiCad project explicitly checks 0.30 mm minimum copper clearance,
0.45 mm drill-to-drill clearance, and at least 1.0 mm-high/0.15 mm-stroke
silkscreen with 0.15 mm clearance. The saved copper uses 0.35 mm signal,
0.65 mm 3V3 and 0.8 mm 5V traces; all copper zones use 0.35 mm
local clearance.
J7 pads 1/2 and J8 pad 1 have a **0.50 mm local copper clearance**
override for the external/switched 5V terminal lands. This applies on
all copper layers against GND fill and other networks; their own 5V traces
remain connected. The filled pad-to-GND gaps measure at least 0.50 mm.
Other pads and routing keep the existing rules, including 0.35 mm
zone clearance. These overrides do not increase the GND pads'
thermal gaps or expand clearance along every 5V trace.
Board setup requires 0.5 mm copper-to-edge clearance;
plane and ground fill outlines start 1 mm in from the routed outline. The 39 vias are
ordinary **0.8 mm pad / 0.4 mm drill through-vias** (0.2 mm annular
ring). All 87 socket holes are 1.0 mm plated drills with 1.7 mm pads
(0.35 mm annular ring); J7/J8 have five 1.3 mm plated terminal holes
with 2.6 mm pads (0.65 mm annular ring).
The four M3 holes are 3.0 mm NPTH, with 3.5 mm
radius copper keepouts. Solder-mask openings expand 0.05 mm per side
from exposed pads (0.1 mm overall), with a 0.1 mm minimum mask web.
The nominal board thickness is 1.6 mm. Visible front-side text measures
at least 1.0 mm high with 0.15 mm strokes. All silk (including footprint
graphics) is outside a 3.5 mm radius from each of the four mounting-hole
centers, verified conservatively against each object's bounding box.

All ten connector footprints are on the front side, with plated through-hole
pads, copper lands and solder-mask openings on both sides. Insert female
headers and terminal pins from the front and solder from the back; do not
flip the footprints. The socket holes are suitable for typical 0.64 mm
square leads (about 0.91 mm diagonal), but confirm the chosen leads and
finished-hole tolerances. Check the terminals' approximately 0.9 mm leads
against their actual shape too. All three GND zones use thermal relief with
0.30 mm gaps and 0.35 mm spokes. J1 pad 22 and J5 pad 6 now connect
to the internal GND plane rather than relying on explicit ground tracks;
all GND pads retain thermal relief. Backside pin order appears
mirrored relative to the front-view labels.

The tightened project rules report **zero KiCad DRC violations and zero
unconnected pads** after regenerating and filling the board. This checks
geometry, not parts: confirm the microphone's provisional 7.62 mm row
spacing/pin order, ESP32 row separation, screen module identity, keyboard
cable orientation, switch pin order and 3.3V regulator load before ordering.
The 3.0 mm M3 drill is nominally tight, not a clearance fit. The
[`fabrication/minibox-carrier-jlcpcb.zip`](fabrication/minibox-carrier-jlcpcb.zip)
archive contains nine Gerber layers (four copper, two mask, two silk,
one outline), a Gerber job file, and separate Excellon plated/non-plated
drills in millimetres. It is a **preview candidate, not fabrication
approval**: check the physical module fit and power path above, then
upload the archive to JLCPCB and inspect its DFM/manufacturing preview
before placing an order. KiCad DRC cannot verify those real-world details.

To regenerate the upload archive from the saved board after a design change:

```powershell
$cli = "C:\Program Files\KiCad\10.0\bin\kicad-cli.exe"
$out = Join-Path $env:TEMP ("minibox-gerber-" + [guid]::NewGuid())
New-Item -ItemType Directory -Path $out | Out-Null
& $cli pcb drc --refill-zones --severity-all --exit-code-violations .\minibox-carrier.kicad_pcb
if ($LASTEXITCODE -ne 0) { throw "PCB DRC failed" }
& $cli pcb export gerbers --output $out --layers "F.Cu,In1.Cu,In2.Cu,B.Cu,F.Mask,B.Mask,F.SilkS,B.SilkS,Edge.Cuts" --subtract-soldermask --precision 6 --check-zones .\minibox-carrier.kicad_pcb
if ($LASTEXITCODE -ne 0) { throw "Gerber export failed" }
& $cli pcb export drill --output $out --format excellon --excellon-units mm --excellon-separate-th .\minibox-carrier.kicad_pcb
if ($LASTEXITCODE -ne 0) { throw "Drill export failed" }
Compress-Archive -Path (Join-Path $out "*") -DestinationPath .\fabrication\minibox-carrier-jlcpcb.zip -Force
```

Using a fresh temporary directory avoids accidentally including stale
Gerbers from an earlier layer count or revision. Only the
archive is the delivered manufacturing preview.

## PNG previews

The latest front and back views, with Chinese color/material legends and dimensions beside
the board, are [front](renders/minibox-carrier-front-legend.png) and
[back](renders/minibox-carrier-back-legend.png). Unannotated versions are
[front](renders/minibox-carrier-front.png) and
[back](renders/minibox-carrier-back.png). Blue dimension leaders label the
100 x 112 mm outline, all four 3.0 mm mounting holes and their
90 x 102 mm center spacing, read directly from the saved board.
Their pixel placement is illustrative, not a manufacturing drawing.
The legends and blue leaders are image annotations, not PCB silkscreen,
and are absent from the Gerbers. The colors distinguish
covered copper, no-copper areas, exposed metal and materials, not copper-layer
count. KiCad models show sockets/terminals rather than complete installed
modules. J3 now includes two standard 1x3 female-socket models,
one on each three-pad strip, at the provisional 7.62 mm separation.

The actual front silkscreen, included in the Gerbers, carries
**PangMiaoMiao MiniBox**, **v1.0**, **2026-10-03** and a compact mechanical
table: `BOARD 100x112`, `PITCH 90x102` (hole centers), `HOLES 4xD3.0`,
all in millimetres. A separate `SOCKETS P2.54` list is generated from
the actual socket placements, counting J3 as two separate strips:

| Female socket, 2.54 mm pitch | Quantity per board |
|---|---|
| 1x22 | 2 |
| 1x14 | 1 |
| 1x7 | 1 |
| 1x6 | 1 |
| 1x5 | 2 |
| 1x3 | 2 |

These are nine female strips / 87 solder pins. J7/J8 are separate
three-/two-position 5.08 mm screw terminals, not female sockets;
all connectors together have 92 solder pins. Socket height and exact
part numbers still need confirmation.

Regenerate these after changing the saved board (from this directory):

```powershell
$cli = "C:\Program Files\KiCad\10.0\bin\kicad-cli.exe"
$front = Join-Path $env:TEMP ("minibox-front-" + [guid]::NewGuid() + ".png")
$back = Join-Path $env:TEMP ("minibox-back-" + [guid]::NewGuid() + ".png")
& $cli pcb render --side top --width 2200 --height 2200 --quality high --background opaque --output $front .\minibox-carrier.kicad_pcb
if ($LASTEXITCODE -ne 0 -or !(Test-Path $front)) { throw "Front render failed" }
python -c "from pathlib import Path; from PIL import Image; im=Image.open(r'$front'); im.verify(); im.close(); Path(r'$front').replace(Path(r'.\renders\minibox-carrier-front.png'))"
if ($LASTEXITCODE -ne 0) { throw "Front PNG replacement failed" }
& $cli pcb render --side bottom --width 2200 --height 2200 --quality high --background opaque --output $back .\minibox-carrier.kicad_pcb
if ($LASTEXITCODE -ne 0 -or !(Test-Path $back)) { throw "Back render failed" }
python -c "from pathlib import Path; from PIL import Image; im=Image.open(r'$back'); im.verify(); im.close(); Path(r'$back').replace(Path(r'.\renders\minibox-carrier-back.png'))"
if ($LASTEXITCODE -ne 0) { throw "Back PNG replacement failed" }
python .\add_render_legends.py
if ($LASTEXITCODE -ne 0) { throw "Legend generation failed" }
```

The legend helper uses Pillow, KiCad's bundled Python for board geometry,
and the Windows Microsoft YaHei font. `--font` accepts another Chinese
font file; `--kicad-python` accepts another KiCad Python executable.
Rendering to fresh files and replacing the images atomically avoids
Windows refusing to truncate a PNG that an open viewer has memory-mapped.
Check that the new output exists: KiCad may report success even after
an image-writing error. The legend helper also saves via atomic replacement.

## Connector assembly quotation

[assembly/README.md](assembly/README.md) contains a supplier-facing inquiry
for the minimum accepted batch, **no more than five PCBs**, including both
5.08 mm screw terminals. The folder includes a draft BOM, placement
coordinates, a 92-pin mapping and a front assembly diagram. J3 is split
into physical J3A/J3B strips in those files, not changed on the PCB.
These are **quotation-only, not production-approved**: all eight exact
part selections, factory coordinate/rotation conventions, physical fit,
pure-DIP batch acceptance and pricing remain to be confirmed.

| Connector | Module/interface | Pin 1 to last pin (J8 left to right; J1/J2 bottom to top; other rows top to bottom; J11/J12 left to right) |
|---|---|---|
| J1 | ESP32-S3 DevKit left row | 3V3, 3V3, RST (NC), GPIO4, GPIO5, GPIO6, GPIO7, GPIO15, GPIO16, GPIO17, GPIO18, GPIO8, GPIO3, GPIO46, GPIO9, GPIO10, GPIO11, GPIO12, GPIO13, GPIO14, 5V, GND |
| J2 | ESP32-S3 DevKit right row | GND, GPIO43, GPIO44, GPIO1, GPIO2, GPIO42, GPIO41, GPIO40, GPIO39, GPIO38, GPIO37 (NC), GPIO36 (NC), GPIO35 (NC), GPIO0, GPIO45, GPIO48, GPIO47, GPIO21, GPIO20 (NC), GPIO19 (NC), GND, GND |
| J3 | INMP441 two-row socket | 1 VDD, 2 GND / 3 SCK (GPIO40), 4 WS (GPIO39) / 5 SD (GPIO15), 6 L/R (GND); pin numbers alternate by column |
| J4 | MAX98357A | LRC (GPIO39), BCLK (GPIO40), DIN (GPIO41), GAIN (GPIO8), SD (GPIO7), GND, VIN |
| J5 | PCF8563T | CLK (NC), INT (NC), SDA (GPIO47), SCL (GPIO21), VCC, GND |
| J6 | ST7796 screen header | VCC (3V3), GND, CS, RESET, DC/RS, SDI/MOSI, SCK, LED, SDO/MISO (NC), T_CLK (NC), T_CS (NC), T_DIN (NC), T_DO (NC), T_IRQ (NC) |
| J7 | 3-position 5.08 mm switch screw terminal | external 5V input, switched 5V load, GND |
| J8 | 2-position 5.08 mm external-power screw terminal | 5V, GND |
| J11 | Three-key keyboard cable | KeyA (GPIO4), KeyB (GPIO5), KeyC (GPIO6), Vcc (3V3), Gnd |
| J12 | Joystick cable | G, V (3V3), X (GPIO1), Y (GPIO2), K (GPIO42) |

GPIO assignments follow the
[official ESP32-S3-DevKitC-1 v1.1 header table](https://docs.espressif.com/projects/esp-dev-kits/zh_CN/latest/esp32s3/esp32-s3-devkitc-1/user_guide_v1.1.html#user-guide-s3-devkitc-1-v1-1-header-blocks).
GPIO40/41 have no ADC: they cannot replace the joystick's analog X/Y.
ADC1 GPIO1/2 are on the physical left header, along with the amplifier's
GPIO39/40/41 I2S signals and RTC GPIO47/21; keypad GPIO4/5/6 and display
GPIO9–14 are on the right. Amplifier enable/gain remain on GPIO7/8.
GPIO0/3/45/46 boot straps, GPIO19/20 USB, GPIO43/44 UART0, GPIO35/36/37
Octal memory and GPIO38/48 RGB candidates have no external module loads.
GPIO16/17/18 remain available as unused header positions.
This remap changes existing hardware wiring: use the matching updated
firmware and carrier, not the old audio/keyboard/joystick/RTC wiring.

J1/J2 follow the repository's `assets/esp32s3_devkit.jpg` pinout (pictured USB
at bottom), then both rows are rotated 180 degrees on the carrier so USB
faces up, without mirroring their pad order. ESP32 rows are drawn
**25.4 mm apart**, pending measurement of the
actual development board; the exact number/spacing of male pins and the USB
clearance must be checked. J3 uses the project-local
`Minibox:INMP441_2x03_Row7.62mm` footprint: 2.54 mm along each three-pin
row, **provisional 7.62 mm between rows**. Its circular courtyard is
19.5 mm in diameter. The photo shows an obvious gap between rows, not
the standard 2.54 mm spacing: **measure the real row spacing and match all
six labeled pins before insertion**. The labels in the photograph do not
establish the pad numbering used by this carrier. Assemble J3 from two
1x3 female strips at the measured separation, not a standard 2x3 socket.
J4 has a provisional
27 x 24 mm module-body envelope on `Dwgs.User`,
including the amplifier's speaker terminal; leave this space unobstructed.
J5 follows `assets/pcf8563t_rtc.jpg` and mounts upright, but the
RTC's SDA=GPIO47/SCL=GPIO21 constants are reserved in firmware,
but the current application has no RTC driver or initialization.
J7/J8 use stock KiCad Phoenix 5.08 mm horizontal screw-terminal
footprints. The supplied [`terminal image`](../assets/crimp_terminal.jpg)
shows 5.08 mm pitch, approximately 0.9 mm pins, and 15 / 9.9 mm
three-/two-way housing widths. The footprints use 1.3 mm plated drills;
their housing outlines are *not* guaranteed to match the pictured
generic terminals. Check the exact parts' lead pattern, housing
clearances, pin insertion direction and wire access before ordering.
J11 and J12 are cabled interfaces; see `assets/keypad.jpg` and
`assets/joystick.jpg`. The keypad photo as viewed from its button face appears
to label its pins `+ - C B A` from left to right, while the owner supplied
`KeyA KeyB KeyC Vcc Gnd` as the carrier's cable-end order. **Do not use a
straight-through cable without confirming the plug's view/orientation and
continuity; re-order the cable conductors if necessary.** The joystick photo
shows `G V X Y K` at its connector. The joystick photo
has a keyed cable connector, so the 2.54 mm socket on the carrier needs a
matching cable adapter with male ends; it cannot directly mate to that
connector. J11 likewise needs a cable with male ends to mate to its female
socket. The
remote module bodies need no carrier-board footprint. Joystick
GPIO1/2/42 support is present in legacy firmware but is not currently
initialized. Unused pins remain as unconnected socket positions; do not
bridge them merely to make a DRC report smaller.
The owner confirmed the screen header follows `assets/tft_spi.jpg`:
14 pins in the order shown above. The photo depicts a **2.8-inch 240 x 320**
module, whereas the current firmware and mechanical model describe a
**4-inch 480 x 320 ST7796** module. Thus header order is confirmed by the
owner, but screen controller, display geometry, header pitch/placement,
VCC rating, and whether LED is a logic input still require verification
against the actual 4-inch module. Touch and SDO remain unconnected because
the current firmware does not use them.
Likewise, the switch's three-pin order and whether its GND is a control
return must be checked before plugging in.

The supply path is upper-edge J8 +5V -> right-side J7 switch power input;
J8 GND -> J7 switch
GND; J7 switched load -> ESP32 J1 5V and amplifier J4 VIN. The development
board's onboard regulator (not the ESP32-S3 chip) produces 3V3, which
powers the microphone, RTC, screen, keyboard and joystick. No screen pin
is connected to 5V. The amplifier alone uses switched 5V among the
peripheral modules. GPIO/SPI are 3.3V logic. LCD SCK is roughly 21.9 mm
on this PCB before accounting for the cable; test the real screen at a
reduced SPI clock if 40 MHz proves unreliable, rather than assuming DRC
guarantees signal integrity.
Connect the speaker only to the amplifier module's own two-pin output terminal.
Never bridge 5V_IN and 5V_SW directly. Do not simultaneously inject external
5V and USB power until the development board's power-path behavior has been
verified. Check that the actual screen accepts a 3.3V supply, its LED pin is
a 3.3V logic input, and the development board's 3.3V regulator supports the
combined display, microphone and RTC current. `docs/hardware-connections.md`
remains the source for firmware
GPIO mappings, I2S sharing and backlight caveats.

This 100 x 112 mm board **does not fit** the enclosure's current 58 x 68 mm
ESP32 expansion-board space; its mounting/height and any cable access require
a new enclosure layout. This is not a replacement for the existing expansion
board. Measure all modules and revise the footprints, keepouts, edge clearance
and mechanical model before fabrication.
