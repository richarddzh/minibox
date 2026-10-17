#pragma once

#include <stdbool.h>
#include "esp_err.h"

typedef struct {
    bool ok;
    int year, month, day, hour, minute, second;
    char date[16];
    char time[16];
    char err[48];
} rtc_pcf8563_time_t;

esp_err_t rtc_pcf8563_init(void);
bool rtc_pcf8563_ready(void);
rtc_pcf8563_time_t rtc_pcf8563_read(void);
esp_err_t rtc_pcf8563_set_from_strings(const char *date, const char *time);
