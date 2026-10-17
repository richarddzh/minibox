#include "voice_chat.h"
#include "audio_io.h"
#include "doubao_agent_client.h"
#include "doubao_asr_client.h"
#include "doubao_tts_player.h"
#include "mem_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_timer.h>

static const char *TAG = "voice";

#define VOICE_TASK_CORE        0
#define VOICE_TASK_PRIO        4
#define VOICE_IO_BUF_BYTES     2048

static volatile voice_chat_status_t s_status = VC_IDLE;
static char s_msg[48] = "voice idle";
static TaskHandle_t s_worker_task;
static doubao_asr_session_t *s_asr;
static size_t s_uploaded_bytes;

static uint8_t s_io_buf[VOICE_IO_BUF_BYTES];

static voice_chat_status_cb_t s_status_cb;
static void *s_status_ctx;
static voice_chat_turn_cb_t s_turn_cb;
static void *s_turn_ctx;

static void voice_worker(void *arg);

static void set_status(voice_chat_status_t st, const char *m) {
    s_status = st;
    if (m) {
        strncpy(s_msg, m, sizeof(s_msg) - 1);
        s_msg[sizeof(s_msg) - 1] = 0;
    }
    if (s_status_cb) s_status_cb(s_status_ctx);
}

void voice_chat_set_status_cb(voice_chat_status_cb_t cb, void *ctx) {
    s_status_cb = cb;
    s_status_ctx = ctx;
}

void voice_chat_set_turn_cb(voice_chat_turn_cb_t cb, void *ctx) {
    s_turn_cb = cb;
    s_turn_ctx = ctx;
}

esp_err_t voice_chat_init(void) {
    esp_err_t e = audio_io_init();
    if (e != ESP_OK) return e;
    if (!s_worker_task) {
        BaseType_t ok = xTaskCreatePinnedToCore(voice_worker, "voice_worker",
                                                6144, NULL, VOICE_TASK_PRIO,
                                                &s_worker_task, VOICE_TASK_CORE);
        if (ok != pdPASS) return ESP_FAIL;
    }
    set_status(VC_IDLE, "voice ready");
    return ESP_OK;
}

bool voice_chat_busy(void) {
    switch (s_status) {
    case VC_CONNECTING:
    case VC_RECORDING:
    case VC_UPLOADING:
    case VC_WAITING_REPLY:
    case VC_PLAYING:
    case VC_DRAINING:
        return true;
    default:
        return false;
    }
}

voice_chat_status_t voice_chat_status(void) { return s_status; }
const char *voice_chat_message(void) { return s_msg; }

static void close_asr(void) {
    if (s_asr) {
        doubao_asr_session_close(s_asr);
        s_asr = NULL;
    }
}

esp_err_t voice_chat_start(void) {
    if (voice_chat_busy()) {
        set_status(VC_ERROR, "voice busy");
        return ESP_ERR_INVALID_STATE;
    }

    set_status(VC_CONNECTING, "asr connect");
    mem_log("asr pre");
    esp_err_t err = doubao_asr_session_start(&s_asr);
    if (err != ESP_OK) {
        close_asr();
        set_status(VC_ERROR, "asr connect fail");
        return err;
    }

    err = audio_io_start_recording(1000);
    if (err != ESP_OK) {
        close_asr();
        set_status(VC_ERROR, "rec start");
        return err;
    }

    s_uploaded_bytes = 0;
    set_status(VC_RECORDING, "recording...");
    ESP_LOGI(TAG, "doubao asr streaming started");
    mem_log("asr post");
    return ESP_OK;
}

esp_err_t voice_chat_pump(void) {
    if (s_status != VC_RECORDING) return ESP_OK;
    if (!audio_io_mic_active()) {
        close_asr();
        set_status(VC_ERROR, "microphone stopped");
        return ESP_FAIL;
    }
    size_t n = audio_io_read_mic(s_io_buf, sizeof(s_io_buf), 5);
    if (n == 0) return ESP_OK;

    esp_err_t err = doubao_asr_session_send_audio(s_asr, s_io_buf, n, false);
    if (err != ESP_OK) {
        close_asr();
        if (audio_io_stop_recording(2000) != ESP_OK) {
            audio_io_abort(2000);
        }
        set_status(VC_ERROR, "asr send fail");
        return err;
    }
    s_uploaded_bytes += n;
    return ESP_OK;
}

static esp_err_t drain_mic_to_asr_final(void) {
    uint8_t last[VOICE_IO_BUF_BYTES];
    size_t last_len = 0;

    for (;;) {
        size_t n = audio_io_read_mic(s_io_buf, sizeof(s_io_buf), 0);
        if (n == 0) break;

        if (last_len > 0) {
            esp_err_t err = doubao_asr_session_send_audio(s_asr, last, last_len, false);
            if (err != ESP_OK) return err;
            s_uploaded_bytes += last_len;
        }
        memcpy(last, s_io_buf, n);
        last_len = n;
    }

    esp_err_t err = doubao_asr_session_send_audio(s_asr, last_len ? last : NULL,
                                                  last_len, true);
    if (err == ESP_OK) s_uploaded_bytes += last_len;
    return err;
}

static void voice_tts_status(void *ctx, const char *msg) {
    (void)ctx;
    if (!msg) return;
    if (strstr(msg, "playing")) {
        set_status(VC_PLAYING, "playing...");
    } else if (strstr(msg, "drain")) {
        set_status(VC_DRAINING, "drain spk");
    } else {
        set_status(VC_WAITING_REPLY, msg);
    }
}

static void voice_worker(void *arg) {
    (void)arg;
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (s_status != VC_UPLOADING) continue;

        if (audio_io_stop_recording(2000) != ESP_OK) {
            audio_io_abort(2000);
            close_asr();
            set_status(VC_ERROR, "rec stop fail");
            continue;
        }

        if (drain_mic_to_asr_final() != ESP_OK) {
            close_asr();
            set_status(VC_ERROR, "asr final fail");
            continue;
        }

        set_status(VC_WAITING_REPLY, "asr result");
        char *transcript = NULL;
        esp_err_t err = doubao_asr_session_finish(s_asr, &transcript);
        close_asr();
        if (err != ESP_OK || !transcript || !transcript[0]) {
            free(transcript);
            set_status(VC_ERROR, "empty transcript");
            continue;
        }
        ESP_LOGI(TAG, "asr uploaded=%u transcript=%s",
                 (unsigned)s_uploaded_bytes, transcript);

        set_status(VC_WAITING_REPLY, "agent reply");
        char *agent_text = NULL;
        err = doubao_agent_chat(transcript, &agent_text);
        if (err != ESP_OK || !agent_text || !agent_text[0]) {
            free(transcript);
            free(agent_text);
            set_status(VC_ERROR, "agent fail");
            continue;
        }
        if (s_turn_cb) s_turn_cb(transcript, agent_text, s_turn_ctx);

        set_status(VC_WAITING_REPLY, "tts connect");
        err = doubao_tts_play_text(agent_text, voice_tts_status, NULL);
        free(transcript);
        free(agent_text);
        if (err != ESP_OK) {
            set_status(VC_ERROR, "tts fail");
            continue;
        }

        set_status(VC_DONE, "voice done");
        mem_log("voice done");
    }
}

esp_err_t voice_chat_stop_and_process(void) {
    if (s_status != VC_RECORDING) {
        set_status(VC_ERROR, "not recording");
        return ESP_ERR_INVALID_STATE;
    }
    if (!s_worker_task) {
        set_status(VC_ERROR, "worker missing");
        return ESP_ERR_INVALID_STATE;
    }
    set_status(VC_UPLOADING, "asr final...");
    xTaskNotifyGive(s_worker_task);
    return ESP_OK;
}
