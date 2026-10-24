# Nofrendo for Minibox

The CPU, PPU, APU, bitmap and mapper sources are vendored from
[espressif/esp32-nesemu](https://github.com/espressif/esp32-nesemu),
revision `693e378` (local reference `C:\gitroot\esp32-nesemu`).
Mappers 0, 1, 2, 3, 4, 7, 66, 112, 178 and 245 are compiled. No ROMs, old ESP32
LCD/controller drivers, Arduino dependency or external checkout is required.

[Shim06/Anemoia-ESP32](https://github.com/Shim06/Anemoia-ESP32), revision
`7cdf281`, was also reviewed from `C:\gitroot\Anemoia-ESP32`:
`Anemoia-ESP32.ino` supplies the frame-deadline, held-controller and
Start+Select interaction reference; its README documents alternate-frame
output for 40 MHz SPI displays; `src/nes.cpp` and `src/core/cartridge.cpp`
demonstrate identifying saves by ROM CRC32 rather than display filename.
Minibox implements these ideas with its existing C/FreeRTOS, SPIFFS and LVGL
interfaces. No Anemoia GPLv3 code was copied into this component.
Its SD bank cache/Flash mmap backend is unnecessary with Minibox's 8 MiB PSRAM;
its standalone I2S driver, TFT_eSPI output, backlight PWM and restart-to-menu
flow would conflict with the existing hardware/application lifecycle.

[derdacavga/Esp32-S3-nes-emulator-by-DSN](https://github.com/derdacavga/Esp32-S3-nes-emulator-by-DSN),
revision `030c4b8`, was reviewed as an additional S3/Nofrendo/MAX98357 reference.
`src/nes_rom.c` uses separately allocated PSRAM PRG/CHR buffers and checks exact
read lengths; `src/osd.cpp` converts indexed pixels through an RGB565 palette,
duplicates mono PCM into stereo, and measures display time separately.
Minibox uses the same hardware-appropriate strategies through its own existing
drivers. Unlike DSN's display-flush audio callback, Minibox generates audio on
every emulated frame, including skipped video frames, and checks output errors.
There is no PSRAM-to-internal-RAM fallback for large ROM/frame allocations:
internal DMA memory must remain available for Minibox's other services.
DSN's GPLv3 Arduino code, ST7789 driver, SD pins and 80 MHz SPI configuration
were not copied; they do not match the Minibox ST7796 wiring or 40 MHz limit.

Original authorship and license legends remain in the source files and AUTHORS.
Upstream's README describes Nofrendo as GPLv2, while the imported source headers
specify the GNU **Library** General Public License version 2.
Both unmodified license texts are provided in COPYING and COPYING.LIB.
Preserve notices and provide corresponding source and applicable relinking
materials when distributing linked firmware; do not describe this core as
Apache-licensed merely because upstream's ESP32-specific drivers were Apache.
The new Minibox adapter files in this component are distributed under the
GNU Library General Public License version 2 (COPYING.LIB).

## Local changes

- ESP-IDF 5.3 CMake component, standard C boolean/allocator support.
- Explicit single-worker `nes_engine_open/frame/close` lifetime instead of
  the legacy global timer/main loop; restored 256x240 bitmap with overdraw.
- Checked NTSC iNES file loading, independent PRG/CHR ownership, trainer
  loading, unsupported-format errors, PSRAM ROM/bitmap allocation.
- 8 KiB battery RAM load/save (32 KiB for mapper 178) keyed by full-ROM CRC32,
  with a temporary file and retained backup, not Anemoia's full-state format.
- 24 kHz audio generated for every emulated frame, including frames not drawn.
  CPU-cycle synchronization before APU register reads/writes and at scanline
  boundaries preserves intra-frame changes instead of synthesizing only the
  final register state. Filter history belongs to the APU context and resets.
  Hardware audio, buffer-paced timing and LVGL output live in `main/app_nes.c`.
- Bytewise CPU word fetches for alignment, zero-page wrap, bank crossings
  and mirrored RAM; latched controller reads and high-strobe handling.
- Correct partial-initialization cleanup, initial PPU nametable pointers,
  16 KiB mirroring in 32 KiB mapper mode, bounded mapper lookup, and
  snapshotting the sprite clipping flag across mapper callbacks.
- CHR-ROM write protection, banked CHR-RAM, and resynchronizing MMC1/MMC3
  PRG/CHR banks when the mapping mode changes. MMC1 PRG is limited to 512 KiB;
  MMC1 board variants needing more than 8 KiB RAM are not covered.

## Additional mapper references

Register behavior was cross-checked against these mature GitHub implementations:

| Mapper | Mesen2 at `b9fa69ddc6d0a331fb103fdb5eef6904305703c2` | FCEUX at `ac7c6cb643dd788daed2b6b8a31fe91d369a4a27` |
|---|---|---|
| 112 | [Mapper112.h](https://github.com/SourMesen/Mesen2/blob/b9fa69ddc6d0a331fb103fdb5eef6904305703c2/Core/NES/Mappers/Ntdec/Mapper112.h) | [112.cpp](https://github.com/TASEmulators/fceux/blob/ac7c6cb643dd788daed2b6b8a31fe91d369a4a27/src/boards/112.cpp) |
| 178 | [Waixing178.h](https://github.com/SourMesen/Mesen2/blob/b9fa69ddc6d0a331fb103fdb5eef6904305703c2/Core/NES/Mappers/Waixing/Waixing178.h) | [178.cpp](https://github.com/TASEmulators/fceux/blob/ac7c6cb643dd788daed2b6b8a31fe91d369a4a27/src/boards/178.cpp) |
| 245 | [MMC3_245.h](https://github.com/SourMesen/Mesen2/blob/b9fa69ddc6d0a331fb103fdb5eef6904305703c2/Core/NES/Mappers/Mmc3Variants/MMC3_245.h) | Existing Minibox MMC3 implementation |

The adapters implement the observed register/banking rules using Nofrendo's
own APIs; no Mesen2/FCEUX source files, emulator infrastructure or ROMs are
vendored. Existing license notices remain unchanged.
112 implements two 8 KiB PRG windows, two 2 KiB and four 1 KiB CHR windows,
outer CHR bits and mirroring. 178 implements four PRG modes, register mirrors
at $4800-$4FFF and four 8 KiB RAM banks; battery saves include all 32 KiB.
178 education-board speech hardware and sports sensors are not emulated.
245 reuses the existing MMC3 register/scanline-IRQ path, adding the 512 KiB
PRG outer bit and fixed, invertible CHR-RAM halves. Its IRQ accuracy remains
that of the old core, not Mesen2's cycle-level MMC3 implementation.
Generated-cartridge regression covers bank/mode changes, CHR-ROM and CHR-RAM,
small/large 245 PRG images and persistence of all four 178 RAM banks.

## Video and audio

The audio adapter also borrows the small-DMA, complete-buffer blocking-output
strategy observed in Anemoia's `setupI2SDAC` and `Apu2A03::writeBuffer`, not its
independent APU task or Arduino driver. NES uses three 256-frame DMA buffers;
400-sample frame remainders stay in the software ring until a block is complete.
DSN's `vid_flush`-coupled synthesis and zero-timeout I2S writes are deliberately
not reused. Both local references output native 256x240 images; neither provides
a 480x320 scaler. At the user's request Minibox now also displays native
256x240 pixels, centered without scaling (112-pixel side margins, 40 top/bottom).
The emulator copies indexed frames into a two-buffer mailbox; Core 0 converts
colors and invalidates changed 16-row bands. Alternate-frame submissions target
up to 30 fps; 122,880-byte native frames reduce SPI traffic by about 55% versus
426x320, but actual animation/audio responsiveness still needs device acceptance.
The producer skips PPU drawing when the mailbox is pending or less than two
audio frames remain in the software ring, but still runs CPU/APU and input.
Startup prefills four frames and the running software watermark is three
frames; the shared audio driver and its DMA configuration are unchanged.

Joystick sampling remains in `main/buttons.c`, not in the emulator core.
Menus use four directions; NES uses eight. NES cardinal input requires the
larger normalized magnitude to exceed the smaller by >10 percentage points
and >1.25 times, with the existing 25% enter / 15% release center hysteresis.
Diagonals require both axes >=40% and smaller/larger >=85%, even while held.
Neither old direction nor diagonal state survives an isolation region.
See the [firmware README diagrams](../../README.md#nes-摇杆活跃区域旧版与新版)
for the previous and current initial-activation regions.

The legacy APU and mapper implementations remain approximate. Cycle-timed
register access does not make the envelope/frame sequencer cycle-exact.
PAL, NES 2.0, VS/FDS,
SNSS save states and nonstandard RAM sizes are deliberately rejected or
unavailable, rather than advertised as compatible.

The standalone C regression is `tests/test_nes_engine.c`, with host-only
ESP error/log shims in `tests/nes_host`. Compile it with this component's
CMake-listed C sources and private include directories using a native C
compiler; run in an empty temporary directory. It generates its own original
test cartridge, checks core output, mid-frame audio register timing, buffer
bounds and file lifecycle, and removes generated
ROM/save files on success. It does not test LCD/I2S hardware or commercial games.
The ordinary firmware build remains `idf.py build`, with no new build wrapper.
