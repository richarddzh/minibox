#include "rtc_pcf8563.h"
#include "rtc_test.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include "esp_log.h"

static const char *TAG = "rtc_time";
static bool s_ready;

static void set_system_clock(const rtc_pcf8563_time_t *rtc) {
    struct tm local = {
        .tm_year = rtc->year - 1900, .tm_mon = rtc->month - 1,
        .tm_mday = rtc->day, .tm_hour = rtc->hour,
        .tm_min = rtc->minute, .tm_sec = rtc->second, .tm_isdst = -1,
    };
    struct timeval now = {.tv_sec = mktime(&local)};
    settimeofday(&now, NULL);
}

esp_err_t rtc_pcf8563_init(void) {
    setenv("TZ", "CST-8", 1);
    tzset();
    esp_err_t err = rtc_test_init();
    s_ready = err == ESP_OK;
    if (s_ready) {
        rtc_pcf8563_time_t rtc = rtc_pcf8563_read();
        if (rtc.ok) set_system_clock(&rtc);
    }
    return err;
}

bool rtc_pcf8563_ready(void) {
    return s_ready;
}

rtc_pcf8563_time_t rtc_pcf8563_read(void) {
    rtc_pcf8563_time_t result = {0};
    rtc_test_state_t rtc = {0};
    esp_err_t err = s_ready ? rtc_test_read(&rtc) : ESP_ERR_INVALID_STATE;
    if (err != ESP_OK) {
        snprintf(result.err, sizeof(result.err), "PCF8563: %s", esp_err_to_name(err));
        return result;
    }
    result.ok = true;
    result.year = rtc.year;
    result.month = rtc.month;
    result.day = rtc.day;
    result.hour = rtc.hour;
    result.minute = rtc.minute;
    result.second = rtc.second;
    snprintf(result.date, sizeof(result.date), "%04d-%02d-%02d",
             rtc.year, rtc.month, rtc.day);
    snprintf(result.time, sizeof(result.time), "%02d:%02d:%02d",
             rtc.hour, rtc.minute, rtc.second);
    return result;
}

esp_err_t rtc_pcf8563_set_from_strings(const char *date, const char *time) {
    rtc_pcf8563_time_t rtc = {0};
    char extra;
    if (!date || !time ||
        sscanf(date, "%d-%d-%d%c", &rtc.year, &rtc.month, &rtc.day, &extra) != 3 ||
        sscanf(time, "%d:%d:%d%c", &rtc.hour, &rtc.minute, &rtc.second, &extra) != 3) {
        ESP_LOGE(TAG, "Invalid network date/time");
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = rtc_test_set(rtc.year, rtc.month, rtc.day,
                               rtc.hour, rtc.minute, rtc.second);
    if (err == ESP_OK) set_system_clock(&rtc);
    return err;
}
