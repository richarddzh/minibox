#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <esp_err.h>

typedef struct {
    char ssid[33];
    char password[65];
} wifi_creds_t;

#define WIFI_CREDS_MAX_PROFILES 8

// Mounts the separate storage partition at /spiffs without auto-formatting.
esp_err_t wifi_creds_init(void);

// Returns 0 if creds loaded, -1 if file missing, -2 on parse/IO error.
int  wifi_creds_load(wifi_creds_t *out);

// Persists credentials. Returns 0 on success.
int  wifi_creds_save(const wifi_creds_t *in);

// Lists saved networks in most-recently-selected order. A legacy
// wifi_config.txt installation is returned as a one-entry list and is
// migrated the next time credentials are saved.
int wifi_creds_list(wifi_creds_t *out, size_t capacity, size_t *count);
int wifi_creds_find(const char *ssid, wifi_creds_t *out);

bool wifi_creds_valid(const wifi_creds_t *c);
