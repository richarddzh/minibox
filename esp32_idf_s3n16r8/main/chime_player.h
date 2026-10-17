#pragma once

#include <stdbool.h>
#include <esp_err.h>

typedef enum {
    CHIME_IDLE = 0,
    CHIME_CONNECTING,
    CHIME_WAITING_REPLY,
    CHIME_PLAYING,
    CHIME_DRAINING,
    CHIME_DONE,
    CHIME_ERROR,
} chime_status_t;

esp_err_t chime_player_init(void);
bool      chime_player_busy(void);
chime_status_t chime_player_status(void);
const char *chime_player_message(void);
esp_err_t  chime_player_start(const char *time_arg);

// Speak an arbitrary fixed UTF-8 text through the TTS backend, bypassing
// the agent step. Used for short spoken announcements such as quiet-mode
// enter/exit prompts. Rejects if the player or audio pipeline is busy.
esp_err_t  chime_player_start_text(const char *text);

typedef void (*chime_player_status_cb_t)(void *ctx);
void chime_player_set_status_cb(chime_player_status_cb_t cb, void *ctx);

typedef void (*chime_player_text_cb_t)(const char *text, void *ctx);
void chime_player_set_text_cb(chime_player_text_cb_t cb, void *ctx);
