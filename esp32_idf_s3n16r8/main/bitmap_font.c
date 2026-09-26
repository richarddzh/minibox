#include "bitmap_font.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_spiffs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define GLYPH_MAX_PIXELS 4096

typedef struct {
    char magic[4];
    uint16_t pixels;
    uint16_t alpha_bits;
    uint32_t count;
    uint32_t index_offset;
    uint32_t data_offset;
    uint32_t total_size;
} font_header_t;

typedef struct {
    uint32_t codepoint;
    uint32_t offset;
    uint16_t advance;
    uint8_t width;
    uint8_t height;
    int16_t x_offset;
    int16_t y_offset;
} font_entry_t;

_Static_assert(sizeof(font_header_t) == 24, "Font header layout");
_Static_assert(sizeof(font_entry_t) == 16, "Font index layout");

static const char *TAG = "font";
static font_header_t s_header;
static uint8_t *s_data;
static font_entry_t *s_index;

esp_err_t bitmap_font_init(void) {
    const esp_vfs_spiffs_conf_t config = {
        .base_path = "/fonts",
        .partition_label = "fonts",
        .max_files = 2,
        .format_if_mount_failed = false,
    };
    ESP_RETURN_ON_ERROR(esp_vfs_spiffs_register(&config), TAG,
                        "mount fonts; flash the filesystem image with idf.py flash");
    const char *path = "/fonts/minibox_sans_24.bin";
    struct stat status;
    ESP_RETURN_ON_FALSE(stat(path, &status) == 0, ESP_ERR_NOT_FOUND, TAG,
                        "font missing: %s", path);
    FILE *file = fopen(path, "rb");
    ESP_RETURN_ON_FALSE(file, ESP_FAIL, TAG, "open font: %s", strerror(errno));
    ESP_RETURN_ON_FALSE(fread(&s_header, sizeof(s_header), 1, file) == 1,
                        ESP_FAIL, TAG, "read header");
    ESP_RETURN_ON_FALSE(memcmp(s_header.magic, "MBF1", 4) == 0 &&
                        s_header.pixels == 24 && s_header.alpha_bits == 4 &&
                        s_header.count > 0 && s_header.count <= 40000 &&
                        s_header.index_offset == sizeof(s_header) &&
                        s_header.data_offset == sizeof(s_header) +
                            s_header.count * sizeof(font_entry_t) &&
                        s_header.total_size == status.st_size &&
                        s_header.data_offset <= s_header.total_size,
                        ESP_ERR_INVALID_SIZE, TAG, "invalid font header");
    s_data = heap_caps_malloc(s_header.total_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    ESP_RETURN_ON_FALSE(s_data, ESP_ERR_NO_MEM, TAG, "PSRAM font buffer");
    memcpy(s_data, &s_header, sizeof(s_header));
    /* Sequential SPIFFS reads avoid the expensive per-glyph file seeks. */
    for (size_t offset = sizeof(s_header); offset < s_header.total_size;) {
        size_t bytes = s_header.total_size - offset;
        if (bytes > 16384) bytes = 16384;
        ESP_RETURN_ON_FALSE(fread(s_data + offset, 1, bytes, file) == bytes,
                            ESP_FAIL, TAG, "read font payload");
        offset += bytes;
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    ESP_RETURN_ON_FALSE(fclose(file) == 0, ESP_FAIL, TAG, "close font");
    s_index = (font_entry_t *)(s_data + s_header.index_offset);
    for (uint32_t i = 0; i < s_header.count; ++i) {
        const font_entry_t *entry = &s_index[i];
        size_t pixels = entry->width * entry->height;
        ESP_RETURN_ON_FALSE((i == 0 || entry->codepoint > s_index[i - 1].codepoint) &&
                            pixels <= GLYPH_MAX_PIXELS &&
                            entry->advance <= 64 &&
                            entry->offset >= s_header.data_offset &&
                            entry->offset <= s_header.total_size &&
                            (pixels + 1) / 2 <= s_header.total_size - entry->offset,
                            ESP_ERR_INVALID_SIZE, TAG, "invalid glyph index");
    }
    ESP_LOGI(TAG, "Minibox Sans 24: %lu glyphs, %lu bytes, 4-bit antialiasing, SPIFFS -> PSRAM",
             (unsigned long)s_header.count, (unsigned long)s_header.total_size);
    return ESP_OK;
}

static esp_err_t glyph(uint32_t codepoint, const font_entry_t **result) {
    uint32_t lo = 0, hi = s_header.count;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2;
        if (s_index[mid].codepoint < codepoint) lo = mid + 1;
        else hi = mid;
    }
    ESP_RETURN_ON_FALSE(lo < s_header.count && s_index[lo].codepoint == codepoint,
                        ESP_ERR_NOT_FOUND, TAG, "missing glyph U+%04lx",
                        (unsigned long)codepoint);
    *result = &s_index[lo];
    return ESP_OK;
}

static esp_err_t decode_utf8(const char **text, uint32_t *codepoint) {
    const unsigned char *p = (const unsigned char *)*text;
    int count;
    uint32_t minimum;
    if (*p < 0x80) {
        *codepoint = *p;
        *text += 1;
        return ESP_OK;
    } else if (*p >= 0xC2 && *p <= 0xDF) {
        count = 2; minimum = 0x80; *codepoint = *p & 0x1F;
    } else if (*p >= 0xE0 && *p <= 0xEF) {
        count = 3; minimum = 0x800; *codepoint = *p & 0x0F;
    } else if (*p >= 0xF0 && *p <= 0xF4) {
        count = 4; minimum = 0x10000; *codepoint = *p & 0x07;
    } else {
        ESP_LOGE(TAG, "invalid UTF-8 leading byte");
        return ESP_ERR_INVALID_ARG;
    }
    for (int i = 1; i < count; ++i) {
        ESP_RETURN_ON_FALSE((p[i] & 0xC0) == 0x80, ESP_ERR_INVALID_ARG,
                            TAG, "invalid UTF-8 continuation");
        *codepoint = (*codepoint << 6) | (p[i] & 0x3F);
    }
    ESP_RETURN_ON_FALSE(*codepoint >= minimum && *codepoint <= 0x10FFFF &&
                        !(*codepoint >= 0xD800 && *codepoint <= 0xDFFF),
                        ESP_ERR_INVALID_ARG, TAG, "invalid UTF-8 codepoint");
    *text += count;
    return ESP_OK;
}

static uint16_t blend(uint16_t foreground, uint16_t background, unsigned alpha) {
    unsigned inverse = 15 - alpha;
    unsigned r = (((foreground >> 11) & 31) * alpha +
                  ((background >> 11) & 31) * inverse + 7) / 15;
    unsigned g = (((foreground >> 5) & 63) * alpha +
                  ((background >> 5) & 63) * inverse + 7) / 15;
    unsigned b = ((foreground & 31) * alpha + (background & 31) * inverse + 7) / 15;
    return (r << 11) | (g << 5) | b;
}

esp_err_t bitmap_font_draw(uint8_t *frame, int width, int height, int x, int y,
                           const char *utf8, uint16_t color) {
    ESP_RETURN_ON_FALSE(s_index, ESP_ERR_INVALID_STATE, TAG, "not initialized");
    ESP_RETURN_ON_FALSE(frame && utf8 && width > 0 && height > 0,
                        ESP_ERR_INVALID_ARG, TAG, "invalid draw arguments");
    const int start_x = x;
    while (*utf8) {
        uint32_t codepoint;
        ESP_RETURN_ON_ERROR(decode_utf8(&utf8, &codepoint), TAG, "text decode");
        if (codepoint == '\n') {
            x = start_x;
            y += 32;
            continue;
        }
        const font_entry_t *entry;
        ESP_RETURN_ON_ERROR(glyph(codepoint, &entry), TAG, "text glyph");
        const uint8_t *coverage = s_data + entry->offset;
        for (unsigned row = 0; row < entry->height; ++row) {
            int py = y + entry->y_offset + (int)row;
            if (py < 0 || py >= height) continue;
            for (unsigned column = 0; column < entry->width; ++column) {
                int px = x + entry->x_offset + (int)column;
                if (px < 0 || px >= width) continue;
                unsigned pixel = row * entry->width + column;
                uint8_t packed = coverage[pixel / 2];
                unsigned alpha = pixel & 1 ? packed & 15 : packed >> 4;
                if (!alpha) continue;
                size_t offset = ((size_t)py * width + px) * 2;
                uint16_t background = ((uint16_t)frame[offset] << 8) | frame[offset + 1];
                uint16_t blended = blend(color, background, alpha);
                frame[offset] = blended >> 8;
                frame[offset + 1] = blended & 0xff;
            }
        }
        x += entry->advance;
    }
    return ESP_OK;
}
