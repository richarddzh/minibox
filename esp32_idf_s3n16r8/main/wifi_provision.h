#pragma once
#include <stdbool.h>
#include <esp_err.h>

esp_err_t wifi_provision_init(void);
esp_err_t wifi_provision_start(void);
esp_err_t wifi_provision_stop(void);
bool      wifi_provision_active(void);

const char *wifi_provision_take_status(void);
