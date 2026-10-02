# Minibox KiCad carrier (provisional)

Open `minibox-carrier.kicad_pro` in KiCad 10. This PCB-only project (no
schematic) has a **120 x 112 mm, R5 rounded** outline, 2 copper layers,
routed female sockets and two wire terminals. The four **3.0 mm
non-plated** M3 holes are at (8,8),
(118,8), (8,110) and (118,110) mm in KiCad board coordinates: spacing
**110 x 102 mm**. Both copper layers exclude tracks and copper fill
within 3.5 mm of each hole center. A 3.0 mm drill is a tight fit for M3,
not the usual 3.2 mm clearance hole; confirm screw tolerance.

The ESP32 sits at upper center, physically **rotated 180 degrees (not
mirrored)** so its USB connectors face the top edge. As a result its
original left header J1 is on the right and its original right header
J2 is on the left; pin 1 of each now sits at the **bottom**. Both rows
were shifted 12 mm upward relative to the first rotated layout. Beneath
their antenna end is a **21.5 x 17 mm two-layer no-copper/no-track/no-via
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
input at the upper edge. J12 joystick and J11 keyboard pad rows are
aligned at **Y=73 mm**, just above the estimated antenna tip near Y=74.4:
J12 stays at X=12 mm, while J11 moved right to X=90 mm.
J7 moved right to X=115 mm and up to **Y=61.57 mm** (pin 1), a 10.43 mm
lift from Y=72. The switch, joystick and keyboard **F.Fab body lower
edges all align at Y=74.27 mm**; this compares physical body outlines,
not their slightly different courtyard padding. Its stock footprint
extent ends at X=120.735, leaving about 2.265 mm to the X=123 board edge.
The antenna keepout itself is unchanged, and both socket rows sit outside
its X=52–73.5 span. The microphone remains at **(50,100) mm**.
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

The routed traces use 0.35 mm signals, 0.65 mm 3V3, and 0.8 mm 5V.
`F.Cu` and `B.Cu` carry signals and power with filled GND zones.
Ground connections are routed first to keep the pours connected.
Through-hole pads and 33 standard through-vias connect the layers;
there are no blind or buried vias. To reduce sharp bends, the routing
step eases 131 right-angle corners into 45-degree transitions.
Each diagonal is extended as far as half the shorter adjoining leg allows,
then reduced if needed to clear other nets, pads, vias and keepouts.
Cuts also preserve connections to vias and tree branches inside the
original legs, not only at their endpoints.
With the layout-aware GPIO allocation, total routed copper length
decreased from the pre-remap 3114.8 mm to 2059.7 mm (33.9%);
LCD SCK decreased from 87.5 to 39.6 mm. About 258.8 mm of routing is
diagonal, with 33 vias instead of 51. Joystick X/Y traces measure
45.9/46.1 mm before the external cable. Local SPI and analog joystick
routes are prioritized before I2S, controls and power.
This is a clearance-checked routing optimization, not a
globally shortest routing solution.
Short connector-to-grid elbows are included, rather than leaving small
right angles. Pad/via anchors remain fixed; a short 45-degree dogleg eases
the outgoing leg where moving a corner would disconnect an anchor.
The router rejects remaining two-track 90-degree elbows before saving.
Electrical multiway junctions and rectangular pads are not trace elbows.
The routing step leaves silkscreen and board outline unchanged.
The two-layer design has no uninterrupted inner GND reference plane:
the PCB-only LCD SCK route is about 39.6 mm. Verify screen SPI reliability
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
0.65 mm 3V3 and 0.8 mm 5V traces; both ground zones use 0.35 mm
local clearance.
J7 pads 1/2 and J8 pad 1 have a **0.50 mm local copper clearance**
override for the external/switched 5V terminal lands. This applies on
both sides against GND fill and other networks; their own 5V traces
remain connected. The filled pad-to-GND gaps measure at least 0.50 mm.
Other pads and routing keep the existing rules, including 0.35 mm
ground-zone clearance. These overrides do not increase the GND pads'
thermal gaps or expand clearance along every 5V trace.
Board setup requires 0.5 mm copper-to-edge clearance;
ground fill starts 1 mm in from the routed outline. The 33 vias are
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
against their actual shape too. Both GND zones use thermal relief with
0.30 mm gaps and 0.35 mm spokes. J1 pad 22 uses its explicit routed GND
connection without a zone connection: the local bottom pour would otherwise
form an isolated thermal island. Existing routed ground traces remain
connected, so thermal relief improves solderability but does not completely
isolate pads thermally from the ground network. Backside pin order appears
mirrored relative to the front-view labels.

The tightened project rules report **zero KiCad DRC violations and zero
unconnected pads** after regenerating and filling the board. This checks
geometry, not parts: confirm the microphone's provisional 7.62 mm row
spacing/pin order, ESP32 row separation, screen module identity, keyboard
cable orientation, switch pin order and 3.3V regulator load before ordering.
The 3.0 mm M3 drill is nominally tight, not a clearance fit. The
[`fabrication/minibox-carrier-jlcpcb.zip`](fabrication/minibox-carrier-jlcpcb.zip)
archive contains seven Gerber layers (two copper, two mask, two silk,
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
& $cli pcb export gerbers --output $out --layers "F.Cu,B.Cu,F.Mask,B.Mask,F.SilkS,B.SilkS,Edge.Cuts" --subtract-soldermask --precision 6 --check-zones .\minibox-carrier.kicad_pcb
if ($LASTEXITCODE -ne 0) { throw "Gerber export failed" }
& $cli pcb export drill --output $out --format excellon --excellon-units mm --excellon-separate-th .\minibox-carrier.kicad_pcb
if ($LASTEXITCODE -ne 0) { throw "Drill export failed" }
Compress-Archive -Path (Join-Path $out "*") -DestinationPath .\fabrication\minibox-carrier-jlcpcb.zip -Force
```

Using a fresh temporary directory avoids accidentally including stale
four-layer Gerbers when regenerating the two-layer archive. Only the
archive is the delivered manufacturing preview.

## PNG previews

The latest front and back views, with Chinese color/material legends beside
the board, are [front](renders/minibox-carrier-front-legend.png) and
[back](renders/minibox-carrier-back-legend.png). Unannotated versions are
[front](renders/minibox-carrier-front.png) and
[back](renders/minibox-carrier-back.png). The legends are image annotations,
not PCB silkscreen, and are absent from the Gerbers. The colors distinguish
covered copper, no-copper areas, exposed metal and materials, not copper-layer
count. KiCad models show sockets/terminals rather than complete installed
modules; the custom microphone socket currently has no 3D connector model.

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

The legend helper uses Pillow and the Windows Microsoft YaHei font;
`--font` accepts another Chinese font file if needed.
Rendering to fresh files and replacing the images atomically avoids
Windows refusing to truncate a PNG that an open viewer has memory-mapped.
Check that the new output exists: KiCad may report success even after
an image-writing error. The legend helper also saves via atomic replacement.

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
peripheral modules. GPIO/SPI are 3.3V logic. LCD SCK is roughly 39.6 mm
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

This 120 x 112 mm board **does not fit** the enclosure's current 58 x 68 mm
ESP32 expansion-board space; its mounting/height and any cable access require
a new enclosure layout. This is not a replacement for the existing expansion
board. Measure all modules and revise the footprints, keepouts, edge clearance
and mechanical model before fabrication.
