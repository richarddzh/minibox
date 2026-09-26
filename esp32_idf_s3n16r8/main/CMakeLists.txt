idf_component_register(
    SRCS "app_main.c" "st7796.c" "joystick.c" "joystick_logic.c" "test_screen.c"
    INCLUDE_DIRS "."
    REQUIRES driver esp_adc esp_timer esp_psram
)
