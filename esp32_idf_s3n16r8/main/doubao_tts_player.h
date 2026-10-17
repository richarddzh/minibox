#pragma once

#include <esp_err.h>

typedef void (*doubao_tts_status_cb_t)(void *ctx, const char *msg);

esp_err_t doubao_tts_play_text(const char *text,
                               doubao_tts_status_cb_t status_cb,
                               void *status_ctx);
