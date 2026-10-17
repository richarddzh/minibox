#pragma once

#include "esp_err.h"
#include "lvgl.h"

/* Reuses the licensed MBF1 asset in the existing fonts partition. */
esp_err_t ui_font_init(void);
const lv_font_t *ui_font_get(unsigned pixels);
