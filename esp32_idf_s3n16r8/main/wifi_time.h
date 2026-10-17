#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <esp_err.h>
#include "wifi_creds.h"

typedef struct {
    bool ok;
    int  http_code;
    char date[16];        // YYYY-MM-DD
    char time[16];        // HH:MM:SS
    char err[48];
} wifi_time_result_t;

#define WIFI_SCAN_MAX_RESULTS 12

typedef struct {
    char ssid[33];
    int8_t rssi;
    bool secured;
} wifi_scan_ap_t;

// Initialise WiFi station once at boot. Idempotent.
esp_err_t wifi_time_init(void);

void wifi_time_set_credentials(const wifi_creds_t *c);
bool wifi_time_has_credentials(void);
const char *wifi_time_ssid(void);

bool wifi_time_is_connected(void);
uint8_t wifi_time_last_disconnect_reason(void);
const char *wifi_time_last_disconnect_reason_text(void);

// Connect (if not yet connected) using the active credentials. Blocks.
esp_err_t wifi_time_connect(void);
// Disconnect station and stop, freeing buffers before setup AP starts.
esp_err_t wifi_time_disconnect(void);
esp_err_t wifi_time_scan(wifi_scan_ap_t *results, size_t capacity,
                         size_t *count);

// One-shot HTTPS GET to the time API.
wifi_time_result_t wifi_time_fetch(void);
