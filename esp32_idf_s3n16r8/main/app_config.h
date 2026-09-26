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
#define AUDIO_BCLK_PIN 16
#define AUDIO_WS_PIN 17
#define MIC_SD_PIN 18
#define SPEAKER_DIN_PIN 15
#define SPEAKER_SD_MODE_PIN 7
#define SPEAKER_GAIN_PIN 8
#define AUDIO_SAMPLE_RATE 16000
#define AUDIO_MAX_SECONDS 3

#define JOYSTICK_X_PIN CONFIG_MINIBOX_JOYSTICK_X_PIN
#define JOYSTICK_Y_PIN CONFIG_MINIBOX_JOYSTICK_Y_PIN
#define JOYSTICK_K_PIN CONFIG_MINIBOX_JOYSTICK_K_PIN
#ifdef CONFIG_MINIBOX_JOYSTICK_K_ACTIVE_LOW
#define JOYSTICK_K_ACTIVE_LEVEL 0
#else
#define JOYSTICK_K_ACTIVE_LEVEL 1
#endif

_Static_assert(JOYSTICK_X_PIN != JOYSTICK_Y_PIN &&
               JOYSTICK_X_PIN != JOYSTICK_K_PIN &&
               JOYSTICK_Y_PIN != JOYSTICK_K_PIN,
               "Joystick pins must be distinct");
_Static_assert(JOYSTICK_X_PIN != SPEAKER_SD_MODE_PIN &&
               JOYSTICK_Y_PIN != SPEAKER_SD_MODE_PIN &&
               JOYSTICK_K_PIN != SPEAKER_SD_MODE_PIN &&
               JOYSTICK_X_PIN != SPEAKER_GAIN_PIN &&
               JOYSTICK_Y_PIN != SPEAKER_GAIN_PIN &&
               JOYSTICK_K_PIN != SPEAKER_GAIN_PIN,
               "Joystick pins must not conflict with amplifier control pins");
