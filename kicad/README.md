# Minibox KiCad carrier (provisional)

Open `minibox-carrier.kicad_pro` in KiCad 10. This is a PCB-only project (no
schematic). The PCB contains a 109 x 101 mm outline and editable, net-assigned
female sockets. It is **not routed** and
**not ready for fabrication**; unconnected-item DRC findings are expected.
Do not export Gerbers or order this board until the mating dimensions, power
system, and routing have been checked against actual parts. To regenerate
the placement on Windows, run:

```powershell
& "C:\Program Files\KiCad\10.0\bin\python.exe" .\generate_board.py
& "C:\Program Files\KiCad\10.0\bin\kicad-cli.exe" pcb drc .\minibox-carrier.kicad_pcb
```

| Socket | Module/interface | Pin 1 to last pin (viewed from component side, pin 1 at top) |
|---|---|---|
| J1 | ESP32-S3 DevKit left row | 3V3, 3V3, RST (NC), GPIO4, GPIO5, GPIO6, GPIO7, GPIO8, GPIO9, GPIO10, GPIO11, GPIO12, GPIO13, GPIO14, GPIO15, GPIO16, GPIO17, GPIO18, GPIO3, GPIO46, 5V, GND |
| J2 | ESP32-S3 DevKit right row | GND, GPIO43, GPIO44, GPIO1, GPIO2, GPIO42, GPIO41, GPIO40, GPIO39, GPIO38, GPIO37 (NC), GPIO36 (NC), GPIO35 (NC), GPIO0, GPIO45, GPIO48, GPIO47, GPIO21, GPIO20 (NC), GPIO19 (NC), GND, GND |
| J3 | INMP441 | VDD, GND, SCK, WS, SD, L/R (GND) |
| J4 | MAX98357A | LRC, BCLK, DIN, GAIN, SD, GND, VIN |
| J5 | PCF8563T | CLK (NC), INT (NC), SDA, SCL, VCC, GND |
| J6 | ST7796 **placeholder** | VCC (isolated), GND, CS, RST, DC, MOSI, SCK, BL, NC, NC |
| J7 | Three-pin switch **placeholder** | external 5V input, switched 5V load, GND |
| J8 | External 5V input | 5V, GND |
| J10 | Screen supply selection | isolated LCD VCC pad; only wire to the verified module-rated supply |

J1/J2 follow the repository's `assets/esp32s3_devkit.jpg` pinout (USB at the
bottom). ESP32 rows are drawn **25.4 mm apart**, pending measurement of the
actual development board; the exact number/spacing of male pins and the USB
clearance must be checked. J3 is a linear header while the photographed
INMP441 board is circular: its physical mating pitch and order must be
checked before using a socket. J5 follows `assets/pcf8563t_rtc.jpg`, but the
RTC's GPIO1/GPIO2 I2C assignment is **proposed**, not implemented in firmware.
J6's screen pin count, order, pitch, VCC rating, and BL input type are
unknown; this 10-pin interface is **not** an established mating socket.
Likewise, the switch's three-pin order and whether its GND is a control
return must be checked before plugging in.

The intended supply path is J8 5V -> J7 power pin -> J7 switched load ->
ESP32 5V and amplifier VIN. ESP32 3V3 powers the microphone and RTC.
Connect the speaker only to the amplifier module's own two-pin output terminal.
Never bridge 5V_IN and 5V_SW directly. Do not simultaneously inject external
5V and USB power until the development board's power-path behavior has been
verified. The screen VCC is deliberately isolated because its supply rating
is unknown. `docs/hardware-connections.md` remains the source for firmware
GPIO mappings, I2S sharing and backlight caveats.

This 109 x 101 mm board **does not fit** the enclosure's current 58 x 68 mm
ESP32 expansion-board space; its mounting/height and any cable access require
a new enclosure layout. The board is an electrical placement draft, not a
replacement for the existing expansion board. Measure all modules and revise
the footprints, keepouts, edge clearance and mechanical model before routing.
