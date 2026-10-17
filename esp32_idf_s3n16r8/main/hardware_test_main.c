#include "app_config.h"
#include "audio_test.h"
#include "joystick_logic.h"
#include "joystick.h"
#include "rtc_test.h"
#include "radio_stream.h"
#include "st7796.h"
#include "test_screen.h"

#include "driver/gpio.h"
#include "driver/rmt_tx.h"
#include "esp_check.h"
#include "esp_psram.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

static const char *TAG = "minibox";
static QueueHandle_t s_button_states;

typedef struct {
    bool pressed[BUTTON_COUNT];
} button_snapshot_t;

static void onboard_led_off(void) {
    rmt_channel_handle_t channel;
    rmt_encoder_handle_t encoder;
    const rmt_tx_channel_config_t config = {
        .gpio_num = ONBOARD_LED_PIN,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10000000,
        .mem_block_symbols = 64,
        .trans_queue_depth = 1,
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&config, &channel));
    const rmt_copy_encoder_config_t copy = {};
    ESP_ERROR_CHECK(rmt_new_copy_encoder(&copy, &encoder));
    ESP_ERROR_CHECK(rmt_enable(channel));
    /* WS2812 retains its color when the pin is merely pulled low. Send black. */
    rmt_symbol_word_t black[24];
    for (unsigned i = 0; i < 24; ++i) {
        black[i] = (rmt_symbol_word_t){
            .level0 = 1, .duration0 = 3,
            .level1 = 0, .duration1 = 9,
        };
    }
    const rmt_transmit_config_t transmit = {.flags.eot_level = 0};
    ESP_ERROR_CHECK(rmt_transmit(channel, encoder, black, sizeof(black), &transmit));
    ESP_ERROR_CHECK(rmt_tx_wait_all_done(channel, 100));
    vTaskDelay(pdMS_TO_TICKS(1));
    ESP_ERROR_CHECK(rmt_disable(channel));
    ESP_ERROR_CHECK(rmt_del_encoder(encoder));
    ESP_ERROR_CHECK(rmt_del_channel(channel));
    ESP_ERROR_CHECK(gpio_set_direction(ONBOARD_LED_PIN, GPIO_MODE_OUTPUT));
    ESP_ERROR_CHECK(gpio_set_level(ONBOARD_LED_PIN, 0));
}

static void sample_buttons(void *arg) {
    (void)arg;
    const gpio_num_t pins[] = {BUTTON_1_PIN, RECORD_BUTTON_PIN, BUTTON_3_PIN, BUTTON_4_PIN};
    joystick_button_t buttons[BUTTON_COUNT] = {0};
    TickType_t last_wake = xTaskGetTickCount();
    for (;;) {
        button_snapshot_t snapshot;
        int64_t now_ms = esp_timer_get_time() / 1000;
        for (int i = 0; i < BUTTON_COUNT; ++i) {
            bool pressed = gpio_get_level(pins[i]) ==
#ifdef CONFIG_MINIBOX_BUTTON_ACTIVE_LOW
                           0;
#else
                           1;
#endif
            bool before = buttons[i].pressed;
            joystick_button_update(&buttons[i], pressed, now_ms);
            snapshot.pressed[i] = buttons[i].pressed;
            if (before != buttons[i].pressed) {
                ESP_LOGI(TAG, "GPIO%d %s", pins[i], buttons[i].pressed ? "pressed" : "released");
            }
        }
#ifndef CONFIG_MINIBOX_RADIO_TEST
        audio_test_submit(&buttons[1]);
#endif
        xQueueOverwrite(s_button_states, &snapshot);
        xTaskDelayUntil(&last_wake, pdMS_TO_TICKS(10));
    }
}

void app_main(void) {
    onboard_led_off();
    ESP_LOGI(TAG, "ESP32-S3 N16R8 six-device PCB test; PSRAM=%u bytes",
             (unsigned)esp_psram_get_size());
    ESP_ERROR_CHECK(st7796_init());
    ESP_ERROR_CHECK(test_screen_init());
    const gpio_config_t inputs = {
        .pin_bit_mask = (1ULL << BUTTON_1_PIN) |
                        (1ULL << RECORD_BUTTON_PIN) |
                        (1ULL << BUTTON_3_PIN) |
                        (1ULL << BUTTON_4_PIN),
        .mode = GPIO_MODE_INPUT,
#ifdef CONFIG_MINIBOX_BUTTON_ACTIVE_LOW
        .pull_up_en = GPIO_PULLUP_ENABLE,
#else
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
#endif
    };
    ESP_ERROR_CHECK(gpio_config(&inputs));
    s_button_states = xQueueCreate(1, sizeof(button_snapshot_t));
    ESP_ERROR_CHECK(s_button_states ? ESP_OK : ESP_ERR_NO_MEM);
#ifndef CONFIG_MINIBOX_RADIO_TEST
    ESP_ERROR_CHECK(audio_test_start());
#endif
    BaseType_t created = xTaskCreate(sample_buttons, "buttons", 4096,
                                     NULL, 5, NULL);
    ESP_ERROR_CHECK(created == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_LOGI(TAG, "Buttons " BUTTON_GPIO_LABELS " ready");
    joystick_state_t stick = {0};
    esp_err_t stick_error = joystick_init(&stick);
    if (stick_error != ESP_OK) {
        ESP_LOGE(TAG, "Joystick initialization failed: %s; center stick and reset",
                 esp_err_to_name(stick_error));
    }
    rtc_test_state_t rtc = {0};
    esp_err_t rtc_error = rtc_test_init();
    if (rtc_error != ESP_OK) {
        ESP_LOGE(TAG, "RTC initialization failed: %s", esp_err_to_name(rtc_error));
    }
#ifdef CONFIG_MINIBOX_RADIO_TEST
    ESP_ERROR_CHECK(radio_stream_start());
#else
    ESP_LOGI(TAG, "Hold " RECORD_GPIO_LABEL " to record (max 3s)");
#endif
    int64_t next_rtc_us = 0, next_log_us = 0;
    int64_t rtc_changed_us = 0;
    uint32_t previous_seconds = 0;
    bool rtc_seen = false;
    bool rtc_ready = rtc_error == ESP_OK;
    TickType_t last_wake = xTaskGetTickCount();
    for (;;) {
        button_snapshot_t snapshot;
        if (xQueueReceive(s_button_states, &snapshot, pdMS_TO_TICKS(1000)) != pdTRUE) {
            ESP_LOGE(TAG, "Button sampling stopped");
            ESP_ERROR_CHECK(test_screen_message("BUTTON ERROR", "CHECK A / B / X / Y"));
            return;
        }
        int64_t now = esp_timer_get_time();
        if (stick_error == ESP_OK) {
            stick_error = joystick_read(&stick);
        }
        if (rtc_ready && now >= next_rtc_us) {
            next_rtc_us = now + 1000000;
            rtc_error = rtc_test_read(&rtc);
            if (rtc_error == ESP_OK) {
                if (!rtc_seen) {
                    rtc_seen = true;
                    rtc_changed_us = now;
                } else if (rtc.seconds_of_day != previous_seconds) {
                    rtc_changed_us = now;
                    rtc.ticking = true;
                } else if (now - rtc_changed_us > 2500000) {
                    rtc.ticking = false;
                    rtc_error = ESP_ERR_INVALID_STATE;
                    ESP_LOGE(TAG, "RTC seconds are not advancing");
                }
                previous_seconds = rtc.seconds_of_day;
            } else {
                rtc.ticking = false;
            }
        }
        if (now >= next_log_us) {
            next_log_us = now + 2000000;
            if (stick_error == ESP_OK) {
                ESP_LOGI(TAG, "Joystick X=%d (%+d%%) Y=%d (%+d%%)",
                         stick.raw_x, stick.percent_x, stick.raw_y, stick.percent_y);
            }
            if (rtc_error == ESP_OK) {
                ESP_LOGI(TAG, "RTC %04d-%02d-%02d %02d:%02d:%02d %s",
                         rtc.year, rtc.month, rtc.day, rtc.hour, rtc.minute, rtc.second,
                         rtc.ticking ? "TICK OK" : "waiting for tick");
            }
        }
        ESP_ERROR_CHECK(test_screen_hardware_update(snapshot.pressed, &stick, stick_error,
                                                     &rtc, rtc_error, audio_test_phase()));
        xTaskDelayUntil(&last_wake, pdMS_TO_TICKS(100));
    }
}
