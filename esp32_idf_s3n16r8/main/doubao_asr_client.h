#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <esp_err.h>

typedef struct doubao_asr_session doubao_asr_session_t;

esp_err_t doubao_asr_session_start(doubao_asr_session_t **out);
esp_err_t doubao_asr_session_send_audio(doubao_asr_session_t *session,
                                        const uint8_t *pcm,
                                        size_t len,
                                        bool is_last);
esp_err_t doubao_asr_session_finish(doubao_asr_session_t *session,
                                    char **out_text);
void doubao_asr_session_close(doubao_asr_session_t *session);
