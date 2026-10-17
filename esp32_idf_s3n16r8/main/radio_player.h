#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <esp_err.h>

#define RADIO_VOLUME_MIN 1
#define RADIO_VOLUME_MAX 5
#define RADIO_SPECTRUM_BANDS 10

typedef enum {
    RADIO_PLAYER_IDLE = 0,
    RADIO_PLAYER_CONNECTING,
    RADIO_PLAYER_BUFFERING,
    RADIO_PLAYER_PLAYING,
    RADIO_PLAYER_STOPPING,
    RADIO_PLAYER_ERROR,
} radio_player_state_t;

typedef struct {
    radio_player_state_t state;
    size_t station_index;
    uint32_t sample_rate;
    uint8_t channels;
    uint32_t bitrate;
    uint8_t volume_level;
    uint8_t spectrum[RADIO_SPECTRUM_BANDS];
    char message[48];
    uint32_t generation;
} radio_player_snapshot_t;

esp_err_t radio_player_init(void);
esp_err_t radio_player_start(size_t station_index);
esp_err_t radio_player_stop(uint32_t timeout_ms);
bool radio_player_busy(void);
void radio_player_get_snapshot(radio_player_snapshot_t *snapshot);
void radio_player_set_volume(uint8_t level);
