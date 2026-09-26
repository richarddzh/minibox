#pragma once

#include <stdint.h>
#include "esp_err.h"

esp_err_t bitmap_font_init(void);
/* Single display task; framebuffer uses big-endian RGB565 pixels. */
esp_err_t bitmap_font_draw(uint8_t *frame, int width, int height, int x, int y,
                           const char *utf8, uint16_t color);
