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

#define JOYSTICK_X_PIN CONFIG_MINIBOX_JOYSTICK_X_PIN
#define JOYSTICK_Y_PIN CONFIG_MINIBOX_JOYSTICK_Y_PIN
#define JOYSTICK_K_PIN CONFIG_MINIBOX_JOYSTICK_K_PIN

_Static_assert(JOYSTICK_X_PIN != JOYSTICK_Y_PIN &&
               JOYSTICK_X_PIN != JOYSTICK_K_PIN &&
               JOYSTICK_Y_PIN != JOYSTICK_K_PIN,
               "Joystick pins must be distinct");
