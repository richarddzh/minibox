#include "wifi_provision.h"
#include "app_config.h"
#include "app_secret_store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <esp_check.h>
#include <esp_http_server.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_wifi.h>

static const char *TAG = "wifi_prov";

#define FORM_BODY_MAX 1600
#define SETUP_PAGE_MAX 4096

static httpd_handle_t s_httpd;
static esp_netif_t *s_ap_netif;
static bool s_active;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static char s_pending_status[64];
static bool s_has_status;
static char s_returned_status[64];

static void set_status(const char *msg) {
    portENTER_CRITICAL(&s_lock);
    snprintf(s_pending_status, sizeof(s_pending_status), "%s", msg);
    s_has_status = true;
    portEXIT_CRITICAL(&s_lock);
}

static int hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static bool url_decode(const char *in, char *out, size_t cap) {
    size_t w = 0;
    for (size_t r = 0; in[r]; ++r) {
        if (w + 1 >= cap) return false;
        if (in[r] == '+') {
            out[w++] = ' ';
        } else if (in[r] == '%') {
            int hi = hex_val(in[r + 1]);
            int lo = hex_val(in[r + 2]);
            if (hi < 0 || lo < 0) return false;
            out[w++] = (char)((hi << 4) | lo);
            r += 2;
        } else {
            out[w++] = in[r];
        }
    }
    out[w] = 0;
    return true;
}

static bool form_value(const char *body, const char *key, char *out, size_t cap) {
    size_t key_len = strlen(key);
    const char *p = body;
    while (*p) {
        const char *eq = strchr(p, '=');
        if (!eq) return false;
        const char *amp = strchr(eq + 1, '&');
        size_t name_len = (size_t)(eq - p);
        if (name_len == key_len && memcmp(p, key, key_len) == 0) {
            size_t raw_len = amp ? (size_t)(amp - (eq + 1)) : strlen(eq + 1);
            char raw[512];
            if (raw_len >= sizeof(raw)) return false;
            memcpy(raw, eq + 1, raw_len);
            raw[raw_len] = 0;
            return url_decode(raw, out, cap);
        }
        if (!amp) return false;
        p = amp + 1;
    }
    return false;
}

static esp_err_t send_page(httpd_req_t *req, const char *message) {
    app_secrets_t *secrets = calloc(1, sizeof(*secrets));
    if (!secrets) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No memory");
        return ESP_OK;
    }
    app_secrets_load_partial(secrets);
    const bool have_asr = secrets->asr_api_key[0] != 0;
    const bool have_agent = secrets->agent_api_key[0] != 0;
    const bool have_tts = secrets->tts_api_key[0] != 0;

    char *page = calloc(1, SETUP_PAGE_MAX);
    if (!page) {
        free(secrets);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No memory");
        return ESP_OK;
    }
    snprintf(page, SETUP_PAGE_MAX,
             "<!doctype html><html><head><meta name=\"viewport\" "
             "content=\"width=device-width,initial-scale=1\">"
             "<title>talkingflower API setup</title></head>"
             "<body><h1>talkingflower API setup</h1>"
             "<p>%s</p>"
             "<p>WiFi networks are configured on the device screen. "
             "Use each Save button below independently.</p>"
             "<form method=\"post\" action=\"/\">"
             "<input type=\"hidden\" name=\"action\" value=\"asr\">"
             "<h2>Doubao ASR API key</h2>"
             "<label>ASR API key<br><input name=\"asr_key\" type=\"password\" "
             "maxlength=\"255\" placeholder=\"%s\"></label><br><br>"
             "<button type=\"submit\">Save ASR key</button>"
             "</form>"
             "<form method=\"post\" action=\"/\">"
             "<input type=\"hidden\" name=\"action\" value=\"agent\">"
             "<h2>Doubao Agent API key</h2>"
             "<label>Agent API key<br><input name=\"agent_key\" type=\"password\" "
             "maxlength=\"255\" placeholder=\"%s\"></label><br><br>"
             "<button type=\"submit\">Save Agent key</button>"
             "</form>"
             "<form method=\"post\" action=\"/\">"
             "<input type=\"hidden\" name=\"action\" value=\"tts\">"
             "<h2>Doubao TTS API key</h2>"
             "<label>TTS API key<br><input name=\"tts_key\" type=\"password\" "
             "maxlength=\"255\" placeholder=\"%s\"></label><br><br>"
             "<button type=\"submit\">Save TTS key</button>"
             "</form>"
             "<p>Return to the device and select Exit setup mode when finished.</p>"
             "</body></html>",
             message ? message : "Save API keys, then exit setup mode on the device.",
             have_asr ? "saved; enter new value to replace" : "required",
             have_agent ? "saved; enter new value to replace" : "required",
             have_tts ? "saved; enter new value to replace" : "required");
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    esp_err_t err = httpd_resp_send(req, page, HTTPD_RESP_USE_STRLEN);
    free(page);
    free(secrets);
    return err;
}

static esp_err_t root_get(httpd_req_t *req) {
    return send_page(req, NULL);
}

static esp_err_t root_post(httpd_req_t *req) {
    if (req->content_len <= 0 || req->content_len >= FORM_BODY_MAX) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid form body");
        return ESP_OK;
    }

    char *body = calloc(1, FORM_BODY_MAX);
    if (!body) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No memory");
        return ESP_OK;
    }
    int total = 0;
    while (total < req->content_len) {
        int got = httpd_req_recv(req, body + total, req->content_len - total);
        if (got <= 0) {
            free(body);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Read failed");
            return ESP_OK;
        }
        total += got;
    }
    body[total] = 0;

    char action[16] = {0};
    if (!form_value(body, "action", action, sizeof(action))) {
        free(body);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid action");
        set_status("Invalid action");
        return ESP_OK;
    }

    app_secrets_t secrets = {0};
    app_secrets_load_partial(&secrets);
    const char *field = NULL;
    char *target = NULL;
    const char *label = NULL;
    if (strcmp(action, "asr") == 0) {
        field = "asr_key";
        target = secrets.asr_api_key;
        label = "ASR key";
    } else if (strcmp(action, "agent") == 0) {
        field = "agent_key";
        target = secrets.agent_api_key;
        label = "Agent key";
    } else if (strcmp(action, "tts") == 0) {
        field = "tts_key";
        target = secrets.tts_api_key;
        label = "TTS key";
    } else {
        free(body);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Unknown action");
        set_status("Unknown action");
        return ESP_OK;
    }

    if (!form_value(body, field, target, APP_SECRET_VALUE_MAX) || !target[0]) {
        free(body);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "API key required");
        set_status("API key needed");
        return ESP_OK;
    }
    if (app_secrets_save_partial(&secrets) != 0) {
        free(body);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "API key save failed");
        set_status("Key save fail");
        return ESP_OK;
    }
    app_secrets_set_current(&secrets);
    set_status(label);

    char msg[64];
    snprintf(msg, sizeof(msg), "%s saved.", label);
    free(body);
    return send_page(req, msg);
}

static esp_err_t start_httpd(void) {
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.uri_match_fn = httpd_uri_match_wildcard;
    cfg.stack_size = 8192;
    cfg.max_open_sockets = 2;
    cfg.lru_purge_enable = true;

    ESP_RETURN_ON_ERROR(httpd_start(&s_httpd, &cfg), TAG, "httpd start");

    httpd_uri_t root_get_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_get,
    };
    httpd_uri_t root_post_uri = {
        .uri = "/",
        .method = HTTP_POST,
        .handler = root_post,
    };
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_httpd, &root_get_uri),
                        TAG, "register GET");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_httpd, &root_post_uri),
                        TAG, "register POST");
    return ESP_OK;
}

esp_err_t wifi_provision_init(void) {
    if (!s_ap_netif) {
        s_ap_netif = esp_netif_create_default_wifi_ap();
        if (!s_ap_netif) return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t wifi_provision_start(void) {
    if (s_active) return ESP_OK;

    wifi_config_t ap_cfg = {0};
    snprintf((char *)ap_cfg.ap.ssid, sizeof(ap_cfg.ap.ssid), "%s",
             APP_PROVISION_AP_SSID);
    ap_cfg.ap.ssid_len = strlen(APP_PROVISION_AP_SSID);
    snprintf((char *)ap_cfg.ap.password, sizeof(ap_cfg.ap.password), "%s",
             APP_PROVISION_AP_PASSWORD);
    ap_cfg.ap.channel = APP_PROVISION_AP_CHANNEL;
    ap_cfg.ap.max_connection = APP_PROVISION_AP_MAX_CONN;
    ap_cfg.ap.authmode = WIFI_AUTH_WPA2_PSK;
    ap_cfg.ap.pmf_cfg.required = false;

    esp_err_t err = esp_wifi_set_mode(WIFI_MODE_AP);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "set AP mode failed: %s", esp_err_to_name(err));
        return err;
    }
    err = esp_wifi_set_config(WIFI_IF_AP, &ap_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "set AP config failed: %s", esp_err_to_name(err));
        return err;
    }
    err = esp_wifi_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "start AP failed: %s", esp_err_to_name(err));
        return err;
    }
    err = start_httpd();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "start httpd failed: %s", esp_err_to_name(err));
        esp_wifi_stop();
        esp_wifi_set_mode(WIFI_MODE_STA);
        return err;
    }

    s_active = true;
    set_status("AP on 192.168.4.1");
    ESP_LOGI(TAG, "AP started ssid=%s", APP_PROVISION_AP_SSID);
    return ESP_OK;
}

esp_err_t wifi_provision_stop(void) {
    if (!s_active) return ESP_OK;

    if (s_httpd) {
        ESP_RETURN_ON_ERROR(httpd_stop(s_httpd), TAG, "httpd stop");
        s_httpd = NULL;
    }
    ESP_RETURN_ON_ERROR(esp_wifi_stop(), TAG, "stop AP");
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "set STA mode");
    s_active = false;
    set_status("AP off");
    ESP_LOGI(TAG, "AP stopped");
    return ESP_OK;
}

bool wifi_provision_active(void) {
    return s_active;
}

const char *wifi_provision_take_status(void) {
    portENTER_CRITICAL(&s_lock);
    if (!s_has_status) {
        portEXIT_CRITICAL(&s_lock);
        return NULL;
    }
    snprintf(s_returned_status, sizeof(s_returned_status), "%s", s_pending_status);
    s_has_status = false;
    s_pending_status[0] = 0;
    portEXIT_CRITICAL(&s_lock);
    return s_returned_status;
}
