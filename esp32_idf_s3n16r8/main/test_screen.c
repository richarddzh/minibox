#include "test_screen.h"
#include "app_config.h"
#include "st7796.h"
#include "bitmap_font.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"

#define BLACK 0x0000
#define WHITE 0xffff
#define GRAY 0x4208
#define GREEN 0x07e0
#define CYAN 0x07ff
#define YELLOW 0xffe0

static const char *TAG = "screen";
static uint8_t *s_frame;
static uint8_t *s_sent;
static joystick_state_t s_numbers;
static int64_t s_numbers_at_us;

static void rect(int x, int y, int width, int height, uint16_t color) {
    int right = x + width, bottom = y + height;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (right > LCD_WIDTH) right = LCD_WIDTH;
    if (bottom > LCD_HEIGHT) bottom = LCD_HEIGHT;
    for (int row = y; row < bottom; ++row) {
        for (int column = x; column < right; ++column) {
            size_t offset = (row * LCD_WIDTH + column) * 2;
            s_frame[offset] = color >> 8;
            s_frame[offset + 1] = color & 0xff;
        }
    }
}

static void text(int x, int y, const char *value, uint16_t color) {
    ESP_ERROR_CHECK(bitmap_font_draw(s_frame, LCD_WIDTH, LCD_HEIGHT, x, y, value, color));
}

static bool tile_changed(int x, int y) {
    for (int row = y; row < y + 16; ++row) {
        size_t offset = (row * LCD_WIDTH + x) * 2;
        if (memcmp(s_frame + offset, s_sent + offset, 16 * 2) != 0) return true;
    }
    return false;
}

static esp_err_t flush_dynamic(void) {
    for (int y = 80; y < 288; y += 16) {
        int x = 0;
        while (x < LCD_WIDTH) {
            if (!tile_changed(x, y)) {
                x += 16;
                continue;
            }
            int first = x;
            x += 16;
            while (x < LCD_WIDTH && tile_changed(x, y)) x += 16;
            size_t offset = (y * LCD_WIDTH + first) * 2;
            ESP_RETURN_ON_ERROR(st7796_draw_region(first, y, x - first, 16,
                                                   s_frame + offset, LCD_WIDTH * 2),
                                TAG, "changed rectangle");
            for (int row = y; row < y + 16; ++row) {
                offset = (row * LCD_WIDTH + first) * 2;
                memcpy(s_sent + offset, s_frame + offset, (x - first) * 2);
            }
        }
    }
    return ESP_OK;
}

esp_err_t test_screen_init(void) {
    ESP_RETURN_ON_ERROR(bitmap_font_init(), TAG, "load filesystem font");
    s_frame = heap_caps_calloc(LCD_WIDTH * LCD_HEIGHT, 2,
                               MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    ESP_RETURN_ON_FALSE(s_frame, ESP_ERR_NO_MEM, TAG, "PSRAM framebuffer");
    s_sent = heap_caps_malloc(LCD_WIDTH * LCD_HEIGHT * 2,
                              MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    ESP_RETURN_ON_FALSE(s_sent, ESP_ERR_NO_MEM, TAG, "PSRAM previous frame");
    rect(0, 0, LCD_WIDTH, 1, WHITE);
    rect(0, LCD_HEIGHT - 1, LCD_WIDTH, 1, WHITE);
    rect(0, 0, 1, LCD_HEIGHT, WHITE);
    rect(LCD_WIDTH - 1, 0, 1, LCD_HEIGHT, WHITE);
    text(16, 12, "Minibox \u5c4f\u5e55\u6d4b\u8bd5 480x320", WHITE);
    const uint16_t colors[] = {0xf800, GREEN, 0x001f, WHITE, BLACK, YELLOW};
    const char *labels[] = {"R", "G", "B", "W", "K", "Y"};
    for (int i = 0; i < 6; ++i) {
        rect(16 + i * 74, 46, 72, 26, colors[i]);
        text(43 + i * 74, 44, labels[i], i == 1 || i == 3 || i == 5 ? BLACK : WHITE);
    }
    text(16, 288, "\u4e2d\u6587 ABC 123 - \u6447\u6746\u6d4b\u8bd5", WHITE);
    ESP_RETURN_ON_ERROR(st7796_draw_rows(0, LCD_HEIGHT, s_frame), TAG, "initial frame");
    memcpy(s_sent, s_frame, LCD_WIDTH * LCD_HEIGHT * 2);
    return st7796_backlight_on();
}

esp_err_t test_screen_message(const char *message, const char *detail) {
    rect(1, 86, LCD_WIDTH - 2, 200, BLACK);
    text(16, 118, message, YELLOW);
    text(16, 152, detail, WHITE);
    return flush_dynamic();
}

esp_err_t test_screen_update(const joystick_state_t *state) {
    int64_t now = esp_timer_get_time();
    if (s_numbers_at_us == 0 || now - s_numbers_at_us >= 250000) {
        s_numbers = *state;
        s_numbers_at_us = now;
    }
    rect(1, 86, LCD_WIDTH - 2, 200, BLACK);
    rect(16, 96, 200, 184, GRAY);
    rect(18, 98, 196, 180, BLACK);
    rect(115, 98, 1, 180, GRAY);
    rect(18, 187, 196, 1, GRAY);
    int x = 116 + state->percent_x * 90 / 100;
    int y = 188 + state->percent_y * 82 / 100;
    rect(x - 5, y - 5, 11, 11, state->button.pressed ? YELLOW : CYAN);

    char line[40];
    snprintf(line, sizeof(line), "X:%4d %4d%%", s_numbers.raw_x, s_numbers.percent_x);
    text(236, 88, line, WHITE);
    snprintf(line, sizeof(line), "Y:%4d %4d%%", s_numbers.raw_y, s_numbers.percent_y);
    text(236, 116, line, WHITE);
    snprintf(line, sizeof(line), "CX:%4d CY:%4d", state->center_x, state->center_y);
    text(236, 144, line, WHITE);
    const char *horizontal = state->direction_x < 0 ? "\u5de6" :
                             state->direction_x > 0 ? "\u53f3" : "\u4e2d";
    const char *vertical = state->direction_y < 0 ? "\u4e0a" :
                           state->direction_y > 0 ? "\u4e0b" : "\u4e2d";
    snprintf(line, sizeof(line), "\u65b9\u5411 X:%s Y:%s", horizontal, vertical);
    text(236, 172, line, CYAN);
    text(236, 200, state->button.pressed ? "K \u6309\u4e0b PRESSED" : "K \u677e\u5f00 RELEASED",
         state->button.pressed ? YELLOW : GREEN);
    snprintf(line, sizeof(line), "\u8ba1\u6570:%" PRIu32, state->button.presses);
    text(236, 228, line, WHITE);
    snprintf(line, sizeof(line), "\u5f15\u811a %d %d %d",
             JOYSTICK_X_PIN, JOYSTICK_Y_PIN, JOYSTICK_K_PIN);
    text(236, 256, line, WHITE);
    return flush_dynamic();
}
