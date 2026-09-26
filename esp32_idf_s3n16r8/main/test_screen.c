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
static int s_visual_x;
static int s_visual_y;
static int s_number_x;
static int s_number_y;
static int64_t s_numbers_at_us;
static struct {
    int x;
    int y;
    int number_x;
    int number_y;
    int direction_x;
    int direction_y;
    bool pressed;
    uint32_t presses;
    audio_phase_t audio;
    bool valid;
} s_displayed;

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

static int stable_axis(int value, int previous) {
    int magnitude = value < 0 ? -value : value;
    if (previous == 0 && magnitude < 5) return 0;
    if (magnitude <= 2) return 0;
    if (value > previous + 1 || value < previous - 1) return value;
    return previous;
}

static void axis_text(int y, char axis, int value) {
    int magnitude = value < 0 ? -value : value;
    char line[24];
    snprintf(line, sizeof(line), "%c: %c%d.%02d", axis, value < 0 ? '-' : '+',
             magnitude / 100, magnitude % 100);
    text(236, y, line, WHITE);
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
    s_displayed.valid = false;
    rect(1, 86, LCD_WIDTH - 2, 200, BLACK);
    text(16, 118, message, YELLOW);
    text(16, 152, detail, WHITE);
    return flush_dynamic();
}

esp_err_t test_screen_update(const joystick_state_t *state, audio_phase_t audio) {
    s_visual_x = stable_axis(state->percent_x, s_visual_x);
    s_visual_y = stable_axis(state->percent_y, s_visual_y);
    int64_t now = esp_timer_get_time();
    if (s_numbers_at_us == 0 || now - s_numbers_at_us >= 250000) {
        s_number_x = s_visual_x;
        s_number_y = s_visual_y;
        s_numbers_at_us = now;
    }
    if (s_displayed.valid &&
        s_displayed.x == s_visual_x && s_displayed.y == s_visual_y &&
        s_displayed.number_x == s_number_x && s_displayed.number_y == s_number_y &&
        s_displayed.direction_x == state->direction_x &&
        s_displayed.direction_y == state->direction_y &&
        s_displayed.pressed == state->button.pressed &&
        s_displayed.presses == state->button.presses &&
        s_displayed.audio == audio) {
        return ESP_OK;
    }
    rect(1, 86, LCD_WIDTH - 2, 200, BLACK);
    rect(16, 96, 200, 184, GRAY);
    rect(18, 98, 196, 180, BLACK);
    rect(115, 98, 1, 180, GRAY);
    rect(18, 187, 196, 1, GRAY);
    int x = 116 + s_visual_x * 90 / 100;
    int y = 188 + s_visual_y * 82 / 100;
    rect(x - 5, y - 5, 11, 11, state->button.pressed ? YELLOW : CYAN);

    char line[40];
    axis_text(88, 'X', s_number_x);
    axis_text(116, 'Y', s_number_y);
    text(236, 144, "\u7cbe\u5ea6 0.01", WHITE);
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
    const char *audio_status = audio == AUDIO_HOLD ? "K \u6309\u4f4f 1 \u79d2" :
                               audio == AUDIO_RECORDING ? "\u5f55\u97f3\u4e2d (3s max)" :
                               audio == AUDIO_WAIT_RELEASE ? "\u5df2\u6ee1 3s \u7b49\u677e\u5f00" :
                               audio == AUDIO_TONE_WAIT_RELEASE ? "\u97f3\u8c03\u6d4b\u8bd5 \u7b49\u677e\u5f00" :
                               audio == AUDIO_PLAYING ? "\u64ad\u653e\u4e2d" :
                               audio == AUDIO_ERROR ? "\u97f3\u9891\u9519\u8bef \u67e5\u4e32\u53e3" :
                               "\u97f3\u9891: K \u6309\u4f4f 1 \u79d2";
    text(236, 256, audio_status, audio == AUDIO_ERROR ? YELLOW : WHITE);
    ESP_RETURN_ON_ERROR(flush_dynamic(), TAG, "update display");
    s_displayed.x = s_visual_x;
    s_displayed.y = s_visual_y;
    s_displayed.number_x = s_number_x;
    s_displayed.number_y = s_number_y;
    s_displayed.direction_x = state->direction_x;
    s_displayed.direction_y = state->direction_y;
    s_displayed.pressed = state->button.pressed;
    s_displayed.presses = state->button.presses;
    s_displayed.audio = audio;
    s_displayed.valid = true;
    return ESP_OK;
}
