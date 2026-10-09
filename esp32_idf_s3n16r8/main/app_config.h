#pragma once

#include "sdkconfig.h"

#define LCD_WIDTH 480
#define LCD_HEIGHT 320
#define LCD_LED_PIN 9
#define LCD_SCK_PIN 10
#define LCD_SDI_PIN 11
#define LCD_DC_PIN 12
#define LCD_RESET_PIN 13
#define LCD_CS_PIN 14
#define LCD_SPI_HZ (CONFIG_MINIBOX_LCD_SPI_MHZ * 1000000)
#define ONBOARD_LED_PIN 48
#define AUDIO_BCLK_PIN 40
#define AUDIO_WS_PIN 39
#define MIC_SD_PIN 17
#define SPEAKER_DIN_PIN 41
#define SPEAKER_SD_MODE_PIN 47
#define SPEAKER_GAIN_PIN 21
#define AUDIO_SAMPLE_RATE 16000
#define AUDIO_MAX_SECONDS 3
#define BUTTON_1_PIN 4
#define RECORD_BUTTON_PIN 5
#define BUTTON_3_PIN 6
#define BUTTON_4_PIN 7
#define BUTTON_COUNT 4
#define RTC_SDA_PIN 16
#define RTC_SCL_PIN 15

#define MINIBOX_STRINGIFY_INNER(value) #value
#define MINIBOX_STRINGIFY(value) MINIBOX_STRINGIFY_INNER(value)
#define BUTTON_GPIO_LABELS "GPIO" MINIBOX_STRINGIFY(BUTTON_1_PIN) "/" \
                          MINIBOX_STRINGIFY(RECORD_BUTTON_PIN) "/" \
                          MINIBOX_STRINGIFY(BUTTON_3_PIN) "/" \
                          MINIBOX_STRINGIFY(BUTTON_4_PIN)
#define RECORD_GPIO_LABEL "GPIO" MINIBOX_STRINGIFY(RECORD_BUTTON_PIN)

#define JOYSTICK_X_PIN CONFIG_MINIBOX_JOYSTICK_X_PIN
#define JOYSTICK_Y_PIN CONFIG_MINIBOX_JOYSTICK_Y_PIN
#define JOYSTICK_K_PIN (-1)
#ifdef CONFIG_MINIBOX_JOYSTICK_K_ACTIVE_LOW
#define JOYSTICK_K_ACTIVE_LEVEL 0
#else
#define JOYSTICK_K_ACTIVE_LEVEL 1
#endif

_Static_assert(JOYSTICK_X_PIN != JOYSTICK_Y_PIN &&
               JOYSTICK_X_PIN != JOYSTICK_K_PIN &&
               JOYSTICK_Y_PIN != JOYSTICK_K_PIN,
               "Joystick pins must be distinct");
_Static_assert(JOYSTICK_X_PIN >= 1 && JOYSTICK_X_PIN <= 10 &&
               JOYSTICK_Y_PIN >= 1 && JOYSTICK_Y_PIN <= 10,
               "ESP32-S3 joystick axes require ADC1 GPIO1-10, not GPIO40/41");
_Static_assert(JOYSTICK_X_PIN != BUTTON_1_PIN &&
               JOYSTICK_X_PIN != RECORD_BUTTON_PIN &&
               JOYSTICK_X_PIN != BUTTON_3_PIN &&
               JOYSTICK_X_PIN != BUTTON_4_PIN &&
               JOYSTICK_Y_PIN != BUTTON_1_PIN &&
               JOYSTICK_Y_PIN != RECORD_BUTTON_PIN &&
               JOYSTICK_Y_PIN != BUTTON_3_PIN &&
               JOYSTICK_Y_PIN != BUTTON_4_PIN &&
               JOYSTICK_K_PIN != BUTTON_1_PIN &&
               JOYSTICK_K_PIN != RECORD_BUTTON_PIN &&
               JOYSTICK_K_PIN != BUTTON_3_PIN &&
               JOYSTICK_K_PIN != BUTTON_4_PIN,
               "Joystick pins must not conflict with keypad pins");
_Static_assert(MIC_SD_PIN != RTC_SDA_PIN && MIC_SD_PIN != RTC_SCL_PIN &&
               SPEAKER_SD_MODE_PIN != BUTTON_4_PIN &&
               SPEAKER_GAIN_PIN != RTC_SDA_PIN && SPEAKER_GAIN_PIN != RTC_SCL_PIN &&
               SPEAKER_SD_MODE_PIN != RTC_SDA_PIN && SPEAKER_SD_MODE_PIN != RTC_SCL_PIN,
               "Revised carrier GPIOs must not share peripheral functions");
_Static_assert(JOYSTICK_X_PIN != SPEAKER_SD_MODE_PIN &&
               JOYSTICK_Y_PIN != SPEAKER_SD_MODE_PIN &&
               JOYSTICK_K_PIN != SPEAKER_SD_MODE_PIN &&
               JOYSTICK_X_PIN != SPEAKER_GAIN_PIN &&
               JOYSTICK_Y_PIN != SPEAKER_GAIN_PIN &&
               JOYSTICK_K_PIN != SPEAKER_GAIN_PIN,
               "Joystick pins must not conflict with amplifier control pins");
_Static_assert(JOYSTICK_K_PIN != AUDIO_BCLK_PIN &&
               JOYSTICK_K_PIN != AUDIO_WS_PIN &&
               JOYSTICK_K_PIN != MIC_SD_PIN &&
               JOYSTICK_K_PIN != SPEAKER_DIN_PIN &&
               JOYSTICK_K_PIN != RTC_SDA_PIN &&
               JOYSTICK_K_PIN != RTC_SCL_PIN,
               "Joystick button must not share I2S or RTC pins");
