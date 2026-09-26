idf_component_register(
    SRCS "app_main.c" "st7796.c" "joystick.c" "joystick_logic.c" "test_screen.c" "bitmap_font.c" "audio_test.c" "audio_test_logic.c"
    INCLUDE_DIRS "."
    REQUIRES esp_driver_gpio esp_driver_spi esp_driver_rmt esp_driver_i2s esp_lcd esp_lcd_st7796 esp_adc esp_timer esp_psram spiffs
)
