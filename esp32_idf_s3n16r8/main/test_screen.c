#include "test_screen.h"
#include "app_config.h"
#include "st7796.h"

#include <inttypes.h>
#include <stdio.h>
#include "esp_check.h"
#include "esp_heap_caps.h"

#define BLACK 0x0000
#define WHITE 0xffff
#define GRAY 0x4208
#define GREEN 0x07e0
#define CYAN 0x07ff
#define YELLOW 0xffe0

static const char *TAG = "screen";
static uint8_t *s_frame;

/* Original 3x5 block glyphs; rows use their low three bits. */
static const uint8_t s_digits[10][5] = {
    {7,5,5,5,7}, {2,6,2,2,7}, {7,1,7,4,7}, {7,1,7,1,7}, {5,5,7,1,1},
    {7,4,7,1,7}, {7,4,7,5,7}, {7,1,2,2,2}, {7,5,7,5,7}, {7,5,7,1,7},
};
static const uint8_t s_letters[26][5] = {
    {2,5,7,5,5}, {6,5,6,5,6}, {3,4,4,4,3}, {6,5,5,5,6},
    {7,4,6,4,7}, {7,4,6,4,4}, {3,4,5,5,3}, {5,5,7,5,5},
    {7,2,2,2,7}, {1,1,1,5,2}, {5,5,6,5,5}, {4,4,4,4,7},
    {5,7,7,5,5}, {5,7,7,7,5}, {2,5,5,5,2}, {6,5,6,4,4},
    {2,5,5,3,1}, {6,5,6,5,5}, {3,4,2,1,6}, {7,2,2,2,2},
    {5,5,5,5,7}, {5,5,5,5,2}, {5,5,7,7,5}, {5,5,2,5,5},
    {5,5,2,2,2}, {7,1,2,4,7},
};

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
    for (; *value; ++value, x += 12) {
        uint8_t punctuation[5] = {0};
        const uint8_t *glyph = punctuation;
        if (*value >= '0' && *value <= '9') glyph = s_digits[*value - '0'];
        else if (*value >= 'A' && *value <= 'Z') glyph = s_letters[*value - 'A'];
        else if (*value == '-') punctuation[2] = 7;
        else if (*value == ':') punctuation[1] = punctuation[3] = 2;
        else if (*value == '%') {
            punctuation[0] = 5;
            punctuation[1] = 1;
            punctuation[2] = 2;
            punctuation[3] = 4;
            punctuation[4] = 5;
        }
        for (int row = 0; row < 5; ++row) {
            for (int column = 0; column < 3; ++column) {
                if (glyph[row] & (4 >> column)) {
                    rect(x + column * 3, y + row * 3, 3, 3, color);
                }
            }
        }
    }
}

static esp_err_t flush_dynamic(void) {
    return st7796_draw_rows(86, 204, s_frame + 86 * LCD_WIDTH * 2);
}

esp_err_t test_screen_init(void) {
    s_frame = heap_caps_calloc(LCD_WIDTH * LCD_HEIGHT, 2,
                               MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    ESP_RETURN_ON_FALSE(s_frame, ESP_ERR_NO_MEM, TAG, "PSRAM framebuffer");
    rect(0, 0, LCD_WIDTH, 1, WHITE);
    rect(0, LCD_HEIGHT - 1, LCD_WIDTH, 1, WHITE);
    rect(0, 0, 1, LCD_HEIGHT, WHITE);
    rect(LCD_WIDTH - 1, 0, 1, LCD_HEIGHT, WHITE);
    text(16, 16, "MINIBOX ST7796 480X320", WHITE);
    const uint16_t colors[] = {0xf800, GREEN, 0x001f, WHITE, BLACK, YELLOW};
    const char *labels[] = {"R", "G", "B", "W", "K", "Y"};
    for (int i = 0; i < 6; ++i) {
        rect(16 + i * 74, 46, 72, 26, colors[i]);
        text(45 + i * 74, 52, labels[i], i == 1 || i == 3 || i == 5 ? BLACK : WHITE);
    }
    text(16, 298, "MOVE STICK - PRESS K", WHITE);
    ESP_RETURN_ON_ERROR(st7796_draw_rows(0, LCD_HEIGHT, s_frame), TAG, "initial frame");
    return st7796_backlight_on();
}

esp_err_t test_screen_message(const char *message, const char *detail) {
    rect(1, 86, LCD_WIDTH - 2, 204, BLACK);
    text(16, 118, message, YELLOW);
    text(16, 152, detail, WHITE);
    return flush_dynamic();
}

esp_err_t test_screen_update(const joystick_state_t *state) {
    rect(1, 86, LCD_WIDTH - 2, 204, BLACK);
    rect(16, 96, 200, 184, GRAY);
    rect(18, 98, 196, 180, BLACK);
    rect(115, 98, 1, 180, GRAY);
    rect(18, 187, 196, 1, GRAY);
    int x = 116 + state->percent_x * 90 / 100;
    int y = 188 + state->percent_y * 82 / 100;
    rect(x - 5, y - 5, 11, 11, state->button.pressed ? YELLOW : CYAN);

    char line[40];
    snprintf(line, sizeof(line), "X:%4d %4d%%", state->raw_x, state->percent_x);
    text(236, 96, line, WHITE);
    snprintf(line, sizeof(line), "Y:%4d %4d%%", state->raw_y, state->percent_y);
    text(236, 120, line, WHITE);
    snprintf(line, sizeof(line), "CX:%4d CY:%4d", state->center_x, state->center_y);
    text(236, 144, line, WHITE);
    const char *horizontal = state->direction_x < 0 ? "LEFT" :
                             state->direction_x > 0 ? "RIGHT" : "CENTER";
    const char *vertical = state->direction_y < 0 ? "UP" :
                           state->direction_y > 0 ? "DOWN" : "CENTER";
    snprintf(line, sizeof(line), "X %s", horizontal);
    text(236, 168, line, CYAN);
    snprintf(line, sizeof(line), "Y %s", vertical);
    text(236, 192, line, CYAN);
    text(236, 216, state->button.pressed ? "K PRESSED" : "K RELEASED",
         state->button.pressed ? YELLOW : GREEN);
    snprintf(line, sizeof(line), "COUNT:%" PRIu32, state->button.presses);
    text(236, 240, line, WHITE);
    snprintf(line, sizeof(line), "PINS %d %d %d",
             JOYSTICK_X_PIN, JOYSTICK_Y_PIN, JOYSTICK_K_PIN);
    text(236, 264, line, WHITE);
    return flush_dynamic();
}
