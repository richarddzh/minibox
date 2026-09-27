#include "app_config.h"
#include "audio_test.h"
#include "joystick_logic.h"
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
    bool pressed[3];
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
    const gpio_num_t pins[] = {BUTTON_1_PIN, RECORD_BUTTON_PIN, BUTTON_3_PIN};
    joystick_button_t buttons[3] = {0};
    TickType_t last_wake = xTaskGetTickCount();
    for (;;) {
        button_snapshot_t snapshot;
        int64_t now_ms = esp_timer_get_time() / 1000;
        for (int i = 0; i < 3; ++i) {
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
        audio_test_submit(&buttons[1]);
        xQueueOverwrite(s_button_states, &snapshot);
        xTaskDelayUntil(&last_wake, pdMS_TO_TICKS(10));
    }
}

void app_main(void) {
    onboard_led_off();
    ESP_LOGI(TAG, "ESP32-S3 N16R8 microphone test; PSRAM=%u bytes",
             (unsigned)esp_psram_get_size());
    ESP_ERROR_CHECK(st7796_init());
    ESP_ERROR_CHECK(test_screen_audio_init());
    const gpio_config_t inputs = {
        .pin_bit_mask = (1ULL << BUTTON_1_PIN) |
                        (1ULL << RECORD_BUTTON_PIN) |
                        (1ULL << BUTTON_3_PIN),
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
    ESP_ERROR_CHECK(audio_test_start());
    BaseType_t created = xTaskCreate(sample_buttons, "buttons", 4096,
                                     NULL, 5, NULL);
    ESP_ERROR_CHECK(created == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_LOGI(TAG, "Buttons GPIO40/41/42 ready; hold GPIO41 to record (max 3s)");
    for (;;) {
        button_snapshot_t snapshot;
        if (xQueueReceive(s_button_states, &snapshot, pdMS_TO_TICKS(1000)) != pdTRUE) {
            ESP_LOGE(TAG, "Button sampling stopped");
            ESP_ERROR_CHECK(test_screen_message("BUTTON ERROR", "CHECK GPIO40/41/42"));
            return;
        }
        ESP_ERROR_CHECK(test_screen_audio_update(snapshot.pressed, audio_test_phase()));
    }
}
