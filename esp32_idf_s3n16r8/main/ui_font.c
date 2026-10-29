#include "ui_font.h"

#include <stdio.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "esp_spiffs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

typedef struct {
    char magic[4];
    uint16_t pixels, alpha_bits;
    uint32_t count, index_offset, data_offset, total_size;
} font_header_t;

typedef struct {
    uint32_t codepoint, offset;
    uint16_t advance;
    uint8_t width, height;
    int16_t x_offset, y_offset;
} font_entry_t;

_Static_assert(sizeof(font_header_t) == 24, "MBF1 header");
_Static_assert(sizeof(font_entry_t) == 16, "MBF1 index");

static uint8_t *s_data;
static font_header_t s_header;
static const font_entry_t *s_index;
static lv_font_t s_fonts[4];
static const unsigned s_sizes[] = {16, 24, 32, 48};
/* LVGL consumes a glyph immediately on the locked display task. */
static uint8_t s_glyph[8192];

static const font_entry_t *find_glyph(uint32_t codepoint) {
    uint32_t lo = 0, hi = s_header.count;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2;
        if (s_index[mid].codepoint < codepoint) lo = mid + 1;
        else hi = mid;
    }
    return lo < s_header.count && s_index[lo].codepoint == codepoint
               ? &s_index[lo] : NULL;
}

static int scale(int value, unsigned size) {
    return (value * (int)size + (value < 0 ? -12 : 12)) / 24;
}

static bool glyph_dsc(const lv_font_t *font, lv_font_glyph_dsc_t *dsc,
                      uint32_t codepoint, uint32_t next) {
    (void)next;
    unsigned size = *(const unsigned *)font->dsc;
    const font_entry_t *entry = find_glyph(codepoint);
    if (!entry) entry = find_glyph('?');
    if (!entry) return false;
    memset(dsc, 0, sizeof(*dsc));
    dsc->adv_w = scale(entry->advance, size);
    dsc->box_w = scale(entry->width, size);
    dsc->box_h = scale(entry->height, size);
    dsc->ofs_x = scale(entry->x_offset, size);
    dsc->ofs_y = (int)size - scale(entry->y_offset, size) - dsc->box_h;
    dsc->bpp = 4;
    return true;
}

static const uint8_t *glyph_bitmap(const lv_font_t *font, uint32_t codepoint) {
    const font_entry_t *entry = find_glyph(codepoint);
    if (!entry) entry = find_glyph('?');
    if (!entry) return NULL;
    unsigned size = *(const unsigned *)font->dsc;
    if (size == 24 && s_header.alpha_bits == 4) return s_data + entry->offset;
    unsigned width = scale(entry->width, size);
    unsigned height = scale(entry->height, size);
    memset(s_glyph, 0, (width * height + 1) / 2);
    for (unsigned y = 0; y < height; ++y) {
        for (unsigned x = 0; x < width; ++x) {
            unsigned src = (y * entry->height / height) * entry->width +
                           x * entry->width / width;
            unsigned bits = s_header.alpha_bits;
            unsigned per_byte = 8 / bits;
            unsigned alpha = (s_data[entry->offset + src / per_byte] >>
                              (8 - bits * (src % per_byte + 1))) & ((1U << bits) - 1);
            if (bits == 2) alpha *= 5;
            unsigned dst = y * width + x;
            s_glyph[dst / 2] |= dst & 1 ? alpha : alpha << 4;
        }
    }
    return s_glyph;
}

esp_err_t ui_font_init(void (*progress)(size_t loaded, size_t total)) {
    if (s_data) return ESP_OK;
    if (!esp_spiffs_mounted("fonts")) {
        const esp_vfs_spiffs_conf_t config = {
            .base_path = "/fonts", .partition_label = "fonts",
            .max_files = 2, .format_if_mount_failed = false,
        };
        esp_err_t err = esp_vfs_spiffs_register(&config);
        if (err != ESP_OK) return err;
    }
    FILE *file = fopen("/fonts/minibox_sans_24.bin", "rb");
    if (!file) return ESP_ERR_NOT_FOUND;
    esp_err_t err = ESP_ERR_INVALID_SIZE;
    if (fread(&s_header, sizeof(s_header), 1, file) != 1 ||
        memcmp(s_header.magic, "MBF1", 4) || s_header.pixels != 24 ||
        (s_header.alpha_bits != 2 && s_header.alpha_bits != 4) ||
        !s_header.count || s_header.count > 40000 ||
        s_header.index_offset != sizeof(s_header) ||
        s_header.data_offset != sizeof(s_header) +
                                s_header.count * sizeof(font_entry_t) ||
        s_header.total_size < s_header.data_offset ||
        s_header.total_size > 16 * 1024 * 1024) goto done;
    s_data = heap_caps_malloc(s_header.total_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_data) { err = ESP_ERR_NO_MEM; goto done; }
    memcpy(s_data, &s_header, sizeof(s_header));
    if (progress) progress(sizeof(s_header), s_header.total_size);
    for (size_t offset = sizeof(s_header); offset < s_header.total_size;) {
        size_t bytes = s_header.total_size - offset;
        if (bytes > 16384) bytes = 16384;
        if (fread(s_data + offset, 1, bytes, file) != bytes) goto done;
        offset += bytes;
        if (progress) progress(offset, s_header.total_size);
        vTaskDelay(1);
    }
    if (fgetc(file) != EOF) goto done;
    s_index = (const font_entry_t *)(s_data + s_header.index_offset);
    for (uint32_t i = 0; i < s_header.count; ++i) {
        const font_entry_t *entry = &s_index[i];
        size_t pixels = entry->width * entry->height;
        if ((i && entry->codepoint <= s_index[i - 1].codepoint) ||
            pixels > 4096 || entry->advance > 64 ||
            entry->offset < s_header.data_offset ||
            entry->offset > s_header.total_size ||
            (pixels * s_header.alpha_bits + 7) / 8 >
                s_header.total_size - entry->offset) goto done;
    }
    for (unsigned i = 0; i < 4; ++i) {
        s_fonts[i] = (lv_font_t){
            .get_glyph_dsc = glyph_dsc, .get_glyph_bitmap = glyph_bitmap,
            .line_height = s_sizes[i] + s_sizes[i] / 6,
            .base_line = s_sizes[i] / 6,
            .dsc = &s_sizes[i],
        };
    }
    err = ESP_OK;
done:
    fclose(file);
    if (err != ESP_OK) {
        heap_caps_free(s_data);
        s_data = NULL;
        s_index = NULL;
    }
    return err;
}

const lv_font_t *ui_font_get(unsigned pixels) {
    if (!s_data) return LV_FONT_DEFAULT;
    unsigned index = pixels <= 16 ? 0 : pixels <= 24 ? 1 : pixels <= 32 ? 2 : 3;
    return &s_fonts[index];
}
