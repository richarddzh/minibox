#pragma once

#include "esp_err.h"
#include "joystick_logic.h"

typedef struct {
    int raw_x;
    int raw_y;
    int center_x;
    int center_y;
    int percent_x;
    int percent_y;
    int direction_x;
    int direction_y;
    joystick_button_t button;
} joystick_state_t;

esp_err_t joystick_init(joystick_state_t *state);
esp_err_t joystick_read(joystick_state_t *state);
