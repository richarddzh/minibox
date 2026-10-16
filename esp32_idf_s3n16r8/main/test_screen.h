#pragma once

#include "joystick.h"
#include "audio_test_logic.h"
#include "app_config.h"
#include "rtc_test.h"

esp_err_t test_screen_init(void);
esp_err_t test_screen_message(const char *message, const char *detail);
esp_err_t test_screen_update(const joystick_state_t *state, audio_phase_t audio);
esp_err_t test_screen_audio_init(void);
esp_err_t test_screen_audio_update(const bool pressed[BUTTON_COUNT], audio_phase_t audio);
esp_err_t test_screen_hardware_update(const bool pressed[BUTTON_COUNT],
                                     const joystick_state_t *stick, esp_err_t stick_error,
                                     const rtc_test_state_t *rtc, esp_err_t rtc_error,
                                     audio_phase_t audio);
