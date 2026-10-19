#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#define NES_VIDEO_WIDTH 256
#define NES_VIDEO_HEIGHT 240
#define NES_AUDIO_RATE 24000
#define NES_FRAME_SAMPLES (NES_AUDIO_RATE / 60)

/* One instance; all calls belong to the emulation worker. */
esp_err_t nes_engine_open(const char *path);
esp_err_t nes_engine_close(void);
const char *nes_engine_error(void);
void nes_engine_frame(uint8_t controller, bool draw, int16_t *samples);
const uint8_t *nes_engine_pixels(int *pitch);
const uint16_t *nes_engine_palette(void);
