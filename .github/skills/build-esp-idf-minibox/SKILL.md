---
name: build-esp-idf-minibox
description: 'Build, flash, or monitor the Minibox ESP32-S3 N16R8 hardware test using standard ESP-IDF commands on Windows.'
argument-hint: 'Specify build, flash, monitor, or a COM port; the current board is COM8.'
user-invocable: true
disable-model-invocation: false
---

# Build and flash Minibox

Adapted from the `talking-alarm` build skill. Keep the workflow simple:
use standard ESP-IDF commands, without extra build scripts or Python tests.

## Project

- Directory: `esp32_idf_s3n16r8`
- Target: `esp32s3`, fixed in the top-level CMake file
- SDK: ESP-IDF 5.3.5
- Hardware: N16R8, 16 MB Quad Flash and 8 MB Octal PSRAM
- Current serial port: COM8 (CH343 USB-UART)
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
- Bootloader and partition data are flashed by `idf.py`; do not hand-code offsets.

Keep generated outputs and local logs out of Git.
The licensed prebuilt assets in `font_data` are an intentional exception:
keep them checked in. Normal builds do not need a font download or conversion.
Keep `sdkconfig.defaults` ASCII-only.
For hardware verification, release the joystick during startup calibration,
then inspect ST7796 initialization, ADC readings, raw K level and pressed state.
The current board settings invert X and use active-high K based on physical feedback.
GPIO48 should receive the black/off RGB command at startup.
Report build, flash and hardware observations separately; serial logs alone
cannot prove the visible display or all physical inputs work.
