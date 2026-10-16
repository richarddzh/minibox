#pragma once

#include "esp_err.h"

esp_err_t radio_stream_init(void);
esp_err_t radio_stream_play(void);
esp_err_t radio_stream_start(void);
const char *radio_stream_status(void);
