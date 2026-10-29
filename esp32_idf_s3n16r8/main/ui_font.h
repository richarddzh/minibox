#pragma once

#include "esp_err.h"
#include "lvgl.h"
#include <stddef.h>

/* Reuses the licensed MBF1 asset in the existing fonts partition. */
esp_err_t ui_font_init(void (*progress)(size_t loaded, size_t total));
const lv_font_t *ui_font_get(unsigned pixels);
