#pragma once
#include <stdbool.h>
#include <esp_err.h>

typedef enum {
    VC_IDLE = 0,
    VC_CONNECTING,
    VC_RECORDING,
    VC_UPLOADING,
    VC_WAITING_REPLY,
    VC_PLAYING,
    VC_DRAINING,
    VC_DONE,
    VC_ERROR,
} voice_chat_status_t;

esp_err_t voice_chat_init(void);

bool voice_chat_busy(void);
voice_chat_status_t voice_chat_status(void);
const char *voice_chat_message(void);

// Open a Doubao ASR session, then ask audio_io to start recording.
esp_err_t voice_chat_start(void);

// Loop pump while recording: drains mic ring into ASR WebSocket frames.
// Returns ESP_OK if still healthy (status may have changed).
esp_err_t voice_chat_pump(void);

// Request stop, then finish ASR, call the Agent and run TTS playback in
// the background worker task.
esp_err_t voice_chat_stop_and_process(void);

// Optional callback fires on every status change (any context).
typedef void (*voice_chat_status_cb_t)(void *ctx);
void voice_chat_set_status_cb(voice_chat_status_cb_t cb, void *ctx);

// Called from the voice worker after both ASR and Agent text are available.
typedef void (*voice_chat_turn_cb_t)(const char *user_text,
                                     const char *assistant_text,
                                     void *ctx);
void voice_chat_set_turn_cb(voice_chat_turn_cb_t cb, void *ctx);
