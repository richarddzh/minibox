#include "doubao_agent_client.h"
#include "app_config.h"
#include "app_secret_store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <esp_http_client.h>
#include <esp_crt_bundle.h>
#include <esp_log.h>
#include <esp_random.h>

static const char *TAG = "doubao_agent";

#define AGENT_HTTP_TIMEOUT_MS 30000
#define AGENT_RESPONSE_MAX_BYTES 8192

static const char *const CHIME_RANDOM_PROMPTS[] = {
    "现在几点啦？",
    "请你随便说点什么吧。",
    "请你说个绕口令吧。",
    "请你讲个笑话吧。",
    "随便聊一下天气？",
    "随便聊一下吃饭？",
    "随便聊一下地球？",
    "随便聊一下月亮？",
    "随便聊一下超级马里奥吧。",
    "随便聊一下上海。",
    "随便聊一下山东。",
    "随便聊一下猫咪。",
    "让我们一起发呆休息吧。",
    "随便聊一下作业。",
    "随便聊一下今天的新闻吧。",
    "随便聊一下上海的热点新闻。",
    "随便聊一下科技方面的热点。",
    "随便聊一下游戏方面的热点。",
    "随便聊一下最近热播的电视剧。",
    "让我们一起想想明天干啥。",
    "感受一下美好的生活。",
    "随便聊一下骑行。",
    "随便聊一下花朵。",
};

typedef struct {
    char *data;
    char *content;
    size_t len;
    bool overflow;
    int status_code;
} agent_http_ctx_t;

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

static char *extract_json_string_after(const char *json,
                                       const char *anchor,
                                       const char *key) {
    const char *p = anchor ? strstr(json, anchor) : json;
    if (!p) return NULL;
    p = strstr(p, key);
    if (!p) return NULL;
    p += strlen(key);
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

static esp_err_t agent_http_event(esp_http_client_event_t *e) {
    agent_http_ctx_t *ctx = (agent_http_ctx_t *)e->user_data;
    if (!ctx) return ESP_OK;

    if (e->event_id == HTTP_EVENT_ON_DATA && e->data && e->data_len > 0) {
        ctx->status_code = esp_http_client_get_status_code(e->client);
        if (ctx->content) return ESP_OK;

        size_t avail = AGENT_RESPONSE_MAX_BYTES - ctx->len;
        size_t take = (size_t)e->data_len;
        if (take > avail) take = avail;
        if (take > 0) {
            memcpy(ctx->data + ctx->len, e->data, take);
            ctx->len += take;
            ctx->data[ctx->len] = 0;
            ctx->content = extract_json_string_after(ctx->data, "\"message\"", "\"content\"");
            if (ctx->content) return ESP_OK;
        }

        if (take < (size_t)e->data_len) {
            ctx->overflow = true;
            return ESP_FAIL;
        }
    }
    return ESP_OK;
}

static char *build_agent_body(const char *prompt) {
    char *escaped_prompt = json_escape_alloc(prompt);
    if (!escaped_prompt) return NULL;

    const char *body_fmt =
        "{\"bot_id\":\"%s\",\"stream\":false,"
        "\"messages\":[{\"role\":\"user\",\"content\":\"%s\"}],"
        "\"extension_options\":{\"disable_follow_up\":true,\"disable_citation\":true,"
        "\"disable_image_text_mix\":true,\"disable_baike_highlight\":true,"
        "\"disable_text_to_image\":true,\"disable_video_text_mix\":true}}";

    int len = snprintf(NULL, 0, body_fmt,
                       APP_DOUBAO_AGENT_BOT_ID,
                       escaped_prompt);
    if (len <= 0) {
        free(escaped_prompt);
        return NULL;
    }

    char *body = malloc((size_t)len + 1);
    if (!body) {
        free(escaped_prompt);
        return NULL;
    }
    snprintf(body, (size_t)len + 1, body_fmt,
             APP_DOUBAO_AGENT_BOT_ID,
             escaped_prompt);
    free(escaped_prompt);
    return body;
}

static esp_err_t doubao_agent_request(const char *prompt, char **out_text) {
    if (!prompt || !prompt[0] || !out_text) return ESP_ERR_INVALID_ARG;
    if (!app_secrets_ready()) return ESP_ERR_INVALID_STATE;
    *out_text = NULL;

    char *body = build_agent_body(prompt);
    if (!body) return ESP_ERR_NO_MEM;

    agent_http_ctx_t ctx = {
        .data = calloc(1, AGENT_RESPONSE_MAX_BYTES + 1),
    };
    if (!ctx.data) {
        free(body);
        return ESP_ERR_NO_MEM;
    }

    esp_http_client_config_t cfg = {
        .url = APP_DOUBAO_AGENT_URL,
        .timeout_ms = AGENT_HTTP_TIMEOUT_MS,
        .event_handler = agent_http_event,
        .user_data = &ctx,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
    };
    esp_http_client_handle_t cli = esp_http_client_init(&cfg);
    if (!cli) {
        free(ctx.data);
        free(body);
        return ESP_FAIL;
    }

    char auth[320];
    snprintf(auth, sizeof(auth), "Bearer %s", app_secrets_agent_api_key());
    esp_http_client_set_method(cli, HTTP_METHOD_POST);
    esp_http_client_set_header(cli, "Authorization", auth);
    esp_http_client_set_header(cli, "Content-Type", "application/json");
    esp_http_client_set_header(cli, "Accept", "application/json");
    esp_http_client_set_post_field(cli, body, (int)strlen(body));

    esp_err_t err = esp_http_client_perform(cli);
    int code = esp_http_client_get_status_code(cli);
    esp_http_client_cleanup(cli);
    free(body);

    if ((err != ESP_OK && !ctx.content) || (ctx.overflow && !ctx.content) || code != 200) {
        ESP_LOGE(TAG, "agent err=%s code=%d len=%u overflow=%d body=%s",
                 esp_err_to_name(err), code, (unsigned)ctx.len, ctx.overflow, ctx.data);
        free(ctx.content);
        free(ctx.data);
        return err != ESP_OK ? err : ESP_FAIL;
    }

    char *text = ctx.content ? ctx.content :
                 extract_json_string_after(ctx.data, "\"message\"", "\"content\"");
    ctx.content = NULL;
    if (!text || !text[0]) {
        ESP_LOGE(TAG, "agent response missing message content: %s", ctx.data);
        free(text);
        free(ctx.data);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "agent text %u bytes: %s", (unsigned)strlen(text), text);
    free(ctx.data);
    *out_text = text;
    return ESP_OK;
}

esp_err_t doubao_agent_chat(const char *user_text, char **out_text) {
    return doubao_agent_request(user_text, out_text);
}

esp_err_t doubao_agent_chime(const char *time_arg, char **out_text) {
    char timed_prompt[256];
    const char *prompt = NULL;
    if (time_arg && time_arg[0]) {
        snprintf(timed_prompt, sizeof(timed_prompt),
                 "现在是%s。请你整点报时。",
                 time_arg);
        prompt = timed_prompt;
    } else {
        size_t count = sizeof(CHIME_RANDOM_PROMPTS) / sizeof(CHIME_RANDOM_PROMPTS[0]);
        prompt = CHIME_RANDOM_PROMPTS[esp_random() % count];
    }
    return doubao_agent_request(prompt, out_text);
}
