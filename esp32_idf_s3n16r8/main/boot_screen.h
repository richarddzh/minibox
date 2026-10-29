#pragma once

#include <stddef.h>
#include "esp_err.h"

/* Single-task LCD ownership until ui_init starts the LVGL service. */
esp_err_t boot_screen_init(void);
void boot_screen_step(const char *text);
esp_err_t boot_screen_result(esp_err_t result);
void boot_screen_font_progress(size_t loaded, size_t total);
