#include "doubao_tts_player.h"
#include "app_config.h"
#include "app_secret_store.h"
#include "audio_io.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <esp_check.h>
#include <esp_http_client.h>
#include <esp_crt_bundle.h>
#include <esp_log.h>
#include <esp_random.h>
#include <mbedtls/base64.h>

static const char *TAG = "doubao_tts";

#define TTS_HTTP_TIMEOUT_MS   60000
#define TTS_PUSH_TIMEOUT_MS   2000
#define TTS_PREFILL_BYTES     (32 * 1024)

typedef enum {
    PARSE_SEARCH_KEY = 0,
    PARSE_AFTER_KEY,
    PARSE_BEFORE_VALUE,
    PARSE_IN_DATA,
    PARSE_SKIP_NULL,
} tts_parse_state_t;

typedef struct {
    doubao_tts_status_cb_t status_cb;
    void *status_ctx;
    tts_parse_state_t state;
    size_t key_match;
    size_t null_match;
    char b64_quad[4];
    size_t b64_len;
    bool escape;
    bool playback_started;
    bool stream_error;
    size_t total_pcm;
    int status_code;
} tts_stream_ctx_t;

static void report_status(tts_stream_ctx_t *ctx, const char *msg) {
    if (ctx->status_cb) ctx->status_cb(ctx->status_ctx, msg);
}

static void make_connect_id(char out[37]) {
    uint32_t r[4] = { esp_random(), esp_random(), esp_random(), esp_random() };
    snprintf(out, 37, "%08x-%04x-%04x-%04x-%04x%08x",
             (unsigned)r[0],
             (unsigned)(r[1] >> 16),
             (unsigned)(r[1] & 0xffff),
             (unsigned)(r[2] >> 16),
             (unsigned)(r[2] & 0xffff),
             (unsigned)r[3]);
}

static char *json_escape_alloc(const char *src) {
    size_t cap = 1;
    for (const unsigned char *p = (const unsigned char *)src; p && *p; ++p) {
        switch (*p) {
        case '"':
        case '\\':
        case '\n':
        case '\r':
        case '\t':
            cap += 2;
            break;
        default:
            cap += (*p < 0x20) ? 6 : 1;
            break;
        }
    }
    char *out = malloc(cap);
    if (!out) return NULL;

    char *w = out;
    for (const unsigned char *p = (const unsigned char *)src; p && *p; ++p) {
        switch (*p) {
        case '"':  *w++ = '\\'; *w++ = '"';  break;
        case '\\': *w++ = '\\'; *w++ = '\\'; break;
        case '\n': *w++ = '\\'; *w++ = 'n';  break;
        case '\r': *w++ = '\\'; *w++ = 'r';  break;
        case '\t': *w++ = '\\'; *w++ = 't';  break;
        default:
            if (*p < 0x20) {
                static const char hex[] = "0123456789abcdef";
                *w++ = '\\'; *w++ = 'u'; *w++ = '0'; *w++ = '0';
                *w++ = hex[*p >> 4]; *w++ = hex[*p & 0x0f];
            } else {
                *w++ = (char)*p;
            }
            break;
        }
    }
    *w = 0;
    return out;
}

static esp_err_t maybe_start_playback(tts_stream_ctx_t *ctx, bool force) {
    if (!ctx->playback_started && ctx->total_pcm > 0 &&
        (force || ctx->total_pcm >= TTS_PREFILL_BYTES)) {
        if (audio_io_start_playback(1000) != ESP_OK) return ESP_FAIL;
        ctx->playback_started = true;
        report_status(ctx, "playing tts");
    }
    return ESP_OK;
}

static esp_err_t push_pcm(tts_stream_ctx_t *ctx, const uint8_t *pcm, size_t len) {
    size_t off = 0;
    while (off < len) {
        size_t pushed = audio_io_write_speaker(pcm + off, len - off,
                                               TTS_PUSH_TIMEOUT_MS);
        if (pushed == 0) {
            if (!ctx->playback_started && ctx->total_pcm > 0) {
                ESP_RETURN_ON_ERROR(maybe_start_playback(ctx, true), TAG, "start playback");
                continue;
            }
            return ESP_FAIL;
        }
        off += pushed;
        ctx->total_pcm += pushed;
        ESP_RETURN_ON_ERROR(maybe_start_playback(ctx, false), TAG, "start playback");
    }
    return ESP_OK;
}

static esp_err_t decode_quad(tts_stream_ctx_t *ctx) {
    uint8_t pcm[3];
    size_t pcm_len = 0;
    int rc = mbedtls_base64_decode(pcm, sizeof(pcm), &pcm_len,
                                   (const unsigned char *)ctx->b64_quad, 4);
    if (rc != 0) {
        ESP_LOGE(TAG, "base64 decode rc=%d", rc);
        return ESP_FAIL;
    }
    ctx->b64_len = 0;
    return pcm_len > 0 ? push_pcm(ctx, pcm, pcm_len) : ESP_OK;
}

static esp_err_t feed_base64_char(tts_stream_ctx_t *ctx, char c) {
    if (isspace((unsigned char)c)) return ESP_OK;
    ctx->b64_quad[ctx->b64_len++] = c;
    if (ctx->b64_len == sizeof(ctx->b64_quad)) return decode_quad(ctx);
    return ESP_OK;
}

static esp_err_t finish_data_value(tts_stream_ctx_t *ctx) {
    if (ctx->b64_len != 0) {
        ESP_LOGE(TAG, "truncated base64 data len=%u", (unsigned)ctx->b64_len);
        ctx->b64_len = 0;
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t feed_tts_json(tts_stream_ctx_t *ctx, const char *data, size_t len) {
    static const char key[] = "\"data\"";
    static const char nullv[] = "null";

    for (size_t i = 0; i < len; ++i) {
        char c = data[i];
        switch (ctx->state) {
        case PARSE_SEARCH_KEY:
            if (c == key[ctx->key_match]) {
                ctx->key_match++;
                if (ctx->key_match == sizeof(key) - 1) {
                    ctx->key_match = 0;
                    ctx->state = PARSE_AFTER_KEY;
                }
            } else {
                ctx->key_match = (c == key[0]) ? 1 : 0;
            }
            break;
        case PARSE_AFTER_KEY:
            if (c == ':') {
                ctx->state = PARSE_BEFORE_VALUE;
            } else if (!isspace((unsigned char)c)) {
                ctx->state = PARSE_SEARCH_KEY;
            }
            break;
        case PARSE_BEFORE_VALUE:
            if (isspace((unsigned char)c)) break;
            if (c == '"') {
                ctx->state = PARSE_IN_DATA;
                ctx->escape = false;
                ctx->b64_len = 0;
            } else if (c == 'n') {
                ctx->state = PARSE_SKIP_NULL;
                ctx->null_match = 1;
            } else {
                ctx->state = PARSE_SEARCH_KEY;
            }
            break;
        case PARSE_IN_DATA:
            if (ctx->escape) {
                ctx->escape = false;
                ESP_RETURN_ON_ERROR(feed_base64_char(ctx, c), TAG, "base64 escaped char");
            } else if (c == '\\') {
                ctx->escape = true;
            } else if (c == '"') {
                ESP_RETURN_ON_ERROR(finish_data_value(ctx), TAG, "finish base64");
                ctx->state = PARSE_SEARCH_KEY;
            } else {
                ESP_RETURN_ON_ERROR(feed_base64_char(ctx, c), TAG, "base64 char");
            }
            break;
        case PARSE_SKIP_NULL:
            if (ctx->null_match < sizeof(nullv) - 1 &&
                c == nullv[ctx->null_match]) {
                ctx->null_match++;
                if (ctx->null_match == sizeof(nullv) - 1) {
                    ctx->state = PARSE_SEARCH_KEY;
                }
            } else {
                ctx->state = PARSE_SEARCH_KEY;
            }
            break;
        }
    }
    return ESP_OK;
}

static esp_err_t finish_tts_json(tts_stream_ctx_t *ctx) {
    if (ctx->state == PARSE_IN_DATA) return finish_data_value(ctx);
    return ESP_OK;
}

static esp_err_t tts_http_event(esp_http_client_event_t *e) {
    tts_stream_ctx_t *ctx = (tts_stream_ctx_t *)e->user_data;
    if (!ctx) return ESP_OK;

    switch (e->event_id) {
    case HTTP_EVENT_ON_CONNECTED:
        if (audio_io_prepare_speaker(APP_DOUBAO_TTS_SAMPLE_RATE, 1) != ESP_OK) {
            ctx->stream_error = true;
            return ESP_FAIL;
        }
        report_status(ctx, "wait tts");
        break;
    case HTTP_EVENT_ON_DATA:
        if (!e->data || e->data_len <= 0) break;
        ctx->status_code = esp_http_client_get_status_code(e->client);
        if (ctx->status_code != 200) break;
        if (feed_tts_json(ctx, (const char *)e->data, (size_t)e->data_len) != ESP_OK) {
            ctx->stream_error = true;
            return ESP_FAIL;
        }
        break;
    default:
        break;
    }
    return ESP_OK;
}

esp_err_t doubao_tts_play_text(const char *text,
                               doubao_tts_status_cb_t status_cb,
                               void *status_ctx) {
    if (!text || text[0] == 0) return ESP_ERR_INVALID_ARG;
    if (!app_secrets_ready()) return ESP_ERR_INVALID_STATE;

    char *escaped_text = json_escape_alloc(text);
    if (!escaped_text) return ESP_ERR_NO_MEM;

    const char *body_fmt =
        "{\"user\":{\"uid\":\"esp32-zhalarm\"},"
        "\"event\":200,"
        "\"req_params\":{\"text\":\"%s\",\"speaker\":\"%s\","
        "\"audio_params\":{\"format\":\"pcm\",\"sample_rate\":%d}}}";
    int body_len = snprintf(NULL, 0, body_fmt, escaped_text,
                            APP_DOUBAO_TTS_SPEAKER,
                            APP_DOUBAO_TTS_SAMPLE_RATE);
    if (body_len <= 0) {
        free(escaped_text);
        return ESP_FAIL;
    }
    char *body = malloc((size_t)body_len + 1);
    if (!body) {
        free(escaped_text);
        return ESP_ERR_NO_MEM;
    }
    snprintf(body, (size_t)body_len + 1, body_fmt, escaped_text,
             APP_DOUBAO_TTS_SPEAKER, APP_DOUBAO_TTS_SAMPLE_RATE);
    free(escaped_text);

    tts_stream_ctx_t ctx = {
        .status_cb = status_cb,
        .status_ctx = status_ctx,
    };
    char connect_id[37];
    make_connect_id(connect_id);

    esp_http_client_config_t cfg = {
        .url = APP_DOUBAO_TTS_URL,
        .timeout_ms = TTS_HTTP_TIMEOUT_MS,
        .event_handler = tts_http_event,
        .user_data = &ctx,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
    };
    esp_http_client_handle_t cli = esp_http_client_init(&cfg);
    if (!cli) {
        free(body);
        return ESP_FAIL;
    }
    esp_http_client_set_method(cli, HTTP_METHOD_POST);
    esp_http_client_set_header(cli, "X-Api-Key", app_secrets_tts_api_key());
    esp_http_client_set_header(cli, "X-Api-Resource-Id", APP_DOUBAO_TTS_RESOURCE_ID);
    esp_http_client_set_header(cli, "X-Api-Connect-Id", connect_id);
    esp_http_client_set_header(cli, "Content-Type", "application/json");
    esp_http_client_set_header(cli, "Accept", "application/json");
    esp_http_client_set_post_field(cli, body, body_len);

    report_status(&ctx, "tts connect");
    esp_err_t err = esp_http_client_perform(cli);
    int code = esp_http_client_get_status_code(cli);
    esp_http_client_cleanup(cli);
    free(body);

    esp_err_t finish_err = finish_tts_json(&ctx);
    if (err != ESP_OK || ctx.stream_error || finish_err != ESP_OK || code != 200) {
        if (ctx.playback_started) {
            if (audio_io_stop_playback(2000) != ESP_OK) audio_io_abort(2000);
        }
        ESP_LOGE(TAG, "perform err=%s code=%d pcm=%u",
                 esp_err_to_name(err), code, (unsigned)ctx.total_pcm);
        return err != ESP_OK ? err : ESP_FAIL;
    }

    ESP_RETURN_ON_ERROR(maybe_start_playback(&ctx, true), TAG, "start playback");
    if (!ctx.playback_started) return ESP_FAIL;

    report_status(&ctx, "drain tts");
    if (audio_io_stop_playback(8000) != ESP_OK) {
        audio_io_abort(2000);
        return ESP_FAIL;
    }
    ctx.playback_started = false;
    ESP_LOGI(TAG, "played %u bytes underrun=%u",
             (unsigned)ctx.total_pcm, (unsigned)audio_io_spk_underrun());
    return ESP_OK;
}
