#include "onboard_led.h"
#include "app_config.h"

#include <stdint.h>
#include <driver/rmt_tx.h>
#include <freertos/FreeRTOS.h>

#define RGB_RMT_RESOLUTION_HZ 10000000

esp_err_t onboard_led_off(void)
{
    rmt_channel_handle_t channel = NULL;
    rmt_encoder_handle_t encoder = NULL;
    bool enabled = false;
    const rmt_tx_channel_config_t channel_config = {
        .gpio_num = APP_ONBOARD_RGB_PIN,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = RGB_RMT_RESOLUTION_HZ,
        .mem_block_symbols = 64,
        .trans_queue_depth = 1,
    };
    esp_err_t err = rmt_new_tx_channel(&channel_config, &channel);
    if (err != ESP_OK) {
        return err;
    }

    const rmt_bytes_encoder_config_t encoder_config = {
        .bit0 = {
            .duration0 = 3,
            .level0 = 1,
            .duration1 = 9,
            .level1 = 0,
        },
        .bit1 = {
            .duration0 = 9,
            .level0 = 1,
            .duration1 = 3,
            .level1 = 0,
        },
        .flags.msb_first = 1,
    };
    err = rmt_new_bytes_encoder(&encoder_config, &encoder);
    if (err == ESP_OK) {
        err = rmt_enable(channel);
        enabled = err == ESP_OK;
    }
    if (err == ESP_OK) {
        const uint8_t off_grb[3] = {0, 0, 0};
        const rmt_transmit_config_t transmit_config = {
            .loop_count = 0,
        };
        err = rmt_transmit(channel, encoder, off_grb, sizeof(off_grb),
                           &transmit_config);
        if (err == ESP_OK) {
            err = rmt_tx_wait_all_done(channel, pdMS_TO_TICKS(100));
        }
    }

    if (enabled) {
        rmt_disable(channel);
    }
    if (encoder) {
        rmt_del_encoder(encoder);
    }
    if (channel) {
        rmt_del_channel(channel);
    }
    return err;
}
