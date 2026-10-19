#pragma once

#include "nes_engine.h"

void nes_engine_set_error(const char *format, ...) __attribute__((format(printf, 1, 2)));
esp_err_t nes_cartridge_save(void);
