#include "app_secret_store.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "esp_log.h"

static app_secrets_t s_current;
static const char *TAG = "secrets";
#define SECRET_TEMP_PATH "/spiffs/app_secrets.tmp"
#define SECRET_BACKUP_PATH "/spiffs/app_secrets.bak"

static void rstrip(char *s) {
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\r' || s[n - 1] == '\n' ||
                     s[n - 1] == ' ' || s[n - 1] == '\t')) {
        s[--n] = 0;
    }
}

bool app_secrets_valid(const app_secrets_t *s) {
    return s && s->asr_api_key[0] && s->agent_api_key[0] && s->tts_api_key[0];
}

int app_secrets_load_partial(app_secrets_t *out) {
    if (!out) return -2;
    memset(out, 0, sizeof(*out));
    FILE *f = fopen(APP_SECRETS_CONFIG_PATH, "r");
    if (!f) f = fopen(SECRET_BACKUP_PATH, "r");
    if (!f) {
        ESP_LOGW(TAG, "API configuration file unavailable");
        return -1;
    }
    char *fields[] = {out->asr_api_key, out->agent_api_key, out->tts_api_key};
    bool ok = true;
    for (unsigned i = 0; i < 3; ++i) {
        if (!fgets(fields[i], APP_SECRET_VALUE_MAX, f) ||
            (!strchr(fields[i], '\n') && !feof(f))) {
            ok = false;
            break;
        }
    }
    if (ferror(f)) ok = false;
    if (fclose(f) != 0) ok = false;
    if (!ok) {
        memset(out, 0, sizeof(*out));
        ESP_LOGE(TAG, "Invalid API configuration: expected three key lines");
        return -2;
    }
    rstrip(out->asr_api_key);
    rstrip(out->agent_api_key);
    rstrip(out->tts_api_key);
    return 0;
}

int app_secrets_load(app_secrets_t *out) {
    int rc = app_secrets_load_partial(out);
    if (rc != 0) return rc;
    return app_secrets_valid(out) ? 0 : -2;
}

int app_secrets_save_partial(const app_secrets_t *in) {
    if (!in) return -2;
    const char *fields[] = {in->asr_api_key, in->agent_api_key, in->tts_api_key};
    for (unsigned i = 0; i < 3; ++i) {
        if (strnlen(fields[i], APP_SECRET_VALUE_MAX) == APP_SECRET_VALUE_MAX ||
            strpbrk(fields[i], "\r\n")) {
            ESP_LOGE(TAG, "Invalid API key format");
            return -2;
        }
    }
    FILE *f = fopen(SECRET_TEMP_PATH, "w");
    if (!f) {
        ESP_LOGE(TAG, "Cannot create API configuration");
        return -2;
    }
    bool ok = fprintf(f, "%s\n%s\n%s\n",
                      in->asr_api_key, in->agent_api_key, in->tts_api_key) >= 0;
    if (fclose(f) != 0) ok = false;
    if (ok) {
        remove(SECRET_BACKUP_PATH);
        ok = rename(APP_SECRETS_CONFIG_PATH, SECRET_BACKUP_PATH) == 0 || errno == ENOENT;
        if (ok && rename(SECRET_TEMP_PATH, APP_SECRETS_CONFIG_PATH) == 0) {
            remove(SECRET_BACKUP_PATH);
            return 0;
        }
        if (ok && rename(SECRET_BACKUP_PATH, APP_SECRETS_CONFIG_PATH) != 0 && errno != ENOENT) {
            ESP_LOGE(TAG, "Cannot restore API configuration backup");
        }
    }
    remove(SECRET_TEMP_PATH);
    ESP_LOGE(TAG, "Cannot save API configuration");
    return -2;
}

int app_secrets_save(const app_secrets_t *in) {
    if (!app_secrets_valid(in)) return -2;
    return app_secrets_save_partial(in);
}

void app_secrets_set_current(const app_secrets_t *s) {
    if (s) s_current = *s;
    else memset(&s_current, 0, sizeof(s_current));
}

bool app_secrets_ready(void) { return app_secrets_valid(&s_current); }
const char *app_secrets_asr_api_key(void) { return s_current.asr_api_key; }
const char *app_secrets_agent_api_key(void) { return s_current.agent_api_key; }
const char *app_secrets_tts_api_key(void) { return s_current.tts_api_key; }
