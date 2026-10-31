#include "radio_player.h"

#include "audio_io.h"
#include "radio_stations.h"
#include "mem_log.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_audio_dec_default.h>
#include <esp_audio_simple_dec.h>
#include <esp_audio_simple_dec_default.h>
#include <esp_http_client.h>
#include <esp_crt_bundle.h>
#include <esp_log.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>

#define RADIO_TASK_STACK_BYTES (24 * 1024)
#define RADIO_TASK_PRIORITY 4
#define RADIO_TASK_CORE 0
#define RADIO_INPUT_BYTES 2048
#define RADIO_OUTPUT_BYTES 8192
#define RADIO_PREFILL_BYTES (16 * 1024)
#define RADIO_HTTP_TIMEOUT_MS 1500
#define RADIO_WRITE_TIMEOUT_MS 2000
#define RADIO_SPECTRUM_INTERVAL_MS 80
#define RADIO_SPECTRUM_SAMPLES 128
#define RADIO_TWO_PI 6.28318530717958647692f

static const char *TAG = "radio_player";

static TaskHandle_t s_task;
static volatile bool s_stop_requested;
static radio_player_snapshot_t s_snapshot = {
    .state = RADIO_PLAYER_IDLE,
    .station_index = 0,
    .volume_level = 3,
    .message = "未播放",
};
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static int64_t s_last_spectrum_ms;
static float s_spectrum_peak;

static int64_t now_ms(void) {
    return esp_timer_get_time() / 1000;
}

static void set_state(radio_player_state_t state, const char *message) {
    taskENTER_CRITICAL(&s_lock);
    s_snapshot.state = state;
    snprintf(s_snapshot.message, sizeof(s_snapshot.message), "%s",
             message ? message : "");
    ++s_snapshot.generation;
    taskEXIT_CRITICAL(&s_lock);
}

static void set_format(const esp_audio_simple_dec_info_t *info) {
    taskENTER_CRITICAL(&s_lock);
    s_snapshot.sample_rate = info->sample_rate;
    s_snapshot.channels = info->channel;
    s_snapshot.bitrate = info->bitrate;
    ++s_snapshot.generation;
    taskEXIT_CRITICAL(&s_lock);
}

static void finish_task(void) {
    taskENTER_CRITICAL(&s_lock);
    s_task = NULL;
    ++s_snapshot.generation;
    taskEXIT_CRITICAL(&s_lock);
    vTaskDelete(NULL);
}

static void process_pcm(int16_t *samples, size_t size, uint8_t channels) {
    static const uint8_t gain_percent[] = {20, 40, 60, 80, 100};
    uint8_t volume;
    taskENTER_CRITICAL(&s_lock);
    volume = s_snapshot.volume_level;
    taskEXIT_CRITICAL(&s_lock);

    size_t sample_count = size / sizeof(*samples);
    int gain = gain_percent[volume - RADIO_VOLUME_MIN];
    if (gain < 100) {
        for (size_t i = 0; i < sample_count; ++i) {
            samples[i] = (int16_t)(((int32_t)samples[i] * gain) / 100);
        }
    }

    int64_t current_ms = now_ms();
    size_t frame_count = sample_count / channels;
    if (current_ms - s_last_spectrum_ms < RADIO_SPECTRUM_INTERVAL_MS ||
        frame_count < RADIO_SPECTRUM_SAMPLES) {
        return;
    }
    s_last_spectrum_ms = current_ms;

    static const uint8_t spectrum_bins[RADIO_SPECTRUM_BANDS] = {
        1, 2, 3, 4, 5, 7, 9, 12, 16, 22,
    };
    float values[RADIO_SPECTRUM_BANDS];
    float frame_peak = 0.0f;
    for (size_t band = 0; band < RADIO_SPECTRUM_BANDS; ++band) {
        float coefficient =
            2.0f * cosf(RADIO_TWO_PI * spectrum_bins[band] /
                        RADIO_SPECTRUM_SAMPLES);
        float q0 = 0.0f;
        float q1 = 0.0f;
        float q2 = 0.0f;
        for (size_t frame = 0; frame < RADIO_SPECTRUM_SAMPLES; ++frame) {
            int32_t mono = samples[frame * channels];
            if (channels == 2) {
                mono = (mono + samples[frame * channels + 1]) / 2;
            }
            q0 = (float)mono + coefficient * q1 - q2;
            q2 = q1;
            q1 = q0;
        }
        float power = q1 * q1 + q2 * q2 - coefficient * q1 * q2;
        values[band] = sqrtf(fmaxf(power, 0.0f)) /
                       RADIO_SPECTRUM_SAMPLES;
        if (values[band] > frame_peak) frame_peak = values[band];
    }

    s_spectrum_peak = fmaxf(frame_peak, s_spectrum_peak * 0.85f);
    taskENTER_CRITICAL(&s_lock);
    for (size_t band = 0; band < RADIO_SPECTRUM_BANDS; ++band) {
        int level = 0;
        if (s_spectrum_peak >= 80.0f) {
            float ratio = values[band] / s_spectrum_peak;
            level = (int)(sqrtf(fminf(ratio, 1.0f)) * 100.0f);
        }
        int previous = s_snapshot.spectrum[band];
        int smoothed = level > previous
                           ? (previous + level * 2) / 3
                           : previous > 8 ? previous - 8 : 0;
        s_snapshot.spectrum[band] = (uint8_t)smoothed;
    }
    ++s_snapshot.generation;
    taskEXIT_CRITICAL(&s_lock);
}

static esp_err_t push_pcm(uint8_t *data, size_t size, uint8_t channels,
                          bool *playback_started, size_t *prefill_bytes) {
    process_pcm((int16_t *)data, size, channels);
    size_t offset = 0;
    while (offset < size && !s_stop_requested) {
        size_t written = audio_io_write_speaker(
            data + offset, size - offset, RADIO_WRITE_TIMEOUT_MS);
        if (written == 0) return ESP_ERR_TIMEOUT;
        offset += written;
        if (!*playback_started) {
            *prefill_bytes += written;
            if (*prefill_bytes >= RADIO_PREFILL_BYTES) {
                mem_log("radio before I2S start");
                esp_err_t err = audio_io_start_playback(2000);
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "audio start failed: %s", esp_err_to_name(err));
                    return err;
                }
                *playback_started = true;
                set_state(RADIO_PLAYER_PLAYING, "正在播放");
            }
        }
    }
    return s_stop_requested ? ESP_ERR_INVALID_STATE : ESP_OK;
}

static esp_err_t decode_stream(esp_http_client_handle_t client) {
    uint8_t *input = heap_caps_malloc(RADIO_INPUT_BYTES,
                                      MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    uint8_t *output = heap_caps_malloc(RADIO_OUTPUT_BYTES,
                                       MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!input || !output) {
        free(input);
        free(output);
        return ESP_ERR_NO_MEM;
    }

    esp_audio_simple_dec_handle_t decoder = NULL;
    esp_audio_simple_dec_cfg_t decoder_cfg = {
        .dec_type = ESP_AUDIO_SIMPLE_DEC_TYPE_MP3,
        .dec_cfg = NULL,
        .cfg_size = 0,
        .use_frame_dec = false,
    };
    esp_audio_err_t decoder_err =
        esp_audio_simple_dec_open(&decoder_cfg, &decoder);
    if (decoder_err != ESP_AUDIO_ERR_OK) {
        ESP_LOGE(TAG, "decoder open failed: %d", decoder_err);
        free(input);
        free(output);
        return ESP_FAIL;
    }

    bool format_ready = false;
    bool playback_started = false;
    size_t prefill_bytes = 0;
    esp_err_t result = ESP_OK;
    set_state(RADIO_PLAYER_BUFFERING, "正在缓冲");

    while (!s_stop_requested) {
        int read = esp_http_client_read(client, (char *)input,
                                        RADIO_INPUT_BYTES);
        if (read == -ESP_ERR_HTTP_EAGAIN) continue;
        if (read < 0) {
            result = ESP_FAIL;
            break;
        }
        if (read == 0) {
            result = ESP_ERR_INVALID_RESPONSE;
            break;
        }

        esp_audio_simple_dec_raw_t raw = {
            .buffer = input,
            .len = (uint32_t)read,
            .eos = false,
        };
        while (raw.len > 0 && !s_stop_requested) {
            esp_audio_simple_dec_out_t frame = {
                .buffer = output,
                .len = RADIO_OUTPUT_BYTES,
            };
            decoder_err = esp_audio_simple_dec_process(decoder, &raw, &frame);
            if (decoder_err == ESP_AUDIO_ERR_BUFF_NOT_ENOUGH) {
                ESP_LOGE(TAG, "decoder output needs %u bytes",
                         (unsigned)frame.needed_size);
                result = ESP_ERR_NO_MEM;
                break;
            }
            if (decoder_err != ESP_AUDIO_ERR_OK) {
                ESP_LOGE(TAG, "MP3 decode failed: %d", decoder_err);
                result = ESP_FAIL;
                break;
            }
            if (frame.decoded_size > 0) {
                if (!format_ready) {
                    esp_audio_simple_dec_info_t info = {0};
                    decoder_err =
                        esp_audio_simple_dec_get_info(decoder, &info);
                    if (decoder_err != ESP_AUDIO_ERR_OK ||
                        info.bits_per_sample != 16 ||
                        (info.channel != 1 && info.channel != 2)) {
                        ESP_LOGE(TAG, "unsupported PCM format");
                        result = ESP_ERR_NOT_SUPPORTED;
                        break;
                    }
                    result = audio_io_prepare_speaker(info.sample_rate,
                                                      info.channel);
                    if (result != ESP_OK) break;
                    set_format(&info);
                    format_ready = true;
                }
                result = push_pcm(frame.buffer, frame.decoded_size,
                                  s_snapshot.channels,
                                  &playback_started, &prefill_bytes);
                if (result != ESP_OK) break;
            }
            if (raw.consumed == 0) {
                if (frame.decoded_size == 0) {
                    result = ESP_ERR_INVALID_RESPONSE;
                }
                break;
            }
            raw.buffer += raw.consumed;
            raw.len -= raw.consumed;
        }
        if (result != ESP_OK) break;
    }

    if (!playback_started && prefill_bytes > 0 && result == ESP_OK) {
        result = audio_io_start_playback(2000);
        playback_started = result == ESP_OK;
    }
    if (playback_started) {
        if (s_stop_requested) {
            audio_io_abort(2000);
        } else {
            audio_io_stop_playback(3000);
        }
    }
    esp_audio_simple_dec_close(decoder);
    free(input);
    free(output);
    return result;
}

static void radio_task(void *arg) {
    size_t station_index = (size_t)(uintptr_t)arg;
    const radio_station_t *station = radio_station_get(station_index);
    esp_err_t result = ESP_FAIL;

    esp_http_client_config_t config = {
        .url = station->url,
        .timeout_ms = RADIO_HTTP_TIMEOUT_MS,
        .buffer_size = RADIO_INPUT_BYTES,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .keep_alive_enable = true,
        .disable_auto_redirect = false,
        .max_redirection_count = 4,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        set_state(RADIO_PLAYER_ERROR, "HTTP 初始化失败");
        finish_task();
        return;
    }

    esp_http_client_set_method(client, HTTP_METHOD_GET);
    esp_http_client_set_header(client, "Accept", "audio/mpeg");
    esp_http_client_set_header(client, "Icy-MetaData", "0");
    esp_http_client_set_header(client, "User-Agent", "Minibox/1.0");

    set_state(RADIO_PLAYER_CONNECTING, "正在连接");
    mem_log("radio before HTTP");
    result = esp_http_client_open(client, 0);
    if (result == ESP_OK) {
        int64_t content_length = esp_http_client_fetch_headers(client);
        int status = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "%s HTTP %d length=%lld", station->name, status,
                 (long long)content_length);
        if (status == 200 || status == 206) {
            mem_log("radio before decoder");
            result = decode_stream(client);
        } else {
            result = ESP_ERR_INVALID_RESPONSE;
        }
    }

    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    mem_log("radio after cleanup");
    if (s_stop_requested) {
        set_state(RADIO_PLAYER_IDLE, "已停止");
    } else if (result == ESP_OK) {
        set_state(RADIO_PLAYER_IDLE, "播放结束");
    } else {
        ESP_LOGE(TAG, "stream failed: %s", esp_err_to_name(result));
        const char *message = result == ESP_ERR_NO_MEM ? "内存不足" :
                              result == ESP_ERR_TIMEOUT ? "读取或播放超时" :
                              result == ESP_ERR_NOT_SUPPORTED ? "音频格式不支持" :
                              "连接或解码失败";
        set_state(RADIO_PLAYER_ERROR, message);
    }
    finish_task();
}

esp_err_t radio_player_init(void) {
    esp_audio_err_t err = esp_audio_dec_register_default();
    if (err != ESP_AUDIO_ERR_OK) return ESP_FAIL;
    err = esp_audio_simple_dec_register_default();
    return err == ESP_AUDIO_ERR_OK ? ESP_OK : ESP_FAIL;
}

esp_err_t radio_player_start(size_t station_index) {
    if (!radio_station_get(station_index)) return ESP_ERR_INVALID_ARG;
    if (radio_player_busy()) return ESP_ERR_INVALID_STATE;
    if (audio_io_phase() != AUDIO_PHASE_IDLE) return ESP_ERR_INVALID_STATE;

    taskENTER_CRITICAL(&s_lock);
    s_stop_requested = false;
    s_snapshot.station_index = station_index;
    s_snapshot.sample_rate = 0;
    s_snapshot.channels = 0;
    s_snapshot.bitrate = 0;
    memset(s_snapshot.spectrum, 0, sizeof(s_snapshot.spectrum));
    s_last_spectrum_ms = 0;
    s_spectrum_peak = 0.0f;
    s_snapshot.state = RADIO_PLAYER_CONNECTING;
    snprintf(s_snapshot.message, sizeof(s_snapshot.message), "准备连接");
    ++s_snapshot.generation;
    taskEXIT_CRITICAL(&s_lock);

    BaseType_t created = xTaskCreatePinnedToCore(
        radio_task, "radio_player", RADIO_TASK_STACK_BYTES,
        (void *)(uintptr_t)station_index, RADIO_TASK_PRIORITY, &s_task,
        RADIO_TASK_CORE);
    if (created != pdPASS) {
        set_state(RADIO_PLAYER_ERROR, "播放器任务创建失败");
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t radio_player_stop(uint32_t timeout_ms) {
    if (!radio_player_busy()) {
        if (s_snapshot.state != RADIO_PLAYER_ERROR) {
            set_state(RADIO_PLAYER_IDLE, "未播放");
        }
        return ESP_OK;
    }
    s_stop_requested = true;
    set_state(RADIO_PLAYER_STOPPING, "正在停止");
    int64_t start = now_ms();
    while (s_task) {
        if ((uint32_t)(now_ms() - start) >= timeout_ms) {
            return ESP_ERR_TIMEOUT;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return ESP_OK;
}

bool radio_player_busy(void) {
    return s_task != NULL;
}

void radio_player_get_snapshot(radio_player_snapshot_t *snapshot) {
    if (!snapshot) return;
    taskENTER_CRITICAL(&s_lock);
    *snapshot = s_snapshot;
    taskEXIT_CRITICAL(&s_lock);
}

void radio_player_set_volume(uint8_t level) {
    if (level < RADIO_VOLUME_MIN) level = RADIO_VOLUME_MIN;
    if (level > RADIO_VOLUME_MAX) level = RADIO_VOLUME_MAX;
    taskENTER_CRITICAL(&s_lock);
    if (s_snapshot.volume_level != level) {
        s_snapshot.volume_level = level;
        ++s_snapshot.generation;
    }
    taskEXIT_CRITICAL(&s_lock);
}
