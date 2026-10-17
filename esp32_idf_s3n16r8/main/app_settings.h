#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <esp_err.h>

#include "gui_app.h"
#include "wifi_creds.h"
#include "wifi_time.h"

const gui_app_t *app_settings_descriptor(void);
void app_settings_set_wifi_scan(const wifi_scan_ap_t *results, size_t count,
                                esp_err_t result);
bool app_settings_take_wifi_credentials(wifi_creds_t *out);
void app_settings_set_wifi_status(const char *status);
