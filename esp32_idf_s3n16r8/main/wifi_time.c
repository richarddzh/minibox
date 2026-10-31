#include "wifi_time.h"
#include "app_config.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <time.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <esp_netif_sntp.h>

static const char *TAG = "wifi_time";

static wifi_creds_t  s_creds;
static bool          s_inited;
static bool          s_started;
static atomic_bool   s_connected;
static atomic_uchar  s_last_disconnect_reason;
static EventGroupHandle_t s_evt;
#define EVT_GOT_IP   BIT0
#define EVT_FAIL     BIT1

static void ensure_backup_dns(esp_netif_t *netif) {
    esp_netif_dns_info_t dns = {0};
    esp_err_t err = esp_netif_get_dns_info(netif, ESP_NETIF_DNS_BACKUP, &dns);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "read backup DNS: %s", esp_err_to_name(err));
        return;
    }
    if (dns.ip.type == ESP_IPADDR_TYPE_V4 && dns.ip.u_addr.ip4.addr == 0) {
        esp_netif_dns_info_t fallback = {
            .ip = ESP_IP4ADDR_INIT(223, 5, 5, 5),
        };
        /* Error replies only try the immediately following DNS slot in lwIP. */
        err = esp_netif_set_dns_info(netif, ESP_NETIF_DNS_BACKUP, &fallback);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "set backup DNS: %s", esp_err_to_name(err));
            return;
        }
        ESP_LOGI(TAG, "backup DNS 223.5.5.5; DHCP main DNS retained");
    }
    for (esp_netif_dns_type_t type = ESP_NETIF_DNS_MAIN;
         type < ESP_NETIF_DNS_MAX; ++type) {
        err = esp_netif_get_dns_info(netif, type, &dns);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "read DNS %d: %s", type, esp_err_to_name(err));
        } else if (dns.ip.type == ESP_IPADDR_TYPE_V4) {
            ESP_LOGI(TAG, "DNS %d: " IPSTR, type, IP2STR(&dns.ip.u_addr.ip4));
        }
    }
}

static void event_handler(void *arg, esp_event_base_t base,
                          int32_t id, void *data) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        if (wifi_time_has_credentials()) esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        const wifi_event_sta_disconnected_t *disc =
            (const wifi_event_sta_disconnected_t *)data;
        s_connected = false;
        s_last_disconnect_reason = disc ? disc->reason : 0;
        ESP_LOGW(TAG, "STA disconnected, reason=%u (%s)",
                 s_last_disconnect_reason,
                 wifi_time_last_disconnect_reason_text());
        xEventGroupSetBits(s_evt, EVT_FAIL);
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *event = data;
        ensure_backup_dns(event->esp_netif);
        s_connected = true;
        xEventGroupSetBits(s_evt, EVT_GOT_IP);
    }
}

esp_err_t wifi_time_init(void) {
    if (s_inited) return ESP_OK;
    s_evt = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    // Voice streaming is sensitive to packet batching/jitter; disable STA
    // power save so Wi-Fi keeps servicing traffic continuously.
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    s_inited = true;
    return ESP_OK;
}

void wifi_time_set_credentials(const wifi_creds_t *c) {
    if (c) s_creds = *c; else memset(&s_creds, 0, sizeof(s_creds));
}

bool wifi_time_has_credentials(void) { return wifi_creds_valid(&s_creds); }
const char *wifi_time_ssid(void) { return s_creds.ssid; }
bool wifi_time_is_connected(void) { return s_connected; }
uint8_t wifi_time_last_disconnect_reason(void) { return s_last_disconnect_reason; }

const char *wifi_time_last_disconnect_reason_text(void) {
    switch (s_last_disconnect_reason) {
    case 0: return "connect timeout";
    case WIFI_REASON_AUTH_EXPIRE: return "auth expired";
    case WIFI_REASON_AUTH_LEAVE: return "auth left";
    case WIFI_REASON_ASSOC_EXPIRE: return "assoc expired";
    case WIFI_REASON_ASSOC_TOOMANY: return "AP full";
    case WIFI_REASON_NOT_AUTHED: return "not authed";
    case WIFI_REASON_NOT_ASSOCED: return "not assoced";
    case WIFI_REASON_ASSOC_LEAVE: return "assoc left";
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT: return "4way timeout";
    case WIFI_REASON_HANDSHAKE_TIMEOUT: return "handshake timeout";
    case WIFI_REASON_AUTH_FAIL: return "auth failed";
    case WIFI_REASON_ASSOC_FAIL: return "assoc failed";
    case WIFI_REASON_NO_AP_FOUND: return "no AP found";
    case WIFI_REASON_NO_AP_FOUND_W_COMPATIBLE_SECURITY: return "security mismatch";
    case WIFI_REASON_NO_AP_FOUND_IN_AUTHMODE_THRESHOLD: return "authmode mismatch";
    case WIFI_REASON_NO_AP_FOUND_IN_RSSI_THRESHOLD: return "weak signal";
    case WIFI_REASON_CONNECTION_FAIL: return "connection fail";
    case WIFI_REASON_BEACON_TIMEOUT: return "beacon timeout";
    case WIFI_REASON_TIMEOUT: return "timeout";
    case WIFI_REASON_AP_INITIATED: return "AP kicked";
    case WIFI_REASON_PEER_INITIATED: return "peer disconnected";
    default: return "unknown";
    }
}

esp_err_t wifi_time_connect(void) {
    if (!wifi_time_has_credentials()) return ESP_ERR_INVALID_STATE;
    if (s_connected) return ESP_OK;

    xEventGroupClearBits(s_evt, EVT_GOT_IP | EVT_FAIL);
    s_last_disconnect_reason = 0;

    wifi_config_t wc = {0};
    memcpy(wc.sta.ssid, s_creds.ssid, strlen(s_creds.ssid));
    memcpy(wc.sta.password, s_creds.password, strlen(s_creds.password));
    wc.sta.threshold.authmode = WIFI_AUTH_OPEN;
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wc));

    if (!s_started) {
        ESP_ERROR_CHECK(esp_wifi_start());
        s_started = true;
    } else {
        esp_wifi_connect();
    }

    EventBits_t bits = xEventGroupWaitBits(
        s_evt, EVT_GOT_IP | EVT_FAIL, pdFALSE, pdFALSE,
        pdMS_TO_TICKS(APP_WIFI_CONNECT_TIMEOUT_MS));
    if (bits & EVT_GOT_IP) return ESP_OK;
    ESP_LOGW(TAG, "STA connect failed, reason=%u (%s)",
             s_last_disconnect_reason,
             wifi_time_last_disconnect_reason_text());
    return ESP_FAIL;
}

esp_err_t wifi_time_disconnect(void) {
    if (!s_started) return ESP_OK;
    esp_wifi_disconnect();
    esp_wifi_stop();
    s_started = false;
    s_connected = false;
    return ESP_OK;
}

esp_err_t wifi_time_scan(wifi_scan_ap_t *results, size_t capacity,
                         size_t *count) {
    if (!count || (capacity > 0 && !results)) return ESP_ERR_INVALID_ARG;
    *count = 0;
    if (!s_inited) return ESP_ERR_INVALID_STATE;

    if (!s_started) {
        esp_err_t err = esp_wifi_set_mode(WIFI_MODE_STA);
        if (err != ESP_OK) return err;
        err = esp_wifi_start();
        if (err != ESP_OK) return err;
        s_started = true;
    }

    wifi_scan_config_t scan = {
        .show_hidden = false,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
    };
    esp_err_t err = esp_wifi_scan_start(&scan, true);
    if (err != ESP_OK) return err;

    uint16_t found = 0;
    err = esp_wifi_scan_get_ap_num(&found);
    if (err != ESP_OK) {
        esp_wifi_clear_ap_list();
        return err;
    }
    if (found == 0 || capacity == 0) {
        esp_wifi_clear_ap_list();
        return ESP_OK;
    }

    uint16_t fetch = found > 32 ? 32 : found;
    wifi_ap_record_t *records = calloc(fetch, sizeof(*records));
    if (!records) {
        esp_wifi_clear_ap_list();
        return ESP_ERR_NO_MEM;
    }
    err = esp_wifi_scan_get_ap_records(&fetch, records);
    if (err != ESP_OK) {
        free(records);
        return err;
    }

    for (uint16_t i = 0; i < fetch; ++i) {
        if (!records[i].ssid[0]) continue;
        size_t existing = *count;
        for (size_t j = 0; j < *count; ++j) {
            if (strcmp(results[j].ssid, (const char *)records[i].ssid) == 0) {
                existing = j;
                break;
            }
        }
        if (existing < *count) {
            if (records[i].rssi > results[existing].rssi) {
                results[existing].rssi = records[i].rssi;
                results[existing].secured =
                    records[i].authmode != WIFI_AUTH_OPEN;
            }
            continue;
        }
        if (*count >= capacity) continue;
        snprintf(results[*count].ssid, sizeof(results[*count].ssid), "%s",
                 (const char *)records[i].ssid);
        results[*count].rssi = records[i].rssi;
        results[*count].secured = records[i].authmode != WIFI_AUTH_OPEN;
        (*count)++;
    }
    free(records);

    for (size_t i = 1; i < *count; ++i) {
        wifi_scan_ap_t item = results[i];
        size_t j = i;
        while (j > 0 && results[j - 1].rssi < item.rssi) {
            results[j] = results[j - 1];
            --j;
        }
        results[j] = item;
    }
    return ESP_OK;
}

wifi_time_result_t wifi_time_fetch(void) {
    wifi_time_result_t r = {0};
    if (!s_connected) {
        snprintf(r.err, sizeof(r.err), "wifi down");
        return r;
    }

    esp_sntp_config_t cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG("ntp.aliyun.com");
    esp_err_t err = esp_netif_sntp_init(&cfg);
    if (err != ESP_OK) {
        snprintf(r.err, sizeof(r.err), "sntp init");
        return r;
    }

    err = esp_netif_sntp_sync_wait(pdMS_TO_TICKS(10000));
    if (err != ESP_OK) {
        esp_netif_sntp_deinit();
        snprintf(r.err, sizeof(r.err), "sntp timeout");
        return r;
    }

    time_t now = 0;
    time(&now);
    setenv("TZ", "CST-8", 1);
    tzset();

    struct tm local = {0};
    localtime_r(&now, &local);
    esp_netif_sntp_deinit();

    if (local.tm_year + 1900 < 2020) {
        snprintf(r.err, sizeof(r.err), "sntp invalid");
        return r;
    }

    if (strftime(r.date, sizeof(r.date), "%Y-%m-%d", &local) == 0 ||
        strftime(r.time, sizeof(r.time), "%H:%M:%S", &local) == 0) {
        snprintf(r.err, sizeof(r.err), "sntp format");
        return r;
    }
    r.ok = true;
    ESP_LOGI(TAG, "SNTP date=%s time=%s", r.date, r.time);
    return r;
}
