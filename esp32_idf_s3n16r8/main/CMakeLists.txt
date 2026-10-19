set(common_sources "st7796.c" "joystick.c" "joystick_logic.c" "bitmap_font.c" "rtc_test.c")

if(CONFIG_MINIBOX_HARDWARE_TEST)
    set(app_sources "hardware_test_main.c" "radio_stream.c" "test_screen.c"
                    "audio_test.c" "audio_test_logic.c")
else()
    set(app_sources
        "app_main.c" "buttons.c" "onboard_led.c" "ring.c" "audio_io.c"
        "app_secret_store.c" "wifi_creds.c" "wifi_provision.c" "wifi_time.c"
        "rtc_pcf8563.c" "voice_chat.c" "chime_player.c"
        "doubao_asr_client.c" "doubao_agent_client.c" "doubao_tts_player.c"
        "radio_stations.c" "radio_player.c" "mem_log.c"
        "ui_screen.c" "ui_font.c" "ui_cat_avatar.c" "gui_shell.c"
        "app_chat_alarm.c" "app_settings.c" "app_radio.c"
        "app_tetris.c" "app_snake.c" "app_nes.c")
    set(app_requires nofrendo)
endif()

idf_component_register(
    SRCS ${common_sources} ${app_sources}
    INCLUDE_DIRS "."
    REQUIRES esp_driver_gpio esp_driver_spi esp_driver_rmt esp_driver_i2s
             esp_driver_i2c esp_lcd esp_lcd_st7796 esp_adc esp_timer esp_psram
             spiffs esp_wifi esp_event esp_netif nvs_flash esp_http_client
             esp_http_server esp-tls mbedtls esp_audio_codec lvgl ${app_requires}
)
