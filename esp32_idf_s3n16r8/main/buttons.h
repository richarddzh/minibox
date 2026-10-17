#pragma once

#include <stdbool.h>

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
const char *buttons_event_name(app_input_event_t event);
