#include "audio_test_logic.h"

audio_action_t audio_test_logic_step(audio_test_logic_t *logic,
                                     const joystick_button_t *button, bool buffer_full) {
    switch (logic->phase) {
    case AUDIO_IDLE:
        if (button->pressed && button->presses != logic->seen_presses) {
            logic->seen_presses = button->presses;
            logic->phase = AUDIO_RECORDING;
            return AUDIO_START_RECORD;
        } else if (!button->pressed) {
            logic->seen_presses = button->presses;
        }
        break;
    case AUDIO_RECORDING:
        if (!button->pressed) {
            logic->phase = AUDIO_PLAYING;
            return AUDIO_STOP_AND_PLAY;
        }
        if (buffer_full) {
            logic->phase = AUDIO_WAIT_RELEASE;
            return AUDIO_STOP_RECORD;
        }
        break;
    case AUDIO_WAIT_RELEASE:
        if (!button->pressed) {
            logic->phase = AUDIO_PLAYING;
            return AUDIO_START_PLAY;
        }
        break;
    case AUDIO_WAIT_RESET:
        if (!button->pressed) {
            logic->seen_presses = button->presses;
            logic->phase = AUDIO_IDLE;
        }
        break;
    case AUDIO_PLAYING:
    case AUDIO_HOLD:
    case AUDIO_TONE_WAIT_RELEASE:
    case AUDIO_ERROR:
        break;
    }
    return AUDIO_NO_ACTION;
}
