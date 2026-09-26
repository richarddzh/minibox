#pragma once

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

esp_err_t st7796_init(void);
esp_err_t st7796_backlight_on(void);
/* RGB565 high byte first. Transfers complete before return; call from one task. */
esp_err_t st7796_draw_rows(int y, int height, const uint8_t *pixels);
esp_err_t st7796_draw_region(int x, int y, int width, int height,
                             const uint8_t *pixels, size_t stride_bytes);
uint32_t st7796_transfer_count(void);
