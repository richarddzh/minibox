#pragma once

#include "joystick.h"

esp_err_t test_screen_init(void);
esp_err_t test_screen_message(const char *message, const char *detail);
esp_err_t test_screen_update(const joystick_state_t *state);
