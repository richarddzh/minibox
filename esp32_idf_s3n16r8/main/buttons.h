#pragma once

#include <stdbool.h>
#include <stdint.h>

enum {
    BUTTON_PAD_A = 0x01, BUTTON_PAD_B = 0x02,
    BUTTON_PAD_SELECT = 0x04, BUTTON_PAD_START = 0x08,
    BUTTON_PAD_UP = 0x10, BUTTON_PAD_DOWN = 0x20,
    BUTTON_PAD_LEFT = 0x40, BUTTON_PAD_RIGHT = 0x80,
};

typedef enum {
    APP_INPUT_MAIN_PRESSED,
    APP_INPUT_MAIN_RELEASED,
    APP_INPUT_JOYSTICK_PRESSED,
    APP_INPUT_LEFT,
    APP_INPUT_RIGHT,
    APP_INPUT_UP,
    APP_INPUT_DOWN,
    APP_INPUT_RECORD_PRESSED,
    APP_INPUT_RECORD_RELEASED,
    APP_INPUT_BACK,
} app_input_event_t;

void buttons_init(void);
bool buttons_poll(app_input_event_t *event);
bool buttons_pressed(void);
/* Debounced levels, including diagonal directions; not navigation repeats. */
uint8_t buttons_gamepad(void);
const char *buttons_event_name(app_input_event_t event);
