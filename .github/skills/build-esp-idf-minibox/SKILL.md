---
name: build-esp-idf-minibox
description: 'Build, flash, or monitor the Minibox ESP32-S3 N16R8 hardware test using standard ESP-IDF commands on Windows.'
argument-hint: 'Specify build, flash, monitor, or a COM port; the current board is COM8.'
user-invocable: true
disable-model-invocation: false
---

# Build and flash Minibox

Default firmware now runs the LVGL chat alarm, radio, settings and games.
NES uses unscaled centered 256x240 video and bilingual titles for known English
ROM filenames. Mapper support includes 112, 178 and 245; see the firmware README
for compatibility limits. Menus use four directions; NES diagonals require both
axes >=40% and minor/major >=85%, with neutral isolation between active regions.
The original hardware test is selected with `CONFIG_MINIBOX_HARDWARE_TEST`.
Hardware and wiring remain unchanged. Normal app credentials are local ignored
`config_data/wifi_config.txt` (SSID, password) and `config_data/app_secrets.txt`
(ASR, Agent, TTS keys), packed into a separate `storage` SPIFFS partition.
API endpoints remain hardcoded. Do not print, commit or share private files/images.
Full `idf.py flash` also overwrites saved device settings with `storage.bin`;
`idf.py app-flash` updates only the application, after explicit flash authorization.

Adapted from the `talking-alarm` build skill. Keep the workflow simple:
use standard ESP-IDF commands, without extra build scripts or Python tests.

## Project

- Directory: `esp32_idf_s3n16r8`
- Target: `esp32s3`, fixed in the top-level CMake file
- SDK: ESP-IDF 5.3.5
- Hardware: N16R8, 16 MB Quad Flash and 8 MB Octal PSRAM
- Current serial port: COM8 (CH343 USB-UART)
- LCD: official `espressif/esp_lcd_st7796` 1.4.0, pinned in
  `main/idf_component.yml` and `dependencies.lock`; no Arduino dependency
- Wiring: [hardware-connections.md](../../../docs/hardware-connections.md)

## Environment

Prefer an already initialized ESP-IDF 5.3.5 PowerShell terminal.
Do not assume a plain PowerShell has `idf.py`, CMake or the toolchain on PATH.

The installations used on this computer are:

- SDK: `C:\esp\v5.3.5\esp-idf`
- Tools: `C:\Espressif`
- SDK Python: `C:\Espressif\tools\python\v5.3.5\venv\Scripts\python.exe`

If initialization is needed, use the installed SDK's official `export.ps1`
or `tools\idf_tools.py export` with the matching tools/Python environment.
Do not blindly invoke the old reference repository's missing
`Initialize-Idf.ps1` or hard-coded installation ID.
Keep environment setup and the requested command in the same shell process.

## Commands

From the repository root:

```powershell
Set-Location .\esp32_idf_s3n16r8
idf.py build
```

Only when flashing is explicitly requested:

```powershell
idf.py -p COM8 flash
```

For an explicitly authorized storage-only update, use
`$env:ESPPORT='COM8'; $env:ESPBAUD='460800'; idf.py storage-flash`.
This custom target may ignore `idf.py -p`; `ESPPORT` prevents auto-selecting
another serial device. It replaces all storage files, including settings and saves.

To monitor an already flashed board:

```powershell
idf.py -p COM8 monitor
```

Exit the monitor with `Ctrl+]` and release the serial port.
Do not flash a different device when COM8 is absent or ambiguous.
Avoid `fullclean`, deleting the build directory, or rerunning `set-target`
for routine changes. Use `idf.py reconfigure` only when configuration needs it.

## Outputs and checks

- `build\minibox_hardware_test.bin`
- `build\minibox_hardware_test.elf`
- `build\fonts.bin` (SPIFFS image; included automatically in `idf.py flash`)
- `build\storage.bin` (private SPIFFS configuration; included in full flash)
- Bootloader and partition data are flashed by `idf.py`; do not hand-code offsets.

Keep generated outputs and local logs out of Git.
The licensed prebuilt assets in `font_data` are an intentional exception:
keep them checked in. Normal builds do not need a font download or conversion.
The first build needs network access to resolve the official LCD component;
later builds reuse the component manager cache.
Keep `sdkconfig.defaults` ASCII-only.
In hardware-test mode, inspect ST7796 initialization, RGB color bars and the displayed
GPIO4/5/6/7 button states. Keep the joystick centered during the approximately
640 ms startup calibration; observe GPIO1/2 raw ADC values and percentages.
PCF8563 uses I2C address 0x51 on SDA15/SCL16; verify advancing seconds and
`TICK OK`. Only a stopped/voltage-low RTC is initialized to local build time.
Startup plays a quiet 250 ms tone, then records and plays 1 second of microphone audio.
GPIO5 starts microphone recording while held
(maximum 3 seconds) and plays the recording after release. Buttons default
to active-low with internal pull-ups; use menuconfig for active-high modules.
GPIO48 should receive the black/off RGB command at startup.
Report build, flash and hardware observations separately; serial logs alone
cannot prove the visible display or all physical inputs work.

For the optional `MINIBOX_RADIO_TEST` mode, configure Wi-Fi only in the ignored
local `sdkconfig`. It plays Donggan 101 only and disables startup/manual microphone
tests to avoid I2S clock conflicts. Verify IP acquisition, network time sync,
HTTP 200, MP3 format and increasing I2S output; confirm audible sound separately.
Never commit credentials or share firmware binaries containing them.
