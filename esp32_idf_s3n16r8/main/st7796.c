#include "st7796.h"
#include "app_config.h"

#include <string.h>
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_lcd_io_spi.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_st7796.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#define TRANSFER_ROWS 16

static const char *TAG = "st7796";
static esp_lcd_panel_handle_t s_panel;
static SemaphoreHandle_t s_transfer_done;
static DMA_ATTR uint8_t s_transfer[LCD_WIDTH * TRANSFER_ROWS * 2];
static uint32_t s_transfer_count;

static bool on_color_done(esp_lcd_panel_io_handle_t io,
                          esp_lcd_panel_io_event_data_t *event, void *context) {
    (void)io;
    (void)event;
    BaseType_t wake = pdFALSE;
    xSemaphoreGiveFromISR((SemaphoreHandle_t)context, &wake);
    return wake == pdTRUE;
}

esp_err_t st7796_init(void) {
    const gpio_config_t backlight = {
        .pin_bit_mask = 1ULL << LCD_LED_PIN,
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&backlight), TAG, "backlight pin");
#ifdef CONFIG_MINIBOX_LCD_BACKLIGHT_ACTIVE_LOW
    const int off_level = 1;
#else
    const int off_level = 0;
#endif
    ESP_RETURN_ON_ERROR(gpio_set_level(LCD_LED_PIN, off_level), TAG, "backlight off");

    s_transfer_done = xSemaphoreCreateBinary();
    ESP_RETURN_ON_FALSE(s_transfer_done, ESP_ERR_NO_MEM, TAG, "transfer semaphore");

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

    esp_lcd_panel_io_handle_t io;
    const esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num = LCD_CS_PIN,
        .dc_gpio_num = LCD_DC_PIN,
        .spi_mode = 0,
        .pclk_hz = LCD_SPI_HZ,
        .trans_queue_depth = 2,
        .on_color_trans_done = on_color_done,
        .user_ctx = s_transfer_done,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi(SPI2_HOST, &io_config, &io),
                        TAG, "SPI panel IO");

#ifdef CONFIG_MINIBOX_LCD_BGR
    const lcd_color_rgb_endian_t color_order = LCD_RGB_ENDIAN_BGR;
#else
    const lcd_color_rgb_endian_t color_order = LCD_RGB_ENDIAN_RGB;
#endif
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = LCD_RESET_PIN,
        .rgb_endian = color_order,
        .bits_per_pixel = 16,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_st7796(io, &panel_config, &s_panel),
                        TAG, "create ST7796 panel");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(s_panel), TAG, "panel reset");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(s_panel), TAG, "panel init");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_swap_xy(s_panel, true), TAG, "landscape");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_mirror(s_panel, false, false), TAG, "orientation");
#ifdef CONFIG_MINIBOX_LCD_INVERTED
    ESP_RETURN_ON_ERROR(esp_lcd_panel_invert_color(s_panel, true), TAG, "inversion on");
#else
    ESP_RETURN_ON_ERROR(esp_lcd_panel_invert_color(s_panel, false), TAG, "inversion off");
#endif
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(s_panel, true), TAG, "display on");
    vTaskDelay(pdMS_TO_TICKS(20));
    ESP_LOGI(TAG, "official esp_lcd_st7796 480x320 RGB565 SPI=%d Hz LED=9 SCK=10 SDI=11 DC=12 RST=13 CS=14",
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
    return st7796_draw_region(0, y, LCD_WIDTH, height, pixels, LCD_WIDTH * 2);
}

esp_err_t st7796_draw_region(int x, int y, int width, int height,
                             const uint8_t *pixels, size_t stride_bytes) {
    ESP_RETURN_ON_FALSE(s_panel, ESP_ERR_INVALID_STATE, TAG, "not initialized");
    ESP_RETURN_ON_FALSE(pixels && x >= 0 && x < LCD_WIDTH && y >= 0 &&
                        y < LCD_HEIGHT && width > 0 && height > 0 &&
                        width <= LCD_WIDTH - x && height <= LCD_HEIGHT - y &&
                        stride_bytes >= (size_t)width * 2,
                        ESP_ERR_INVALID_ARG, TAG, "invalid rectangle");
    for (int row = 0; row < height; row += TRANSFER_ROWS) {
        int count = height - row;
        if (count > TRANSFER_ROWS) count = TRANSFER_ROWS;
        for (int line = 0; line < count; ++line) {
            memcpy(s_transfer + line * width * 2,
                   pixels + (row + line) * stride_bytes, width * 2);
        }
        ESP_RETURN_ON_ERROR(esp_lcd_panel_draw_bitmap(s_panel, x, y + row,
                            x + width, y + row + count, s_transfer), TAG, "pixel transfer");
        ESP_RETURN_ON_FALSE(xSemaphoreTake(s_transfer_done, pdMS_TO_TICKS(1000)) == pdTRUE,
                            ESP_ERR_TIMEOUT, TAG, "SPI transfer timeout");
        ++s_transfer_count;
    }
    return ESP_OK;
}

uint32_t st7796_transfer_count(void) {
    return s_transfer_count;
}
