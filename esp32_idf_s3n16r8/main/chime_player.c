#include "chime_player.h"
#include "audio_io.h"
#include "doubao_agent_client.h"
#include "doubao_tts_player.h"

#include <stdlib.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>

static const char *TAG = "chime";

#define CHIME_TASK_CORE 0
#define CHIME_TASK_PRIO 4

static TaskHandle_t s_worker_task;
static volatile chime_status_t s_status = CHIME_IDLE;
static char s_msg[48] = "chime idle";
static char s_time_arg[16];
static char s_fixed_text[256];
static bool s_text_only;
static chime_player_status_cb_t s_status_cb;
static void *s_status_ctx;
static chime_player_text_cb_t s_text_cb;
static void *s_text_ctx;

static void set_status(chime_status_t st, const char *msg) {
    s_status = st;
    if (msg) {
        strncpy(s_msg, msg, sizeof(s_msg) - 1);
        s_msg[sizeof(s_msg) - 1] = 0;
    }
    if (s_status_cb) s_status_cb(s_status_ctx);
}

static void chime_tts_status(void *ctx, const char *msg) {
    (void)ctx;
    if (!msg) return;
    if (strstr(msg, "playing")) {
        set_status(CHIME_PLAYING, "playing chime");
    } else if (strstr(msg, "drain")) {
        set_status(CHIME_DRAINING, "drain chime");
    } else {
        set_status(CHIME_WAITING_REPLY, msg);
    }
}

static void chime_worker(void *arg) {
    (void)arg;
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (s_status != CHIME_CONNECTING) continue;

        if (s_text_only) {
            set_status(CHIME_WAITING_REPLY, "tts speak");
            esp_err_t err = doubao_tts_play_text(s_fixed_text,
                                                 chime_tts_status, NULL);
            s_text_only = false;
            s_fixed_text[0] = 0;
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "tts speak err=%s", esp_err_to_name(err));
                set_status(CHIME_ERROR, "tts fail");
                continue;
            }
            set_status(CHIME_DONE, "tts done");
            continue;
        }

        set_status(CHIME_WAITING_REPLY, "agent chime");
        char *text = NULL;
        esp_err_t err = doubao_agent_chime(s_time_arg, &text);
        if (err != ESP_OK || !text || !text[0]) {
            ESP_LOGE(TAG, "agent chime err=%s", esp_err_to_name(err));
            free(text);
            set_status(CHIME_ERROR, "chime fail");
            continue;
        }

        ESP_LOGI(TAG, "chime text %u bytes: %s", (unsigned)strlen(text), text);
        if (s_text_cb) s_text_cb(text, s_text_ctx);
        set_status(CHIME_WAITING_REPLY, "tts chime");
        err = doubao_tts_play_text(text, chime_tts_status, NULL);
        free(text);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "tts err=%s", esp_err_to_name(err));
            set_status(CHIME_ERROR, "chime tts fail");
            continue;
        }
        set_status(CHIME_DONE, "chime done");
    }
}

esp_err_t chime_player_init(void) {
    if (s_worker_task) return ESP_OK;
    BaseType_t ok = xTaskCreatePinnedToCore(chime_worker, "chime_worker",
                                            6144, NULL, CHIME_TASK_PRIO,
                                            &s_worker_task, CHIME_TASK_CORE);
    if (ok != pdPASS) return ESP_FAIL;
    set_status(CHIME_IDLE, "chime ready");
    return ESP_OK;
}

bool chime_player_busy(void) {
    switch (s_status) {
    case CHIME_CONNECTING:
    case CHIME_WAITING_REPLY:
    case CHIME_PLAYING:
    case CHIME_DRAINING:
        return true;
    default:
        return false;
    }
}

chime_status_t chime_player_status(void) { return s_status; }
const char *chime_player_message(void) { return s_msg; }

esp_err_t chime_player_start(const char *time_arg) {
    if (!s_worker_task) return ESP_ERR_INVALID_STATE;
    if (chime_player_busy()) return ESP_ERR_INVALID_STATE;
    if (audio_io_phase() != AUDIO_PHASE_IDLE) return ESP_ERR_INVALID_STATE;
    if (time_arg) {
        strncpy(s_time_arg, time_arg, sizeof(s_time_arg) - 1);
        s_time_arg[sizeof(s_time_arg) - 1] = 0;
    } else {
        s_time_arg[0] = 0;
    }
    s_text_only = false;
    s_fixed_text[0] = 0;
    set_status(CHIME_CONNECTING, "start chime");
    xTaskNotifyGive(s_worker_task);
    return ESP_OK;
}

esp_err_t chime_player_start_text(const char *text) {
    if (!s_worker_task) return ESP_ERR_INVALID_STATE;
    if (chime_player_busy()) return ESP_ERR_INVALID_STATE;
    if (audio_io_phase() != AUDIO_PHASE_IDLE) return ESP_ERR_INVALID_STATE;
    if (!text || !text[0]) return ESP_ERR_INVALID_ARG;
    strncpy(s_fixed_text, text, sizeof(s_fixed_text) - 1);
    s_fixed_text[sizeof(s_fixed_text) - 1] = 0;
    s_text_only = true;
    s_time_arg[0] = 0;
    set_status(CHIME_CONNECTING, "start tts");
    xTaskNotifyGive(s_worker_task);
    return ESP_OK;
}

void chime_player_set_status_cb(chime_player_status_cb_t cb, void *ctx) {
    s_status_cb = cb;
    s_status_ctx = ctx;
}

void chime_player_set_text_cb(chime_player_text_cb_t cb, void *ctx) {
    s_text_cb = cb;
    s_text_ctx = ctx;
}
