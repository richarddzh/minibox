#include "rtc_test.h"
#include "app_config.h"

#include <stdio.h>
#include <string.h>
#include "driver/i2c_master.h"
#include "esp_check.h"

static const char *TAG = "rtc";
static i2c_master_dev_handle_t s_device;

static int decode_bcd(uint8_t value) {
    if ((value & 15) > 9 || (value >> 4) > 9) return -1;
    return (value >> 4) * 10 + (value & 15);
}

static uint8_t encode_bcd(int value) {
    return (value / 10) * 16 + value % 10;
}

static esp_err_t read_registers(uint8_t *data) {
    const uint8_t start = 0;
    return i2c_master_transmit_receive(s_device, &start, 1, data, 9, 100);
}

esp_err_t rtc_test_init(void) {
    const i2c_master_bus_config_t config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = RTC_SDA_PIN,
        .scl_io_num = RTC_SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus;
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&config, &bus), TAG, "create I2C bus");
    ESP_RETURN_ON_ERROR(i2c_master_probe(bus, 0x51, 100), TAG,
                        "PCF8563 not found at 0x51; check SDA15/SCL16 and power");
    const i2c_device_config_t device = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = 0x51,
        .scl_speed_hz = 100000,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bus, &device, &s_device),
                        TAG, "add PCF8563");
    uint8_t data[9];
    ESP_RETURN_ON_ERROR(read_registers(data), TAG, "read control/time registers");
    if ((data[0] & 0x20) || (data[2] & 0x80)) {
        ESP_LOGW(TAG, "RTC stopped or voltage-low flag set; setting local build time %s %s",
                 __DATE__, __TIME__);
        char month_name[4];
        int day, year, hour, minute, second;
        ESP_RETURN_ON_FALSE(sscanf(__DATE__, "%3s %d %d", month_name, &day, &year) == 3 &&
                            sscanf(__TIME__, "%d:%d:%d", &hour, &minute, &second) == 3,
                            ESP_ERR_INVALID_STATE, TAG, "parse build timestamp");
        const char *months = "JanFebMarAprMayJunJulAugSepOctNovDec";
        const char *month = strstr(months, month_name);
        ESP_RETURN_ON_FALSE(month, ESP_ERR_INVALID_STATE, TAG, "parse build month");
        int month_number = (month - months) / 3 + 1;
        ESP_RETURN_ON_ERROR(rtc_test_set(year, month_number, day, hour, minute, second),
                            TAG, "write RTC build time");
    }
    ESP_LOGI(TAG, "PCF8563 at 0x51, SDA=%d SCL=%d; checking calendar and ticking",
             RTC_SDA_PIN, RTC_SCL_PIN);
    return ESP_OK;
}

esp_err_t rtc_test_set(int year, int month, int day, int hour, int minute, int second) {
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    ESP_RETURN_ON_FALSE(s_device, ESP_ERR_INVALID_STATE, TAG, "RTC not initialized");
    ESP_RETURN_ON_FALSE(year >= 1900 && year <= 2099 && month >= 1 && month <= 12 &&
                        day >= 1 && hour >= 0 && hour < 24 && minute >= 0 && minute < 60 &&
                        second >= 0 && second < 60,
                        ESP_ERR_INVALID_ARG, TAG, "invalid calendar fields");
    bool leap = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
    ESP_RETURN_ON_FALSE(day <= days[month - 1] + (month == 2 && leap),
                        ESP_ERR_INVALID_ARG, TAG, "invalid calendar date");
    uint8_t data[9];
    ESP_RETURN_ON_ERROR(read_registers(data), TAG, "read control before setting");
    static const int offsets[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    int y = year - (month < 3);
    int weekday = (y + y / 4 - y / 100 + y / 400 + offsets[month - 1] + day) % 7;
    const uint8_t stop[] = {0, (data[0] & ~0x80) | 0x20};
    ESP_RETURN_ON_ERROR(i2c_master_transmit(s_device, stop, sizeof(stop), 100),
                        TAG, "stop RTC for setting");
    const uint8_t calendar[] = {
        2, encode_bcd(second), encode_bcd(minute), encode_bcd(hour),
        encode_bcd(day), weekday, encode_bcd(month) | (year < 2000 ? 0x80 : 0),
        encode_bcd(year % 100),
    };
    esp_err_t written = i2c_master_transmit(s_device, calendar, sizeof(calendar), 100);
    const uint8_t start[] = {0, data[0] & ~(0x20 | 0x80)};
    ESP_RETURN_ON_ERROR(i2c_master_transmit(s_device, start, sizeof(start), 100),
                        TAG, "restart RTC");
    ESP_RETURN_ON_ERROR(written, TAG, "write calendar");
    return ESP_OK;
}

esp_err_t rtc_test_read(rtc_test_state_t *state) {
    ESP_RETURN_ON_FALSE(state && s_device, ESP_ERR_INVALID_STATE, TAG, "RTC not initialized");
    uint8_t data[9];
    ESP_RETURN_ON_ERROR(read_registers(data), TAG, "read RTC");
    ESP_RETURN_ON_FALSE(!(data[0] & (0x20 | 0x80)) && !(data[2] & 0x80),
                        ESP_ERR_INVALID_STATE, TAG, "RTC stopped, test mode, or voltage low");
    state->second = decode_bcd(data[2] & 0x7f);
    state->minute = decode_bcd(data[3] & 0x7f);
    state->hour = decode_bcd(data[4] & 0x3f);
    state->day = decode_bcd(data[5] & 0x3f);
    state->month = decode_bcd(data[7] & 0x1f);
    int year = decode_bcd(data[8]);
    state->year = ((data[7] & 0x80) ? 1900 : 2000) + year;
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    ESP_RETURN_ON_FALSE(year >= 0 && state->month >= 1 && state->month <= 12 &&
                        state->day >= 1 && state->hour >= 0 && state->hour < 24 &&
                        state->minute >= 0 && state->minute < 60 &&
                        state->second >= 0 && state->second < 60,
                        ESP_ERR_INVALID_RESPONSE, TAG, "invalid BCD calendar");
    bool leap = state->year % 4 == 0 &&
                (state->year % 100 != 0 || state->year % 400 == 0);
    ESP_RETURN_ON_FALSE(state->day <= days[state->month - 1] +
                        (state->month == 2 && leap),
                        ESP_ERR_INVALID_RESPONSE, TAG, "invalid calendar date");
    state->seconds_of_day = state->hour * 3600 + state->minute * 60 + state->second;
    return ESP_OK;
}
