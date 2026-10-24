#undef NDEBUG
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nes_engine.h"
#include "nes.h"
#include "nesinput.h"

/* Original 6502 test program: set a backdrop, play a pulse, mark RAM, loop.
 * No commercial ROM or downloaded game data is used. */
static const uint8_t program[] = {
    0x78, 0xD8, 0xA2, 0xFF, 0x9A,
    0xA9, 0x00, 0x8D, 0x00, 0x20, 0x8D, 0x01, 0x20,
    0xA9, 0x3F, 0x8D, 0x06, 0x20, 0xA9, 0x00, 0x8D, 0x06, 0x20,
    0xA9, 0x21, 0x8D, 0x07, 0x20,
    0xA9, 0x01, 0x8D, 0x15, 0x40,
    0xA9, 0xBF, 0x8D, 0x00, 0x40,
    0xA9, 0xFD, 0x8D, 0x02, 0x40,
    0xA9, 0x08, 0x8D, 0x03, 0x40,
    0xA9, 0x5A, 0x85, 0x00,
};

static int16_t samples[NES_FRAME_SAMPLES];

static void make_rom(unsigned mapper, unsigned flags, unsigned prg, unsigned chr) {
    FILE *file = fopen("test_generated.nes", "wb");
    assert(file);
    uint8_t header[16] = {'N', 'E', 'S', 0x1A, prg, chr,
                          (mapper << 4) | flags, mapper & 0xF0};
    assert(fwrite(header, 1, 16, file) == 16);
    uint8_t trainer[512];
    memset(trainer, 0xA5, sizeof(trainer));
    if (flags & 4) assert(fwrite(trainer, 1, 512, file) == 512);
    size_t bytes = prg * 16384;
    uint8_t *data = calloc(1, bytes);
    assert(data);
    for (size_t i = 0; i < bytes; ++i) data[i] = i / 8192;
    size_t base = bytes - 16384;
    memcpy(data + base, program, sizeof(program));
    data[base + sizeof(program)] = 0x4C;
    data[base + sizeof(program) + 1] = sizeof(program);
    data[base + sizeof(program) + 2] = 0xC0;
    for (int i = 0; i < 3; ++i) {
        data[bytes - 6 + i * 2] = 0;
        data[bytes - 5 + i * 2] = 0xC0;
    }
    assert(fwrite(data, 1, bytes, file) == bytes);
    free(data);
    for (unsigned bank = 0; bank < chr * 8; ++bank) {
        uint8_t block[1024];
        memset(block, bank, sizeof(block));
        assert(fwrite(block, 1, sizeof(block), file) == sizeof(block));
    }
    assert(fclose(file) == 0);
}

static void patch_file(long offset, uint8_t value) {
    FILE *file = fopen("test_generated.nes", "r+b");
    assert(file && fseek(file, offset, SEEK_SET) == 0);
    assert(fputc(value, file) != EOF);
    assert(fclose(file) == 0);
}

static void test_frames_and_controller(void) {
    make_rom(0, 4, 1, 1);
    for (int cycle = 0; cycle < 40; ++cycle) {
        if (cycle == 0) puts("Opening generated NROM");
        assert(nes_engine_open("test_generated.nes") == ESP_OK);
        if (cycle == 0) puts("Running generated NROM");
        assert(nes_getcontextptr()->rominfo->sram[0x1000] == 0xA5);
        bool audible = false;
        for (int frame = 0; frame < 12; ++frame) {
            nes_engine_frame(0x81, frame % 2 == 0, samples);
            for (int i = 0; i < NES_FRAME_SAMPLES; ++i) audible |= samples[i] != 0;
        }
        assert(audible);
        nes6502_context cpu;
        nes6502_getcontext(&cpu);
        assert(cpu.mem_page[0][0] == 0x5A);
        int pitch;
        const uint8_t *pixels = nes_engine_pixels(&pitch);
        assert(pitch >= 272 && (pixels[120 * pitch + 128] & 0x3F) == 0x21);
        assert(nes_engine_palette()[0x21] != 0);

        ppu_writehigh(0x4016, 1);
        for (int i = 0; i < 16; ++i) assert((ppu_readhigh(0x4016) & 1) == 1);
        ppu_writehigh(0x4016, 0);
        nes_engine_frame(0, false, samples);
        for (int i = 0; i < 8; ++i) assert((ppu_readhigh(0x4016) & 1) == ((0x81 >> i) & 1));
        for (int i = 0; i < 40; ++i) assert((ppu_readhigh(0x4016) & 1) == 1);
        ppu_writehigh(0x4016, 1);
        ppu_writehigh(0x4016, 0);
        assert((ppu_readhigh(0x4016) & 1) == 0);

        uint8_t before = nes_getcontextptr()->rominfo->vrom[0];
        ppu_write(0x2006, 0);
        ppu_write(0x2006, 0);
        ppu_write(0x2007, 0xFF);
        assert(nes_getcontextptr()->rominfo->vrom[0] == before);
        assert(nes_engine_close() == ESP_OK);
    }
}

static void test_audio_register_timing(void) {
    make_rom(0, 0, 1, 1);
    assert(nes_engine_open("test_generated.nes") == ESP_OK);
    nes_engine_frame(0, false, samples);
    apu_t initial;
    apu_getcontext(&initial);
    initial.mix_enable = 1;
    initial.filter_type = APU_FILTER_NONE;
    apu_setcontext(&initial);
    apu_write(0x4015, 1);
    apu_write(0x4000, 0xBF);
    apu_write(0x4002, 0xFD);
    apu_write(0x4003, 0x08);
    apu_getcontext(&initial);

    int16_t expected[NES_FRAME_SAMPLES];
    apu_process(expected, NES_FRAME_SAMPLES / 2);
    apu_write(0x4015, 0);
    apu_process(expected + NES_FRAME_SAMPLES / 2, NES_FRAME_SAMPLES / 2);

    struct {
        int16_t before;
        int16_t pcm[NES_FRAME_SAMPLES];
        int16_t after;
    } output;
    for (int i = 0; i < NES_FRAME_SAMPLES; ++i) output.pcm[i] = 0x5A5A;
    output.before = output.after = 0x1234;
    apu_setcontext(&initial);
    apu_begin_frame(output.pcm, NES_FRAME_SAMPLES, 4000);
    uint32_t start = nes6502_getcycles(false);
    nes6502_execute(1000);
    unsigned first = (nes6502_getcycles(false) - start) * NES_FRAME_SAMPLES / 4000;
    assert(first > 0 && first < NES_FRAME_SAMPLES / 2);
    assert(apu_read(0x4015) & 1);
    assert(memcmp(output.pcm, expected, first * sizeof(int16_t)) == 0);
    assert(output.pcm[first] == 0x5A5A);
    nes6502_execute(1000);
    unsigned middle = (nes6502_getcycles(false) - start) * NES_FRAME_SAMPLES / 4000;
    assert(middle == NES_FRAME_SAMPLES / 2);
    /* Disabling mid-frame must not erase the pulse already played. */
    apu_write(0x4015, 0);
    assert(memcmp(output.pcm, expected, middle * sizeof(int16_t)) == 0);
    assert(output.pcm[middle] == 0x5A5A);
    nes6502_execute(5000);
    apu_sync();
    apu_end_frame();
    assert(memcmp(output.pcm, expected, sizeof(expected)) == 0);
    assert(output.before == 0x1234 && output.after == 0x1234);
    assert(nes_engine_close() == ESP_OK);
}

static void write_mapper(unsigned address, uint8_t value) {
    const map_memwrite *handler = nes_getcontextptr()->mmc->intf->mem_write;
    for (; handler && handler->write_func; ++handler) {
        if (address >= handler->min_range && address <= handler->max_range) {
            handler->write_func(address, value);
            return;
        }
    }
    assert(!"mapper handler missing");
}

static void write_mmc1(unsigned address, uint8_t value) {
    for (int i = 0; i < 5; ++i) write_mapper(address, (value >> i) & 1);
}

static void expect_prg(unsigned address, unsigned bank8k) {
    nes6502_context cpu;
    nes6502_getcontext(&cpu);
    assert(cpu.mem_page[address >> 12] == nes_getcontextptr()->rominfo->rom + bank8k * 8192);
}

static void expect_chr(unsigned address, unsigned bank1k) {
    ppu_t ppu;
    ppu_getcontext(&ppu);
    rominfo_t *rom = nes_getcontextptr()->rominfo;
    uint8_t *chr = rom->vrom ? rom->vrom : rom->vram;
    assert(ppu.page[address >> 10] + address == chr + bank1k * 1024);
}

static void test_mappers_and_cpu_edges(void) {
    make_rom(1, 0, 4, 2);
    assert(nes_engine_open("test_generated.nes") == ESP_OK);
    expect_prg(0x8000, 0); expect_prg(0xC000, 6);
    write_mmc1(0xE000, 2);
    expect_prg(0x8000, 4); expect_prg(0xC000, 6);
    write_mmc1(0x8000, 0x08);
    expect_prg(0x8000, 0); expect_prg(0xC000, 4);
    write_mmc1(0xA000, 2);
    write_mmc1(0xC000, 1);
    write_mmc1(0x8000, 0x1C);
    expect_chr(0, 8); expect_chr(0x1000, 4);
    assert(nes_engine_close() == ESP_OK);

    make_rom(4, 0, 4, 2);
    assert(nes_engine_open("test_generated.nes") == ESP_OK);
    expect_prg(0x8000, 0); expect_prg(0xA000, 1); expect_prg(0xC000, 6); expect_prg(0xE000, 7);
    write_mapper(0x8000, 6); write_mapper(0x8001, 3);
    expect_prg(0x8000, 3);
    write_mapper(0x8000, 0x46);
    expect_prg(0x8000, 6); expect_prg(0xC000, 3);
    write_mapper(0x8000, 0x80);
    expect_chr(0, 4); expect_chr(0x1000, 0);
    assert(nes_engine_close() == ESP_OK);

    const unsigned mappers[] = {0, 2, 3, 7, 66};
    for (unsigned i = 0; i < sizeof(mappers) / sizeof(mappers[0]); ++i) {
        unsigned mapper = mappers[i];
        make_rom(mapper, 0, 2, mapper == 7 ? 0 : 1);
        assert(nes_engine_open("test_generated.nes") == ESP_OK);
        for (int frame = 0; frame < 4; ++frame) nes_engine_frame(0, true, samples);
        nes6502_context cpu;
        nes6502_getcontext(&cpu);
        assert(cpu.mem_page[0][0] == 0x5A);
        assert(nes_engine_close() == ESP_OK);
    }
    make_rom(0, 0, 1, 0);
    assert(nes_engine_open("test_generated.nes") == ESP_OK);
    nes6502_context cpu;
    nes6502_getcontext(&cpu);
    uint8_t *rom = nes_getcontextptr()->rominfo->rom;
    rom[0xFFE] = 0x4C; rom[0xFFF] = 0; rom[0x2000] = 0xC0;
    cpu.mem_page[9] = rom + 0x2000;
    cpu.pc_reg = 0x8FFE;
    cpu.burn_cycles = 0;
    nes6502_setcontext(&cpu);
    nes6502_execute(3);
    nes6502_getcontext(&cpu);
    assert(cpu.pc_reg == 0xC000);
    rom[0] = 0xB1; rom[1] = 0xFF;
    cpu.pc_reg = 0x8000; cpu.y_reg = 0;
    cpu.mem_page[0][0xFF] = 0; cpu.mem_page[0][0] = 0x60; cpu.mem_page[0][0x100] = 0x70;
    nes_getcontextptr()->rominfo->sram[0] = 0xAB;
    nes6502_setcontext(&cpu);
    nes6502_execute(5);
    nes6502_getcontext(&cpu);
    assert(cpu.a_reg == 0xAB);
    assert(nes_engine_close() == ESP_OK);
}

static void test_saves_and_rejection(void) {
    make_rom(0, 2, 1, 0);
    assert(nes_engine_open("test_generated.nes") == ESP_OK);
    unsigned crc = nes_getcontextptr()->rominfo->crc32;
    nes_getcontextptr()->rominfo->sram[17] = 0x5A;
    assert(nes_engine_close() == ESP_OK);
    assert(nes_engine_open("test_generated.nes") == ESP_OK);
    assert(nes_getcontextptr()->rominfo->sram[17] == 0x5A);
    assert(nes_engine_close() == ESP_OK);
    char save[40], backup[44];
    snprintf(save, sizeof(save), "nes_%08x.sav", crc);
    snprintf(backup, sizeof(backup), "%s.bak", save);
    assert(remove(save) == 0);
    assert(nes_engine_open("test_generated.nes") == ESP_OK);
    assert(nes_getcontextptr()->rominfo->sram[17] == 0x5A);
    assert(nes_engine_close() == ESP_OK);
    patch_file(16 + 100, 0x99);
    assert(nes_engine_open("test_generated.nes") == ESP_OK);
    assert(nes_getcontextptr()->rominfo->sram[17] == 0);
    unsigned changed_crc = nes_getcontextptr()->rominfo->crc32;
    assert(changed_crc != crc);
    assert(nes_engine_close() == ESP_OK);
    char changed_save[40];
    snprintf(changed_save, sizeof(changed_save), "nes_%08x.sav", changed_crc);
    assert(remove(changed_save) == 0);
    make_rom(0, 2, 1, 0);
    FILE *file = fopen(save, "wb");
    assert(file && fputc(0, file) != EOF && fclose(file) == 0);
    assert(nes_engine_open("test_generated.nes") != ESP_OK);
    assert(strstr(nes_engine_error(), "save"));
    assert(nes_engine_close() == ESP_OK);
    assert(remove(save) == 0 && remove(backup) == 0);

    const unsigned offsets[] = {0, 4, 7, 9, 6};
    const uint8_t values[] = {0, 0, 8, 1, 0xF0};
    for (unsigned i = 0; i < sizeof(values); ++i) {
        make_rom(0, 0, 1, 0);
        patch_file(offsets[i], values[i]);
        assert(nes_engine_open("test_generated.nes") != ESP_OK);
        assert(nes_engine_error()[0]);
        assert(nes_engine_close() == ESP_OK);
    }
    make_rom(0, 0, 1, 0);
    patch_file(4, 2);
    assert(nes_engine_open("test_generated.nes") != ESP_OK);
    assert(nes_engine_close() == ESP_OK);
    assert(remove("test_generated.nes") == 0);
    assert(nes_engine_open("test_generated.nes") != ESP_OK);
    assert(nes_engine_close() == ESP_OK);
}

static void test_added_mappers(void) {
    make_rom(112, 0, 8, 64);
    assert(nes_engine_open("test_generated.nes") == ESP_OK);
    expect_prg(0xC000, 14); expect_prg(0xE000, 15);
    write_mapper(0x8000, 0); write_mapper(0xA000, 5);
    expect_prg(0x8000, 5);
    write_mapper(0x8000, 1); write_mapper(0xA000, 9);
    expect_prg(0xA000, 9);
    write_mapper(0x8000, 2); write_mapper(0xA000, 7);
    expect_chr(0, 6); expect_chr(0x400, 7);
    write_mapper(0x8000, 3); write_mapper(0xA000, 11);
    expect_chr(0x800, 10); expect_chr(0xC00, 11);
    for (unsigned i = 4; i < 8; ++i) {
        write_mapper(0x8000, i); write_mapper(0xA000, i + 1);
    }
    for (unsigned i = 4; i < 8; ++i) {
        write_mapper(0xC000, 1 << i);
        for (unsigned j = 4; j < 8; ++j)
            expect_chr(0x1000 + (j - 4) * 0x400, j + 1 + (i == j ? 256 : 0));
    }
    ppu_t ppu;
    write_mapper(0xE000, 1);
    ppu_getcontext(&ppu);
    assert(ppu.page[8] + 0x2000 == ppu.page[9] + 0x2400);
    write_mapper(0xE000, 0);
    ppu_getcontext(&ppu);
    assert(ppu.page[8] + 0x2000 == ppu.page[10] + 0x2800);
    assert(nes_engine_close() == ESP_OK);

    make_rom(245, 0, 64, 0);
    assert(nes_engine_open("test_generated.nes") == ESP_OK);
    expect_prg(0xC000, 62); expect_prg(0xE000, 63);
    write_mapper(0x8000, 6); write_mapper(0x8001, 5);
    write_mapper(0x8000, 7); write_mapper(0x8001, 9);
    write_mapper(0x8000, 0); write_mapper(0x8001, 2);
    expect_prg(0x8000, 69); expect_prg(0xA000, 73);
    expect_prg(0xC000, 126); expect_prg(0xE000, 127);
    expect_chr(0, 0); expect_chr(0x1000, 4);
    write_mapper(0x8000, 0xC0);
    expect_prg(0x8000, 126); expect_prg(0xC000, 69);
    expect_chr(0, 4); expect_chr(0x1000, 0);
    write_mapper(0x8001, 0);
    expect_prg(0x8000, 62); expect_prg(0xC000, 5);
    assert(nes_engine_close() == ESP_OK);
    make_rom(245, 0, 8, 0);
    assert(nes_engine_open("test_generated.nes") == ESP_OK);
    expect_prg(0xC000, 14); expect_prg(0xE000, 15);
    write_mapper(0x8000, 0); write_mapper(0x8001, 2);
    expect_prg(0xC000, 14); expect_prg(0xE000, 15);
    assert(nes_engine_close() == ESP_OK);
    make_rom(245, 0, 8, 2);
    assert(nes_engine_open("test_generated.nes") == ESP_OK);
    write_mapper(0x8000, 2); write_mapper(0x8001, 10);
    expect_chr(0x1000, 10);
    assert(nes_engine_close() == ESP_OK);

    make_rom(178, 2, 32, 0);
    patch_file(8, 4);
    assert(nes_engine_open("test_generated.nes") == ESP_OK);
    assert(nes_getcontextptr()->rominfo->sram_banks == 32);
    expect_prg(0x8000, 0); expect_prg(0xC000, 2);
    write_mapper(0x4802, 1); write_mapper(0x4801, 3);
    expect_prg(0x8000, 20); expect_prg(0xC000, 22);
    write_mapper(0x4800, 4);
    expect_prg(0x8000, 22); expect_prg(0xC000, 22);
    write_mapper(0x4800, 2);
    expect_prg(0x8000, 22); expect_prg(0xC000, 30);
    write_mapper(0x4800, 6); write_mapper(0x4801, 2);
    expect_prg(0x8000, 20); expect_prg(0xC000, 28);
    expect_chr(0, 0); expect_chr(0x1C00, 7);
    for (int bank = 0; bank < 4; ++bank) {
        write_mapper(0x4FFF, bank);
        nes6502_context cpu;
        nes6502_getcontext(&cpu);
        assert(cpu.mem_page[6] == nes_getcontextptr()->rominfo->sram + bank * 8192);
        assert(cpu.mem_page[7] == cpu.mem_page[6] + 4096);
        cpu.mem_page[6][0] = 0xA0 + bank;
        cpu.mem_page[7][4095] = 0xB0 + bank;
    }
    unsigned crc = nes_getcontextptr()->rominfo->crc32;
    assert(nes_engine_close() == ESP_OK);
    assert(nes_engine_open("test_generated.nes") == ESP_OK);
    for (int bank = 0; bank < 4; ++bank) {
        write_mapper(0x4803, bank);
        nes6502_context cpu;
        nes6502_getcontext(&cpu);
        assert(cpu.mem_page[6][0] == 0xA0 + bank);
        assert(cpu.mem_page[7][4095] == 0xB0 + bank);
    }
    assert(nes_engine_close() == ESP_OK);
    char save[40], backup[44];
    snprintf(save, sizeof(save), "nes_%08x.sav", crc);
    snprintf(backup, sizeof(backup), "%s.bak", save);
    assert(remove(save) == 0 && remove(backup) == 0);
    assert(remove("test_generated.nes") == 0);
}

int main(void) {
    setbuf(stdout, NULL);
    puts("NES frame/controller regression");
    test_frames_and_controller();
    puts("NES cycle-timed audio regression");
    test_audio_register_timing();
    puts("NES mapper/CPU regression");
    test_mappers_and_cpu_edges();
    puts("NES 112/178/245 mapper and banked-save regression");
    test_added_mappers();
    puts("NES storage/validation regression");
    test_saves_and_rejection();
    puts("NES core: CPU/PPU/APU, controller latch, mapper modes, saves, rejection and 40 reloads passed.");
    return 0;
}
