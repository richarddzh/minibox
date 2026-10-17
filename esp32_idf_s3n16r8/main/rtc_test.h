#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

typedef struct {
    int year, month, day, hour, minute, second;
    uint32_t seconds_of_day;
    bool ticking;
} rtc_test_state_t;

esp_err_t rtc_test_init(void);
esp_err_t rtc_test_set(int year, int month, int day, int hour, int minute, int second);
esp_err_t rtc_test_read(rtc_test_state_t *state);
