#include "joystick.h"
#include "st7796.h"
#include "test_screen.h"
#include "app_config.h"

#include <inttypes.h>
#include "driver/gpio.h"
#include "driver/rmt_tx.h"
#include "esp_check.h"
#include "esp_psram.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

static const char *TAG = "minibox";

typedef struct {
    joystick_state_t state;
    esp_err_t error;
} input_sample_t;

static QueueHandle_t s_samples;
static input_sample_t s_input;

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
    const rmt_copy_encoder_config_t copy = {0};
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
    ESP_LOGI(TAG, "Onboard RGB LED off (GPIO%d)", ONBOARD_LED_PIN);
}

static void sample_joystick(void *arg) {
    (void)arg;
    TickType_t last_wake = xTaskGetTickCount();
    for (;;) {
        s_input.error = joystick_read(&s_input.state);
        xQueueOverwrite(s_samples, &s_input);
        if (s_input.error != ESP_OK) {
            vTaskDelete(NULL);
        }
        xTaskDelayUntil(&last_wake, pdMS_TO_TICKS(10));
    }
}

static void show_input_error(esp_err_t error) {
    ESP_LOGE(TAG, "Joystick stopped: %s; check wiring, release stick and reset",
             esp_err_to_name(error));
    ESP_ERROR_CHECK(test_screen_message("JOYSTICK ERROR", "CHECK WIRING - RESET"));
}

void app_main(void) {
    onboard_led_off();
    ESP_LOGI(TAG, "ESP32-S3 N16R8 hardware test; PSRAM=%u bytes",
             (unsigned)esp_psram_get_size());
    ESP_ERROR_CHECK(st7796_init());
    ESP_ERROR_CHECK(test_screen_init());
    ESP_ERROR_CHECK(test_screen_message("CALIBRATING", "RELEASE STICK AND K"));
    vTaskDelay(pdMS_TO_TICKS(1500));
    esp_err_t error = joystick_init(&s_input.state);
    if (error != ESP_OK) {
        show_input_error(error);
        return;
    }
    s_samples = xQueueCreate(1, sizeof(input_sample_t));
    ESP_ERROR_CHECK(s_samples ? ESP_OK : ESP_ERR_NO_MEM);
    /* Input sampling remains responsive during blocking SPI screen transfers. */
    BaseType_t created = xTaskCreate(sample_joystick, "joystick", 4096, NULL, 5, NULL);
    ESP_ERROR_CHECK(created == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);

    unsigned log_counter = 0;
#ifdef CONFIG_MINIBOX_LCD_STATIC_TEST
    bool first_frame = true;
#endif
    for (;;) {
        input_sample_t sample;
        if (xQueueReceive(s_samples, &sample, pdMS_TO_TICKS(1000)) != pdTRUE) {
            show_input_error(ESP_ERR_TIMEOUT);
            return;
        }
        if (sample.error != ESP_OK) {
            show_input_error(sample.error);
            return;
        }
#ifdef CONFIG_MINIBOX_LCD_STATIC_TEST
        if (first_frame) {
            ESP_ERROR_CHECK(test_screen_update(&sample.state));
            first_frame = false;
            ESP_LOGW(TAG, "STATIC DISPLAY: no further SPI writes; input sampling continues");
        }
#else
        ESP_ERROR_CHECK(test_screen_update(&sample.state));
#endif
        if (++log_counter >= 5) {
            log_counter = 0;
            ESP_LOGI(TAG, "X=%d (%d%% dir=%d) Y=%d (%d%% dir=%d) K_LEVEL=%d PRESSED=%d count=%" PRIu32 " LCD_TX=%" PRIu32,
                     sample.state.raw_x, sample.state.percent_x, sample.state.direction_x,
                     sample.state.raw_y, sample.state.percent_y, sample.state.direction_y,
                     sample.state.raw_k, sample.state.button.pressed, sample.state.button.presses,
                     st7796_transfer_count());
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
