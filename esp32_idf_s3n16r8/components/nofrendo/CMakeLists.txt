idf_component_register(
    SRCS "nes_engine.c" "nes_cartridge.c" "bitmap.c"
         "cpu/nes6502.c" "nes/nes.c" "nes/nes_ppu.c" "nes/nes_pal.c"
         "nes/nes_mmc.c" "nes/nesinput.c" "sndhrdw/nes_apu.c"
         "mappers/map000.c" "mappers/map001.c" "mappers/map002.c"
         "mappers/map003.c" "mappers/map004.c" "mappers/map007.c"
         "mappers/map066.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "." "cpu" "nes" "sndhrdw" "libsnss"
    REQUIRES esp_common
    PRIV_REQUIRES heap log
)
# The legacy CPU/PPU use aliased word accesses and intentional fallthrough.
target_compile_options(${COMPONENT_LIB} PRIVATE -O2 -fno-strict-aliasing
    -Wno-unused-function -Wno-unused-variable -Wno-implicit-fallthrough)
