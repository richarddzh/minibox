#pragma once

#include "joystick.h"
#include "audio_test_logic.h"

esp_err_t test_screen_init(void);
esp_err_t test_screen_message(const char *message, const char *detail);
esp_err_t test_screen_update(const joystick_state_t *state, audio_phase_t audio);
esp_err_t test_screen_audio_init(void);
esp_err_t test_screen_audio_update(const bool pressed[3], audio_phase_t audio);
