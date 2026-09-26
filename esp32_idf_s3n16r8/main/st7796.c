#include "st7796.h"
#include "app_config.h"

#include <string.h>
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_attr.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TRANSFER_ROWS 16

static const char *TAG = "st7796";
static spi_device_handle_t s_spi;
static DMA_ATTR uint8_t s_transfer[LCD_WIDTH * TRANSFER_ROWS * 2];

static esp_err_t transmit(bool data, const void *buffer, size_t bytes) {
    ESP_RETURN_ON_ERROR(gpio_set_level(LCD_DC_PIN, data), TAG, "DC");
    spi_transaction_t transaction = {
        .length = bytes * 8,
        .tx_buffer = buffer,
    };
    return spi_device_polling_transmit(s_spi, &transaction);
}

static esp_err_t command(uint8_t cmd, const uint8_t *data, size_t bytes) {
    ESP_RETURN_ON_ERROR(transmit(false, &cmd, 1), TAG, "command 0x%02x", cmd);
    if (bytes) {
        ESP_RETURN_ON_ERROR(transmit(true, data, bytes), TAG, "command data");
    }
    return ESP_OK;
}

esp_err_t st7796_init(void) {
    const gpio_config_t outputs = {
        .pin_bit_mask = (1ULL << LCD_LED_PIN) | (1ULL << LCD_DC_PIN) |
                        (1ULL << LCD_RESET_PIN),
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&outputs), TAG, "output pins");
#ifdef CONFIG_MINIBOX_LCD_BACKLIGHT_ACTIVE_LOW
    const int off_level = 1;
#else
    const int off_level = 0;
#endif
    ESP_RETURN_ON_ERROR(gpio_set_level(LCD_LED_PIN, off_level), TAG, "backlight off");
    ESP_RETURN_ON_ERROR(gpio_set_level(LCD_RESET_PIN, 1), TAG, "reset high");
    vTaskDelay(pdMS_TO_TICKS(10));
    ESP_RETURN_ON_ERROR(gpio_set_level(LCD_RESET_PIN, 0), TAG, "reset low");
    vTaskDelay(pdMS_TO_TICKS(20));
    ESP_RETURN_ON_ERROR(gpio_set_level(LCD_RESET_PIN, 1), TAG, "reset release");
    vTaskDelay(pdMS_TO_TICKS(120));

    const spi_bus_config_t bus = {
        .mosi_io_num = LCD_SDI_PIN,
        .miso_io_num = -1,
        .sclk_io_num = LCD_SCK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = sizeof(s_transfer),
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO),
                        TAG, "SPI bus");
    const spi_device_interface_config_t device = {
        .clock_speed_hz = LCD_SPI_HZ,
        .mode = 0,
        .spics_io_num = LCD_CS_PIN,
        .queue_size = 1,
    };
    ESP_RETURN_ON_ERROR(spi_bus_add_device(SPI2_HOST, &device, &s_spi),
                        TAG, "SPI device");

    ESP_RETURN_ON_ERROR(command(0x01, NULL, 0), TAG, "software reset");
    vTaskDelay(pdMS_TO_TICKS(150));
    ESP_RETURN_ON_ERROR(command(0x11, NULL, 0), TAG, "sleep out");
    vTaskDelay(pdMS_TO_TICKS(120));

    /* ST7796 extended-command unlock, landscape (MV|MX), and RGB565. */
    uint8_t madctl = 0x60;
#ifdef CONFIG_MINIBOX_LCD_BGR
    madctl |= 0x08;
#endif
    const struct {
        uint8_t cmd;
        uint8_t size;
        uint8_t data[14];
    } init[] = {
        {0xF0, 1, {0xC3}},
        {0xF0, 1, {0x96}},
        {0x36, 1, {madctl}},
        {0x3A, 1, {0x55}},
        {0xB4, 1, {0x01}},
        {0xB7, 1, {0xC6}},
        {0xE8, 8, {0x40, 0x8A, 0x00, 0x00, 0x29, 0x19, 0xA5, 0x33}},
        {0xC1, 1, {0x06}},
        {0xC2, 1, {0xA7}},
        {0xC5, 1, {0x18}},
        {0xE0, 14, {0xF0, 0x09, 0x0B, 0x06, 0x04, 0x15, 0x2F,
                    0x54, 0x42, 0x3C, 0x17, 0x14, 0x18, 0x1B}},
        {0xE1, 14, {0xE0, 0x09, 0x0B, 0x06, 0x04, 0x03, 0x2B,
                    0x43, 0x42, 0x3B, 0x16, 0x14, 0x17, 0x1B}},
        {0xF0, 1, {0x3C}},
        {0xF0, 1, {0x69}},
    };
    for (size_t i = 0; i < sizeof(init) / sizeof(init[0]); ++i) {
        ESP_RETURN_ON_ERROR(command(init[i].cmd, init[i].data, init[i].size),
                            TAG, "initialization");
    }
#ifdef CONFIG_MINIBOX_LCD_INVERTED
    ESP_RETURN_ON_ERROR(command(0x21, NULL, 0), TAG, "inversion on");
#else
    ESP_RETURN_ON_ERROR(command(0x20, NULL, 0), TAG, "inversion off");
#endif
    ESP_RETURN_ON_ERROR(command(0x13, NULL, 0), TAG, "normal mode");
    ESP_RETURN_ON_ERROR(command(0x29, NULL, 0), TAG, "display on");
    vTaskDelay(pdMS_TO_TICKS(20));
    ESP_LOGI(TAG, "480x320 RGB565 SPI=%d Hz LED=9 SCK=10 SDI=11 DC=12 RST=13 CS=14",
             LCD_SPI_HZ);
    return ESP_OK;
}

esp_err_t st7796_backlight_on(void) {
#ifdef CONFIG_MINIBOX_LCD_BACKLIGHT_ACTIVE_LOW
    return gpio_set_level(LCD_LED_PIN, 0);
#else
    return gpio_set_level(LCD_LED_PIN, 1);
#endif
}

esp_err_t st7796_draw_rows(int y, int height, const uint8_t *pixels) {
    ESP_RETURN_ON_FALSE(s_spi, ESP_ERR_INVALID_STATE, TAG, "not initialized");
    ESP_RETURN_ON_FALSE(pixels && y >= 0 && y < LCD_HEIGHT && height > 0 &&
                        height <= LCD_HEIGHT - y,
                        ESP_ERR_INVALID_ARG, TAG, "invalid row range");
    uint16_t end = y + height - 1;
    const uint8_t columns[] = {0, 0, (LCD_WIDTH - 1) >> 8, (LCD_WIDTH - 1) & 0xff};
    const uint8_t rows[] = {(uint16_t)y >> 8, y & 0xff, end >> 8, end & 0xff};
    ESP_RETURN_ON_ERROR(command(0x2A, columns, sizeof(columns)), TAG, "columns");
    ESP_RETURN_ON_ERROR(command(0x2B, rows, sizeof(rows)), TAG, "rows");
    ESP_RETURN_ON_ERROR(command(0x2C, NULL, 0), TAG, "memory write");
    for (int row = 0; row < height; row += TRANSFER_ROWS) {
        int count = height - row;
        if (count > TRANSFER_ROWS) count = TRANSFER_ROWS;
        size_t bytes = count * LCD_WIDTH * 2;
        /* PSRAM cannot be used directly by this SPI DMA path. */
        memcpy(s_transfer, pixels + row * LCD_WIDTH * 2, bytes);
        ESP_RETURN_ON_ERROR(transmit(true, s_transfer, bytes), TAG, "pixels");
    }
    return ESP_OK;
}
