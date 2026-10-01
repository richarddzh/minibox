# Minibox KiCad carrier (provisional)

Open `minibox-carrier.kicad_pro` in KiCad 10. This PCB-only project (no
schematic) has a **120 x 112 mm** outline, 4 copper layers and routed
female sockets. The four **3.0 mm non-plated** M3 holes are at (8,8),
(118,8), (8,110) and (118,110) mm in KiCad board coordinates: spacing
**110 x 102 mm**. All four copper layers exclude tracks and copper fill
within 3.5 mm of each hole center. A 3.0 mm drill is a tight fit for M3,
not the usual 3.2 mm clearance hole; confirm screw tolerance.

The layout places the ESP32 at upper left; the RTC, amplifier and 5V input
at upper right; the display cable at the far right; and the joystick,
microphone, keyboard and switch along the lower edge. J11 and J12 are
horizontal 1x5 sockets, numbered **left to right** on the component side.
Every socket position, including electrically unused positions, has a
silkscreen pin name. `TP1` is a small exposed **3V3 measurement pad**, not
an extra power input. J3's circular courtyard and J4's module outline
reserve their estimated body footprints; the RTC stands vertically and
the screen/joystick/keyboard/switch connect by cable.

The routed traces use 0.35 mm signals, 0.65 mm 3V3, and 0.8 mm 5V.
`In1.Cu` is a ground plane, with additional ground fills on front and
back; `In2.Cu` carries routed signals. Through-hole pads and vias connect
the layers. **KiCad DRC has no violations or unconnected pads**, but
DRC cannot verify real module dimensions, polarity or regulator current.
An isolated two-layer routing trial with the same placement failed to
connect the amplifier GAIN pin; a different placement/manual route might
still work, but would need fresh DRC and signal-integrity checks. Four
layers also preserve the continuous inner ground reference for the
approximately 149 mm LCD SCK run and its external cable.
Do not order this PCB until those measurements and power-path checks are
complete. To regenerate and check it on Windows from this directory, run:

```powershell
& "C:\Program Files\KiCad\10.0\bin\python.exe" .\generate_board.py
& "C:\Program Files\KiCad\10.0\bin\python.exe" .\route_board.py
& "C:\Program Files\KiCad\10.0\bin\kicad-cli.exe" pcb drc --refill-zones --save-board .\minibox-carrier.kicad_pcb
```

| Socket | Module/interface | Pin 1 to last pin (J11/J12 left to right; other rows top to bottom) |
|---|---|---|
| J1 | ESP32-S3 DevKit left row | 3V3, 3V3, RST (NC), GPIO4, GPIO5, GPIO6, GPIO7, GPIO15, GPIO16, GPIO17, GPIO18, GPIO8, GPIO3, GPIO46, GPIO9, GPIO10, GPIO11, GPIO12, GPIO13, GPIO14, 5V, GND |
| J2 | ESP32-S3 DevKit right row | GND, GPIO43, GPIO44, GPIO1, GPIO2, GPIO42, GPIO41, GPIO40, GPIO39, GPIO38, GPIO37 (NC), GPIO36 (NC), GPIO35 (NC), GPIO0, GPIO45, GPIO48, GPIO47, GPIO21, GPIO20 (NC), GPIO19 (NC), GND, GND |
| J3 | INMP441 two-row socket | 1 VDD, 2 GND / 3 SCK, 4 WS / 5 SD, 6 L/R (GND); pin numbers alternate by column |
| J4 | MAX98357A | LRC, BCLK, DIN, GAIN, SD, GND, VIN |
| J5 | PCF8563T | CLK (NC), INT (NC), SDA, SCL, VCC, GND |
| J6 | ST7796 screen header | VCC (3V3), GND, CS, RESET, DC/RS, SDI/MOSI, SCK, LED, SDO/MISO (NC), T_CLK (NC), T_CS (NC), T_DIN (NC), T_DO (NC), T_IRQ (NC) |
| J7 | Three-pin switch **placeholder** | external 5V input, switched 5V load, GND |
| J8 | External 5V input | 5V, GND |
| J11 | Three-key keyboard cable | KeyA (GPIO40), KeyB (GPIO41), KeyC (GPIO42), Vcc (3V3), Gnd |
| J12 | Joystick cable | G, V (3V3), X (GPIO4), Y (GPIO5), K (GPIO6) |
| TP1 | 3.3 V probe pad | 3V3 |

J1/J2 follow the repository's `assets/esp32s3_devkit.jpg` pinout (USB at the
bottom). ESP32 rows are drawn **25.4 mm apart**, pending measurement of the
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
RTC's GPIO1/GPIO2 I2C assignment is **proposed**, not implemented in firmware.
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
GPIO4/5/6 support is present in legacy firmware but is not currently
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

The supply path is upper-edge J8 +5V -> lower-edge J7 switch power input;
J8 GND -> J7 switch
GND; J7 switched load -> ESP32 J1 5V and amplifier J4 VIN. The development
board's onboard regulator (not the ESP32-S3 chip) produces 3V3, which
powers the microphone, RTC, screen, keyboard and joystick. No screen pin
is connected to 5V. The amplifier alone uses switched 5V among the
peripheral modules. GPIO/SPI are 3.3V logic. LCD SCK is roughly 149 mm
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
