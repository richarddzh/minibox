#pragma once

#include "esp_err.h"
#include "audio_test_logic.h"
#include "joystick.h"

esp_err_t audio_test_start(void);
void audio_test_submit(const joystick_button_t *button);
audio_phase_t audio_test_phase(void);
