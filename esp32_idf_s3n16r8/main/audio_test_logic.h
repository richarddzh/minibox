#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "joystick_logic.h"

typedef enum {
    AUDIO_IDLE,
    AUDIO_HOLD,
    AUDIO_RECORDING,
    AUDIO_WAIT_RELEASE,
    AUDIO_TONE_WAIT_RELEASE,
    AUDIO_PLAYING,
    AUDIO_WAIT_RESET,
    AUDIO_ERROR,
} audio_phase_t;

typedef enum {
    AUDIO_NO_ACTION,
    AUDIO_START_RECORD,
    AUDIO_STOP_RECORD,
    AUDIO_STOP_AND_PLAY,
    AUDIO_START_PLAY,
} audio_action_t;

typedef struct {
    audio_phase_t phase;
    uint32_t seen_presses;
    int64_t pressed_at_ms;
} audio_test_logic_t;

audio_action_t audio_test_logic_step(audio_test_logic_t *logic,
                                     const joystick_button_t *button,
                                     int64_t now_ms, bool buffer_full);
