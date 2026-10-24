/* Minibox iNES loader. No game data is bundled with the emulator. */
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include "nes_engine_internal.h"
#include "noftypes.h"
#include "nes.h"
#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#endif

#define MAX_ROM_BYTES (2 * 1024 * 1024)

static void *rom_alloc(size_t bytes) {
#ifdef ESP_PLATFORM
    return heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
    return malloc(bytes);
#endif
}

static void save_paths(const rominfo_t *rom, char *path, char *temp, char *backup) {
    const char *slash = strrchr(rom->filename, '/');
    int prefix = slash ? (int)(slash - rom->filename + 1) : 0;
    snprintf(path, PATH_MAX + 1, "%.*snes_%08" PRIx32 ".sav",
             prefix, rom->filename, (uint32_t)rom->crc32);
    sprintf(temp, "%s.tmp", path);
    sprintf(backup, "%s.bak", path);
}

static uint32_t crc32_update(uint32_t crc, const uint8_t *data, size_t bytes) {
    while (bytes--) {
        crc ^= *data++;
        for (unsigned bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ ((crc & 1) ? UINT32_C(0xedb88320) : 0);
        }
    }
    return crc;
}

static bool load_sram(rominfo_t *rom) {
    if (!(rom->flags & ROM_FLAG_BATTERY)) return true;
    char path[PATH_MAX + 1], temp[PATH_MAX + 5], backup[PATH_MAX + 5];
    save_paths(rom, path, temp, backup);
    FILE *file = fopen(path, "rb");
    if (!file && errno == ENOENT) file = fopen(backup, "rb");
    if (!file) {
        if (errno == ENOENT) return true;
        nes_engine_set_error("Cannot read battery save (%d)", errno);
        return false;
    }
    size_t bytes = (size_t)rom->sram_banks * 1024;
    bool ok = fread(rom->sram, 1, bytes, file) == bytes;
    ok = ok && fgetc(file) == EOF && !ferror(file);
    if (fclose(file) != 0) ok = false;
    if (!ok) nes_engine_set_error("Invalid battery save (expected %u bytes)", (unsigned)bytes);
    return ok;
}

esp_err_t nes_cartridge_save(void) {
    rominfo_t *rom = nes_getcontextptr()->rominfo;
    if (!rom || !(rom->flags & ROM_FLAG_BATTERY)) return ESP_OK;
    char path[PATH_MAX + 1], temp[PATH_MAX + 5], backup[PATH_MAX + 5];
    save_paths(rom, path, temp, backup);
    FILE *file = fopen(temp, "wb");
    if (!file) {
        nes_engine_set_error("Cannot create battery save (%d)", errno);
        return ESP_FAIL;
    }
    size_t bytes = (size_t)rom->sram_banks * 1024;
    bool ok = fwrite(rom->sram, 1, bytes, file) == bytes;
    if (fclose(file) != 0) ok = false;
    if (!ok) {
        nes_engine_set_error("Battery save write failed; storage may be full");
        return ESP_FAIL;
    }
    struct stat st;
    if (stat(path, &st) == 0) {
        if ((remove(backup) != 0 && errno != ENOENT) || rename(path, backup) != 0) {
            nes_engine_set_error("Cannot back up battery save (%d)", errno);
            return ESP_FAIL;
        }
    } else if (errno != ENOENT) {
        nes_engine_set_error("Cannot inspect battery save (%d)", errno);
        return ESP_FAIL;
    }
    if (rename(temp, path) != 0) {
        nes_engine_set_error("Cannot replace save; previous .bak retained (%d)", errno);
        return ESP_FAIL;
    }
    return ESP_OK;
}

int rom_checkmagic(const char *filename) {
    uint8_t magic[4];
    FILE *file = fopen(filename, "rb");
    if (!file) return -1;
    bool ok = fread(magic, 1, sizeof(magic), file) == sizeof(magic) &&
              memcmp(magic, "NES\x1a", sizeof(magic)) == 0;
    fclose(file);
    return ok ? 0 : -1;
}

rominfo_t *rom_load(const char *filename) {
    uint8_t header[16];
    struct stat st;
    rominfo_t *rom = NULL;
    FILE *file = NULL;
    const char *extension = filename ? strrchr(filename, '.') : NULL;
    if (!filename || strlen(filename) > PATH_MAX - 32 ||
        !extension || strcasecmp(extension, ".nes") != 0) {
        nes_engine_set_error("Invalid .nes path");
        return NULL;
    }
    file = fopen(filename, "rb");
    if (!file) {
        nes_engine_set_error("Cannot open ROM (%d)", errno);
        return NULL;
    }
    if (fread(header, 1, sizeof(header), file) != sizeof(header) ||
        memcmp(header, "NES\x1a", 4) != 0) {
        nes_engine_set_error("Not an iNES ROM");
        goto failed;
    }
    if ((header[7] & 0x0c) == 0x08) {
        nes_engine_set_error("NES 2.0 is not supported");
        goto failed;
    }
    unsigned mapper = (header[6] >> 4) | (header[7] & 0xf0);
    if ((header[7] & 0x0f) || (header[9] & 1) ||
        header[8] > (mapper == 178 ? 4 : 1) ||
        header[10] || header[11] || header[12] || header[13] || header[14] || header[15]) {
        nes_engine_set_error("Requires clean NTSC iNES 1.0 with supported RAM size");
        goto failed;
    }
    if (!mmc_peek(mapper)) {
        nes_engine_set_error("Mapper %u not supported (0,1,2,3,4,7,66,112,178,245)", mapper);
        goto failed;
    }
    size_t prg_bytes = (size_t)header[4] * 16384;
    size_t chr_bytes = (size_t)header[5] * 8192;
    size_t total = 16 + ((header[6] & 4) ? 512 : 0) + prg_bytes + chr_bytes;
    if (!prg_bytes || total > MAX_ROM_BYTES ||
        stat(filename, &st) != 0 || st.st_size != total) {
        nes_engine_set_error("Invalid ROM length (max 2 MiB, no trailing data)");
        goto failed;
    }
    if (((mapper == 0 || mapper == 3) && header[4] > 2) ||
        (mapper == 1 && header[4] > 32) ||
        (mapper == 0 && header[5] > 1) ||
        ((mapper == 7 || mapper == 66) && (header[4] < 2 || (header[4] & 1)))) {
        nes_engine_set_error("Invalid PRG/CHR banks for mapper %u", mapper);
        goto failed;
    }
    rom = calloc(1, sizeof(*rom));
    if (!rom) goto no_memory;
    strcpy(rom->filename, filename);
    rom->rom_banks = header[4];
    rom->vrom_banks = header[5];
    rom->sram_banks = mapper == 178 ? 32 : 8;
    rom->vram_banks = 1;
    rom->mapper_number = mapper;
    rom->mirror = (header[6] & 1) ? MIRROR_VERT : MIRROR_HORIZ;
    if (header[6] & 2) rom->flags |= ROM_FLAG_BATTERY;
    if (header[6] & 4) rom->flags |= ROM_FLAG_TRAINER;
    if (header[6] & 8) rom->flags |= ROM_FLAG_FOURSCREEN;
    rom->sram = calloc(rom->sram_banks, 1024);
    rom->rom = rom_alloc(prg_bytes);
    if (chr_bytes) rom->vrom = rom_alloc(chr_bytes);
    else rom->vram = calloc(1, 8192);
    if (!rom->sram || !rom->rom || (!rom->vrom && !rom->vram)) goto no_memory;
    if ((header[6] & 4) && fread(rom->sram + 0x1000, 1, 512, file) != 512) goto read_failed;
    if (fread(rom->rom, 1, prg_bytes, file) != prg_bytes) goto read_failed;
    if (chr_bytes && fread(rom->vrom, 1, chr_bytes, file) != chr_bytes) goto read_failed;
    uint32_t crc = crc32_update(UINT32_MAX, header, sizeof(header));
    if (header[6] & 4) crc = crc32_update(crc, rom->sram + 0x1000, 512);
    crc = crc32_update(crc, rom->rom, prg_bytes);
    if (chr_bytes) crc = crc32_update(crc, rom->vrom, chr_bytes);
    rom->crc32 = crc ^ UINT32_MAX;
    fclose(file);
    file = NULL;
    if (!load_sram(rom)) goto failed;
    return rom;

read_failed:
    nes_engine_set_error("ROM read failed");
    goto failed;
no_memory:
    nes_engine_set_error("Not enough memory for cartridge");
failed:
    if (file) fclose(file);
    rom_free(&rom);
    return NULL;
}

void rom_free(rominfo_t **rom) {
    if (!*rom) return;
    free((*rom)->rom);
    free((*rom)->vrom);
    free((*rom)->vram);
    free((*rom)->sram);
    free(*rom);
    *rom = NULL;
}
