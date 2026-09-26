#include "joystick.h"
#include "app_config.h"

#include <limits.h>
#include <string.h>
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_check.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "joystick";
static adc_oneshot_unit_handle_t s_adc;
static adc_channel_t s_x_channel;
static adc_channel_t s_y_channel;

static esp_err_t read_axes(int *x, int *y) {
    int total_x = 0, total_y = 0;
    for (int i = 0; i < 8; ++i) {
        int raw_x, raw_y;
        ESP_RETURN_ON_ERROR(adc_oneshot_read(s_adc, s_x_channel, &raw_x), TAG, "X ADC");
        ESP_RETURN_ON_ERROR(adc_oneshot_read(s_adc, s_y_channel, &raw_y), TAG, "Y ADC");
        total_x += raw_x;
        total_y += raw_y;
    }
    *x = total_x / 8;
    *y = total_y / 8;
    return ESP_OK;
}

esp_err_t joystick_init(joystick_state_t *state) {
    ESP_RETURN_ON_FALSE(state, ESP_ERR_INVALID_ARG, TAG, "null state");
    memset(state, 0, sizeof(*state));
    adc_unit_t x_unit, y_unit;
    ESP_RETURN_ON_ERROR(adc_oneshot_io_to_channel(JOYSTICK_X_PIN, &x_unit, &s_x_channel),
                        TAG, "X pin");
    ESP_RETURN_ON_ERROR(adc_oneshot_io_to_channel(JOYSTICK_Y_PIN, &y_unit, &s_y_channel),
                        TAG, "Y pin");
    ESP_RETURN_ON_FALSE(x_unit == ADC_UNIT_1 && y_unit == ADC_UNIT_1,
                        ESP_ERR_INVALID_ARG, TAG, "axes require ADC1 pins");
    const adc_oneshot_unit_init_cfg_t unit = {.unit_id = ADC_UNIT_1};
    ESP_RETURN_ON_ERROR(adc_oneshot_new_unit(&unit, &s_adc), TAG, "ADC unit");
    const adc_oneshot_chan_cfg_t channel = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    ESP_RETURN_ON_ERROR(adc_oneshot_config_channel(s_adc, s_x_channel, &channel),
                        TAG, "X channel");
    ESP_RETURN_ON_ERROR(adc_oneshot_config_channel(s_adc, s_y_channel, &channel),
                        TAG, "Y channel");
    const gpio_config_t button = {
        .pin_bit_mask = 1ULL << JOYSTICK_K_PIN,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&button), TAG, "button pin");
    ESP_LOGI(TAG, "X=%d Y=%d K=%d (active low); release joystick during calibration",
             JOYSTICK_X_PIN, JOYSTICK_Y_PIN, JOYSTICK_K_PIN);
    int total_x = 0, total_y = 0;
    int min_x = INT_MAX, min_y = INT_MAX, max_x = 0, max_y = 0;
    for (int i = 0; i < 64; ++i) {
        int x, y;
        ESP_RETURN_ON_ERROR(read_axes(&x, &y), TAG, "calibration");
        total_x += x;
        total_y += y;
        if (x < min_x) min_x = x;
        if (x > max_x) max_x = x;
        if (y < min_y) min_y = y;
        if (y > max_y) max_y = y;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    state->center_x = total_x / 64;
    state->center_y = total_y / 64;
    ESP_LOGI(TAG, "centers X=%d Y=%d; spans X=%d Y=%d",
             state->center_x, state->center_y, max_x - min_x, max_y - min_y);
    ESP_RETURN_ON_FALSE(joystick_center_valid(state->center_x) &&
                        joystick_center_valid(state->center_y) &&
                        max_x - min_x <= 300 && max_y - min_y <= 300,
                        ESP_ERR_INVALID_STATE, TAG,
                        "calibration invalid; check wiring, release stick, reset");
    state->button.candidate = gpio_get_level(JOYSTICK_K_PIN) == 0;
    state->button.pressed = state->button.candidate;
    state->button.changed_ms = esp_timer_get_time() / 1000;
    return joystick_read(state);
}

esp_err_t joystick_read(joystick_state_t *state) {
    ESP_RETURN_ON_FALSE(state && s_adc && joystick_center_valid(state->center_x) &&
                        joystick_center_valid(state->center_y),
                        ESP_ERR_INVALID_STATE, TAG, "not calibrated");
    ESP_RETURN_ON_ERROR(read_axes(&state->raw_x, &state->raw_y), TAG, "read axes");
    bool invert_x = false, invert_y = false;
#ifdef CONFIG_MINIBOX_JOYSTICK_X_INVERTED
    invert_x = true;
#endif
#ifdef CONFIG_MINIBOX_JOYSTICK_Y_INVERTED
    invert_y = true;
#endif
    state->percent_x = joystick_percent(state->raw_x, state->center_x, invert_x);
    state->percent_y = joystick_percent(state->raw_y, state->center_y, invert_y);
    state->direction_x = joystick_direction(state->percent_x, state->direction_x);
    state->direction_y = joystick_direction(state->percent_y, state->direction_y);
    joystick_button_update(&state->button, gpio_get_level(JOYSTICK_K_PIN) == 0,
                           esp_timer_get_time() / 1000);
    return ESP_OK;
}
