#include "doubao_asr_client.h"
#include "app_config.h"
#include "app_secret_store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <esp_log.h>
#include <esp_random.h>
#include <esp_timer.h>
#include <esp_tls.h>
#include <esp_crt_bundle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <mbedtls/base64.h>

static const char *TAG = "doubao_asr";

#define ASR_HOST "openspeech.bytedance.com"
#define ASR_PATH "/api/v3/sauc/bigmodel_nostream"
#define ASR_PORT 443
#define ASR_CONNECT_TIMEOUT_MS 10000
#define ASR_READ_TIMEOUT_MS 30000
#define ASR_FRAME_MAX_BYTES 8192
#define ASR_TEXT_MAX_BYTES 2048

#define ASR_PROTOCOL_VERSION 0x1
#define ASR_HEADER_SIZE_WORDS 0x1
#define ASR_SERIALIZATION_NONE 0x0
#define ASR_SERIALIZATION_JSON 0x1
#define ASR_COMPRESSION_NONE 0x0
#define ASR_MSG_FULL_CLIENT_REQUEST 0x1
#define ASR_MSG_AUDIO_ONLY_REQUEST 0x2
#define ASR_MSG_FULL_SERVER_RESPONSE 0x9
#define ASR_MSG_ERROR 0xf
#define ASR_FLAG_NONE 0x0
#define ASR_FLAG_POS_SEQUENCE 0x1
#define ASR_FLAG_LAST_NO_SEQUENCE 0x2
#define ASR_FLAG_LAST_NEG_SEQUENCE 0x3

struct doubao_asr_session {
    esp_tls_t *tls;
    char *latest_text;
    bool final_sent;
};

static inline int64_t now_ms(void) { return esp_timer_get_time() / 1000; }

static void make_uuid(char out[37]) {
    uint32_t r[4] = { esp_random(), esp_random(), esp_random(), esp_random() };
    snprintf(out, 37, "%08x-%04x-%04x-%04x-%04x%08x",
             (unsigned)r[0],
             (unsigned)(r[1] >> 16),
             (unsigned)(r[1] & 0xffff),
             (unsigned)(r[2] >> 16),
             (unsigned)(r[2] & 0xffff),
             (unsigned)r[3]);
}

static esp_err_t make_ws_key(char out[25]) {
    uint8_t raw[16];
    for (size_t i = 0; i < sizeof(raw); i += 4) {
        uint32_t r = esp_random();
        memcpy(raw + i, &r, 4);
    }
    size_t olen = 0;
    int rc = mbedtls_base64_encode((unsigned char *)out, 25, &olen, raw, sizeof(raw));
    if (rc != 0 || olen >= 25) return ESP_FAIL;
    out[olen] = 0;
    return ESP_OK;
}

static int tls_write_all(esp_tls_t *tls, const void *buf, size_t len) {
    const uint8_t *p = (const uint8_t *)buf;
    size_t off = 0;
    while (off < len) {
        ssize_t w = esp_tls_conn_write(tls, p + off, len - off);
        if (w <= 0) {
            if (w == ESP_TLS_ERR_SSL_WANT_READ ||
                w == ESP_TLS_ERR_SSL_WANT_WRITE) {
                vTaskDelay(1);
                continue;
            }
            return -1;
        }
        off += (size_t)w;
    }
    return 0;
}

static int tls_read_exact(esp_tls_t *tls, uint8_t *dst, size_t len, uint32_t timeout_ms) {
    size_t off = 0;
    int64_t t0 = now_ms();
    while (off < len && (uint32_t)(now_ms() - t0) < timeout_ms) {
        ssize_t r = esp_tls_conn_read(tls, dst + off, len - off);
        if (r > 0) {
            off += (size_t)r;
            t0 = now_ms();
            continue;
        }
        if (r == 0) return -1;
        if (r == ESP_TLS_ERR_SSL_WANT_READ ||
            r == ESP_TLS_ERR_SSL_WANT_WRITE) {
            vTaskDelay(1);
            continue;
        }
        return -1;
    }
    return off == len ? 0 : -1;
}

static int tls_read_line(esp_tls_t *tls, char *line, size_t cap, uint32_t timeout_ms) {
    size_t n = 0;
    int64_t t0 = now_ms();
    while ((uint32_t)(now_ms() - t0) < timeout_ms) {
        uint8_t c = 0;
        ssize_t r = esp_tls_conn_read(tls, &c, 1);
        if (r > 0) {
            if (c == '\r') continue;
            if (c == '\n') {
                line[n] = 0;
                return (int)n;
            }
            if (n + 1 < cap) line[n++] = (char)c;
            t0 = now_ms();
            continue;
        }
        if (r == 0) break;
        if (r == ESP_TLS_ERR_SSL_WANT_READ ||
            r == ESP_TLS_ERR_SSL_WANT_WRITE) {
            vTaskDelay(1);
            continue;
        }
        break;
    }
    line[n] = 0;
    return -1;
}

static esp_err_t websocket_send_frame(esp_tls_t *tls, uint8_t opcode,
                                      const uint8_t *payload, size_t len) {
    size_t hdr_len = 2;
    if (len > 125 && len <= 0xffff) hdr_len += 2;
    else if (len > 0xffff) hdr_len += 8;
    hdr_len += 4;

    uint8_t *frame = malloc(hdr_len + len);
    if (!frame) return ESP_ERR_NO_MEM;

    size_t off = 0;
    frame[off++] = 0x80 | (opcode & 0x0f);
    if (len <= 125) {
        frame[off++] = 0x80 | (uint8_t)len;
    } else if (len <= 0xffff) {
        frame[off++] = 0x80 | 126;
        frame[off++] = (uint8_t)(len >> 8);
        frame[off++] = (uint8_t)len;
    } else {
        frame[off++] = 0x80 | 127;
        for (int i = 7; i >= 0; --i) frame[off++] = (uint8_t)(((uint64_t)len) >> (8 * i));
    }

    uint32_t mask = esp_random();
    uint8_t mask_bytes[4];
    memcpy(mask_bytes, &mask, sizeof(mask_bytes));
    memcpy(frame + off, mask_bytes, sizeof(mask_bytes));
    off += sizeof(mask_bytes);
    for (size_t i = 0; i < len; ++i) {
        frame[off + i] = payload[i] ^ mask_bytes[i & 3];
    }

    int rc = tls_write_all(tls, frame, off + len);
    free(frame);
    return rc == 0 ? ESP_OK : ESP_FAIL;
}

static esp_err_t websocket_read_frame(esp_tls_t *tls, uint8_t **out_payload,
                                      size_t *out_len, uint8_t *out_opcode) {
    *out_payload = NULL;
    *out_len = 0;
    *out_opcode = 0;

    for (;;) {
        uint8_t hdr[2];
        if (tls_read_exact(tls, hdr, sizeof(hdr), ASR_READ_TIMEOUT_MS) != 0) return ESP_FAIL;
        uint8_t opcode = hdr[0] & 0x0f;
        bool masked = (hdr[1] & 0x80) != 0;
        uint64_t len = hdr[1] & 0x7f;
        if (len == 126) {
            uint8_t ext[2];
            if (tls_read_exact(tls, ext, sizeof(ext), ASR_READ_TIMEOUT_MS) != 0) return ESP_FAIL;
            len = ((uint64_t)ext[0] << 8) | ext[1];
        } else if (len == 127) {
            uint8_t ext[8];
            if (tls_read_exact(tls, ext, sizeof(ext), ASR_READ_TIMEOUT_MS) != 0) return ESP_FAIL;
            len = 0;
            for (int i = 0; i < 8; ++i) len = (len << 8) | ext[i];
        }
        if (len > ASR_FRAME_MAX_BYTES) {
            ESP_LOGE(TAG, "websocket frame too large: %llu", (unsigned long long)len);
            return ESP_ERR_NO_MEM;
        }

        uint8_t mask[4] = {0};
        if (masked && tls_read_exact(tls, mask, sizeof(mask), ASR_READ_TIMEOUT_MS) != 0) {
            return ESP_FAIL;
        }

        uint8_t *payload = NULL;
        if (len > 0) {
            payload = malloc((size_t)len + 1);
            if (!payload) return ESP_ERR_NO_MEM;
            if (tls_read_exact(tls, payload, (size_t)len, ASR_READ_TIMEOUT_MS) != 0) {
                free(payload);
                return ESP_FAIL;
            }
            for (size_t i = 0; masked && i < (size_t)len; ++i) payload[i] ^= mask[i & 3];
            payload[len] = 0;
        }

        if (opcode == 0x9) {
            websocket_send_frame(tls, 0xA, payload, (size_t)len);
            free(payload);
            continue;
        }
        if (opcode == 0x8) {
            free(payload);
            return ESP_FAIL;
        }

        *out_payload = payload;
        *out_len = (size_t)len;
        *out_opcode = opcode;
        return ESP_OK;
    }
}

static char *json_unescape_alloc(const char *src, size_t len) {
    char *out = malloc(len + 1);
    if (!out) return NULL;
    char *w = out;
    for (size_t i = 0; i < len; ++i) {
        char c = src[i];
        if (c != '\\' || i + 1 >= len) {
            *w++ = c;
            continue;
        }
        c = src[++i];
        switch (c) {
        case '"':  *w++ = '"';  break;
        case '\\': *w++ = '\\'; break;
        case '/':  *w++ = '/';  break;
        case 'b':  *w++ = '\b'; break;
        case 'f':  *w++ = '\f'; break;
        case 'n':  *w++ = '\n'; break;
        case 'r':  *w++ = '\r'; break;
        case 't':  *w++ = '\t'; break;
        case 'u':
            if (i + 4 < len) i += 4;
            break;
        default:
            *w++ = c;
            break;
        }
    }
    *w = 0;
    return out;
}

static char *extract_result_text(const char *json) {
    const char *p = strstr(json, "\"result\"");
    if (!p) return NULL;
    p = strstr(p, "\"text\"");
    if (!p) return NULL;
    p += strlen("\"text\"");
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    if (*p != ':') return NULL;
    p++;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    if (*p != '"') return NULL;
    p++;

    const char *start = p;
    bool escape = false;
    while (*p) {
        if (escape) {
            escape = false;
        } else if (*p == '\\') {
            escape = true;
        } else if (*p == '"') {
            return json_unescape_alloc(start, (size_t)(p - start));
        }
        p++;
    }
    return NULL;
}

static esp_err_t parse_asr_response(doubao_asr_session_t *session,
                                    const uint8_t *payload,
                                    size_t len) {
    if (len < 8) return ESP_FAIL;
    uint8_t message_type = payload[1] >> 4;
    uint8_t flags = payload[1] & 0x0f;
    uint8_t serialization = payload[2] >> 4;
    uint8_t compression = payload[2] & 0x0f;
    size_t offset = (payload[0] & 0x0f) * 4;

    if (flags == ASR_FLAG_POS_SEQUENCE || flags == ASR_FLAG_LAST_NEG_SEQUENCE) {
        offset += 4;
    }
    if (offset > len) return ESP_FAIL;

    if (message_type == ASR_MSG_ERROR) {
        if (len < offset + 8) return ESP_FAIL;
        uint32_t code = ((uint32_t)payload[offset] << 24) |
                        ((uint32_t)payload[offset + 1] << 16) |
                        ((uint32_t)payload[offset + 2] << 8) |
                        payload[offset + 3];
        offset += 4;
        uint32_t msg_len = ((uint32_t)payload[offset] << 24) |
                           ((uint32_t)payload[offset + 1] << 16) |
                           ((uint32_t)payload[offset + 2] << 8) |
                           payload[offset + 3];
        offset += 4;
        ESP_LOGE(TAG, "asr error code=%u msg=%.*s",
                 (unsigned)code, (int)msg_len, (const char *)(payload + offset));
        return ESP_FAIL;
    }
    if (message_type != ASR_MSG_FULL_SERVER_RESPONSE ||
        serialization != ASR_SERIALIZATION_JSON ||
        compression != ASR_COMPRESSION_NONE) {
        ESP_LOGE(TAG, "unexpected asr response type=%u serialization=%u compression=%u",
                 message_type, serialization, compression);
        return ESP_FAIL;
    }
    if (len < offset + 4) return ESP_FAIL;
    uint32_t body_len = ((uint32_t)payload[offset] << 24) |
                        ((uint32_t)payload[offset + 1] << 16) |
                        ((uint32_t)payload[offset + 2] << 8) |
                        payload[offset + 3];
    offset += 4;
    if (body_len == 0 || len < offset + body_len) return ESP_OK;

    char *json = malloc(body_len + 1);
    if (!json) return ESP_ERR_NO_MEM;
    memcpy(json, payload + offset, body_len);
    json[body_len] = 0;

    char *text = extract_result_text(json);
    if (text && text[0]) {
        free(session->latest_text);
        session->latest_text = text;
        ESP_LOGI(TAG, "asr text: %s", session->latest_text);
    } else {
        free(text);
    }
    free(json);
    return ESP_OK;
}

static esp_err_t read_asr_response(doubao_asr_session_t *session) {
    uint8_t *payload = NULL;
    size_t len = 0;
    uint8_t opcode = 0;
    esp_err_t err = websocket_read_frame(session->tls, &payload, &len, &opcode);
    if (err != ESP_OK) return err;
    if (opcode != 0x2) {
        ESP_LOGE(TAG, "unexpected websocket opcode=%u", opcode);
        free(payload);
        return ESP_FAIL;
    }
    err = parse_asr_response(session, payload, len);
    free(payload);
    return err;
}

static esp_err_t send_asr_payload(doubao_asr_session_t *session,
                                  uint8_t message_type,
                                  uint8_t flags,
                                  uint8_t serialization,
                                  const uint8_t *body,
                                  size_t body_len) {
    size_t payload_len = 8 + body_len;
    uint8_t *payload = malloc(payload_len);
    if (!payload) return ESP_ERR_NO_MEM;
    payload[0] = (ASR_PROTOCOL_VERSION << 4) | ASR_HEADER_SIZE_WORDS;
    payload[1] = (message_type << 4) | flags;
    payload[2] = (serialization << 4) | ASR_COMPRESSION_NONE;
    payload[3] = 0;
    payload[4] = (uint8_t)(body_len >> 24);
    payload[5] = (uint8_t)(body_len >> 16);
    payload[6] = (uint8_t)(body_len >> 8);
    payload[7] = (uint8_t)body_len;
    if (body_len > 0) memcpy(payload + 8, body, body_len);

    esp_err_t err = websocket_send_frame(session->tls, 0x2, payload, payload_len);
    free(payload);
    if (err != ESP_OK) return err;
    return read_asr_response(session);
}

static esp_err_t websocket_handshake(doubao_asr_session_t *session) {
    char key[25];
    char request_id[37];
    if (make_ws_key(key) != ESP_OK) return ESP_FAIL;
    make_uuid(request_id);

    char req[1024];
    int n = snprintf(req, sizeof(req),
                     "GET %s HTTP/1.1\r\n"
                     "Host: %s\r\n"
                     "Upgrade: websocket\r\n"
                     "Connection: Upgrade\r\n"
                     "Sec-WebSocket-Key: %s\r\n"
                     "Sec-WebSocket-Version: 13\r\n"
                     "X-Api-Key: %s\r\n"
                     "X-Api-Resource-Id: %s\r\n"
                     "X-Api-Request-Id: %s\r\n"
                     "X-Api-Sequence: -1\r\n"
                     "\r\n",
                     ASR_PATH, ASR_HOST, key,
                     app_secrets_asr_api_key(),
                     APP_DOUBAO_ASR_RESOURCE_ID,
                     request_id);
    if (n <= 0 || n >= (int)sizeof(req)) return ESP_FAIL;
    if (tls_write_all(session->tls, req, (size_t)n) != 0) return ESP_FAIL;

    char line[192];
    if (tls_read_line(session->tls, line, sizeof(line), ASR_CONNECT_TIMEOUT_MS) <= 0) {
        return ESP_FAIL;
    }
    int code = 0;
    if (sscanf(line, "HTTP/%*s %d", &code) != 1 || code != 101) {
        ESP_LOGE(TAG, "websocket handshake failed: %s", line);
        return ESP_FAIL;
    }
    for (;;) {
        int r = tls_read_line(session->tls, line, sizeof(line), ASR_CONNECT_TIMEOUT_MS);
        if (r < 0) return ESP_FAIL;
        if (r == 0) break;
    }
    return ESP_OK;
}

esp_err_t doubao_asr_session_start(doubao_asr_session_t **out) {
    if (!out) return ESP_ERR_INVALID_ARG;
    if (!app_secrets_ready()) return ESP_ERR_INVALID_STATE;
    *out = NULL;

    doubao_asr_session_t *session = calloc(1, sizeof(*session));
    if (!session) return ESP_ERR_NO_MEM;

    esp_tls_cfg_t cfg = {
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = ASR_CONNECT_TIMEOUT_MS,
    };
    session->tls = esp_tls_init();
    if (!session->tls) {
        free(session);
        return ESP_FAIL;
    }

    int rc = esp_tls_conn_new_sync(ASR_HOST, strlen(ASR_HOST), ASR_PORT, &cfg, session->tls);
    if (rc != 1) {
        ESP_LOGE(TAG, "tls connect rc=%d", rc);
        doubao_asr_session_close(session);
        return ESP_FAIL;
    }
    if (websocket_handshake(session) != ESP_OK) {
        doubao_asr_session_close(session);
        return ESP_FAIL;
    }

    const char *body =
        "{\"user\":{\"uid\":\"esp32-zhalarm\"},"
        "\"audio\":{\"format\":\"pcm\",\"codec\":\"raw\",\"rate\":16000,"
        "\"bits\":16,\"channel\":1,\"language\":\"zh-CN\"},"
        "\"request\":{\"model_name\":\"bigmodel\",\"enable_itn\":true,"
        "\"enable_punc\":true,\"enable_ddc\":false,\"show_utterances\":false}}";
    esp_err_t err = send_asr_payload(session,
                                     ASR_MSG_FULL_CLIENT_REQUEST,
                                     ASR_FLAG_NONE,
                                     ASR_SERIALIZATION_JSON,
                                     (const uint8_t *)body,
                                     strlen(body));
    if (err != ESP_OK) {
        doubao_asr_session_close(session);
        return err;
    }

    *out = session;
    return ESP_OK;
}

esp_err_t doubao_asr_session_send_audio(doubao_asr_session_t *session,
                                        const uint8_t *pcm,
                                        size_t len,
                                        bool is_last) {
    if (!session || !session->tls) return ESP_ERR_INVALID_ARG;
    if (session->final_sent) return ESP_ERR_INVALID_STATE;
    if (!pcm && len > 0) return ESP_ERR_INVALID_ARG;
    session->final_sent = is_last;
    return send_asr_payload(session,
                            ASR_MSG_AUDIO_ONLY_REQUEST,
                            is_last ? ASR_FLAG_LAST_NO_SEQUENCE : ASR_FLAG_NONE,
                            ASR_SERIALIZATION_NONE,
                            pcm ? pcm : (const uint8_t *)"",
                            len);
}

esp_err_t doubao_asr_session_finish(doubao_asr_session_t *session,
                                    char **out_text) {
    if (!session || !out_text) return ESP_ERR_INVALID_ARG;
    *out_text = NULL;
    if (!session->final_sent) {
        esp_err_t err = doubao_asr_session_send_audio(session, NULL, 0, true);
        if (err != ESP_OK) return err;
    }
    if (!session->latest_text || !session->latest_text[0]) return ESP_FAIL;
    *out_text = strdup(session->latest_text);
    return *out_text ? ESP_OK : ESP_ERR_NO_MEM;
}

void doubao_asr_session_close(doubao_asr_session_t *session) {
    if (!session) return;
    if (session->tls) {
        esp_tls_conn_destroy(session->tls);
        session->tls = NULL;
    }
    free(session->latest_text);
    free(session);
}
