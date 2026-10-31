#include "boot_screen.h"

#include "st7796.h"
#include "boot_logo_data.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"

#define WIDTH 480
#define HEIGHT 320
#define STRIPE_ROWS 16
#define LINE_HEIGHT 12
#define TEXT_TOP 144
#define LINES 14
#define COLUMNS 77
#define PIXEL_BYTES (WIDTH * STRIPE_ROWS * 2)

_Static_assert(TEXT_TOP + LINES * LINE_HEIGHT <= HEIGHT, "Boot log fits display");
_Static_assert(6 + BOOT_LOGO_HEIGHT <= TEXT_TOP, "Boot logo fits header");

static const char *TAG = "boot";
static uint8_t *s_pixels;
static char s_lines[LINES][COLUMNS + 1];
static unsigned s_count;
static bool s_initialized;
static char s_step[48];
static int64_t s_step_us;
static size_t s_progress;

typedef struct {
    char ch;
    uint8_t rows[7];
} ascii_glyph_t;

static const ascii_glyph_t s_ascii[] = {
    {'A', {14,17,17,31,17,17,17}}, {'B', {30,17,17,30,17,17,30}},
    {'C', {14,17,16,16,16,17,14}}, {'D', {30,17,17,17,17,17,30}},
    {'E', {31,16,16,30,16,16,31}}, {'F', {31,16,16,30,16,16,16}},
    {'G', {14,17,16,23,17,17,14}}, {'H', {17,17,17,31,17,17,17}},
    {'I', {14,4,4,4,4,4,14}},     {'J', {7,2,2,2,2,18,12}},
    {'K', {17,18,20,24,20,18,17}}, {'L', {16,16,16,16,16,16,31}},
    {'M', {17,27,21,21,17,17,17}}, {'N', {17,25,25,21,19,19,17}},
    {'O', {14,17,17,17,17,17,14}}, {'P', {30,17,17,30,16,16,16}},
    {'Q', {14,17,17,17,21,18,13}}, {'R', {30,17,17,30,20,18,17}},
    {'S', {15,16,16,14,1,1,30}},   {'T', {31,4,4,4,4,4,4}},
    {'U', {17,17,17,17,17,17,14}}, {'V', {17,17,17,17,17,10,4}},
    {'W', {17,17,17,21,21,27,17}}, {'X', {17,17,10,4,10,17,17}},
    {'Y', {17,17,10,4,4,4,4}},    {'Z', {31,1,2,4,8,16,31}},
    {'0', {14,17,19,21,25,17,14}}, {'1', {4,12,4,4,4,4,14}},
    {'2', {14,17,1,2,4,8,31}},    {'3', {30,1,1,14,1,1,30}},
    {'4', {2,6,10,18,31,2,2}},    {'5', {31,16,16,30,1,1,30}},
    {'6', {14,16,16,30,17,17,14}}, {'7', {31,1,2,4,8,8,8}},
    {'8', {14,17,17,14,17,17,14}}, {'9', {14,17,17,15,1,1,14}},
    {'[', {14,8,8,8,8,8,14}},     {']', {14,2,2,2,2,2,14}},
    {'.', {0,0,0,0,0,6,6}},       {':', {0,6,6,0,6,6,0}},
    {'/', {1,2,2,4,8,8,16}},      {'-', {0,0,0,31,0,0,0}},
    {'%', {17,2,4,4,8,16,17}},    {'?', {14,17,1,2,4,0,4}},
};

static void pixel(int x, int y, uint16_t color) {
    if (x < 0 || x >= WIDTH || y < 0 || y >= STRIPE_ROWS) return;
    size_t offset = (y * WIDTH + x) * 2;
    s_pixels[offset] = color >> 8;
    s_pixels[offset + 1] = color & 255;
}

static void text(int x, int y, const char *value, int scale, uint16_t color) {
    for (; *value; ++value, x += 6 * scale) {
        int ch = toupper((unsigned char)*value);
        for (size_t i = 0; i < sizeof(s_ascii) / sizeof(s_ascii[0]); ++i) {
            if (s_ascii[i].ch != ch) continue;
            for (int row = 0; row < 7; ++row) {
                for (int col = 0; col < 5; ++col) {
                    if (!(s_ascii[i].rows[row] & (16 >> col))) continue;
                    for (int dy = 0; dy < scale; ++dy)
                        for (int dx = 0; dx < scale; ++dx)
                            pixel(x + col * scale + dx, y + row * scale + dy, color);
                }
            }
            break;
        }
    }
}

static void draw_line(unsigned line) {
    memset(s_pixels, 0, PIXEL_BYTES);
    text(8, 2, s_lines[line], 1, 0xc618);
    ESP_ERROR_CHECK(st7796_draw_rows(TEXT_TOP + line * LINE_HEIGHT,
                                     LINE_HEIGHT, s_pixels));
}

esp_err_t boot_screen_init(void) {
    if (s_initialized) return ESP_OK;
    esp_err_t err = st7796_init();
    if (err != ESP_OK) return err;
    /* st7796 copies these pixels into its own internal DMA transfer buffer. */
    s_pixels = heap_caps_malloc(PIXEL_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_pixels) {
        ESP_LOGE(TAG, "boot pixel buffer allocation failed");
        return ESP_ERR_NO_MEM;
    }
    memset(s_pixels, 0, PIXEL_BYTES);
    for (int y = 0; y < HEIGHT; y += STRIPE_ROWS) {
        err = st7796_draw_rows(y, STRIPE_ROWS, s_pixels);
        if (err != ESP_OK) goto failed;
    }
    for (int top = 0; top < TEXT_TOP; top += STRIPE_ROWS) {
        memset(s_pixels, 0, PIXEL_BYTES);
        text(BOOT_LOGO_WIDTH + 32, 10 - top, "MINIBOX BIOS", 2, 0xc618);
        text(BOOT_LOGO_WIDTH + 32, 34 - top,
             "ESP32-S3 / 16MB FLASH / 8MB PSRAM", 1, 0x8410);
        for (int y = 0; y < STRIPE_ROWS; ++y) {
            int logo_y = top + y - 6;
            if (logo_y < 0 || logo_y >= BOOT_LOGO_HEIGHT) continue;
            for (int x = 0; x < BOOT_LOGO_WIDTH; ++x) {
                pixel(8 + x, y, s_boot_logo_rgb565[logo_y * BOOT_LOGO_WIDTH + x]);
            }
        }
        err = st7796_draw_rows(top, STRIPE_ROWS, s_pixels);
        if (err != ESP_OK) goto failed;
    }
    err = st7796_backlight_on();
    if (err != ESP_OK) goto failed;
    s_initialized = true;
    return ESP_OK;
failed:
    ESP_LOGE(TAG, "boot display failed: %s", esp_err_to_name(err));
    boot_screen_finish();
    return err;
}

void boot_screen_finish(void) {
    heap_caps_free(s_pixels);
    s_pixels = NULL;
}

void boot_screen_step(const char *value) {
    if (s_count == LINES) {
        memmove(s_lines, s_lines + 1, sizeof(s_lines) - sizeof(s_lines[0]));
        --s_count;
        for (unsigned i = 0; i < s_count; ++i) draw_line(i);
    }
    snprintf(s_step, sizeof(s_step), "%s", value);
    snprintf(s_lines[s_count], sizeof(s_lines[0]), "[ .. ] %.47s", value);
    draw_line(s_count++);
    s_step_us = esp_timer_get_time();
    s_progress = 0;
    ESP_LOGI(TAG, "starting: %s", value);
}

esp_err_t boot_screen_result(esp_err_t result) {
    unsigned long elapsed = (esp_timer_get_time() - s_step_us) / 1000;
    if (s_count) {
        snprintf(s_lines[s_count - 1], sizeof(s_lines[0]), "[%s] %.47s %lums",
                 result == ESP_OK ? " OK " : "FAIL", s_step, elapsed);
        draw_line(s_count - 1);
    }
    if (result == ESP_OK) ESP_LOGI(TAG, "%s: OK (%lums)", s_step, elapsed);
    else ESP_LOGE(TAG, "%s: %s (%lums)", s_step, esp_err_to_name(result), elapsed);
    return result;
}

void boot_screen_font_progress(size_t loaded, size_t total) {
    if (!total || !s_count) return;
    if (loaded != total && loaded - s_progress < 65536 && s_progress) return;
    s_progress = loaded;
    snprintf(s_lines[s_count - 1], sizeof(s_lines[0]), "[ .. ] FONTS %u/%u KIB %u%%",
             (unsigned)((loaded + 1023) / 1024),
             (unsigned)((total + 1023) / 1024), (unsigned)(loaded * 100 / total));
    draw_line(s_count - 1);
}
