#include <assert.h>
#include <stdio.h>
#include "audio_test_logic.h"

static void test_release_before_limit(void) {
    audio_test_logic_t logic = {0};
    joystick_button_t button = {0};
    assert(audio_test_logic_step(&logic, &button, false) == AUDIO_NO_ACTION);
    button.pressed = true;
    button.presses = 1;
    assert(audio_test_logic_step(&logic, &button, false) == AUDIO_START_RECORD);
    assert(logic.phase == AUDIO_RECORDING);
    assert(audio_test_logic_step(&logic, &button, false) == AUDIO_NO_ACTION);
    button.pressed = false;
    assert(audio_test_logic_step(&logic, &button, false) == AUDIO_STOP_AND_PLAY);
    assert(logic.phase == AUDIO_PLAYING);
    logic.phase = AUDIO_WAIT_RESET;
    assert(audio_test_logic_step(&logic, &button, false) == AUDIO_NO_ACTION);
    assert(logic.phase == AUDIO_IDLE);
    button.pressed = true;
    button.presses = 2;
    assert(audio_test_logic_step(&logic, &button, false) == AUDIO_START_RECORD);
}

static void test_limit_and_release(void) {
    audio_test_logic_t logic = {0};
    joystick_button_t button = {.pressed = true, .presses = 1};
    assert(audio_test_logic_step(&logic, &button, false) == AUDIO_START_RECORD);
    assert(audio_test_logic_step(&logic, &button, true) == AUDIO_STOP_RECORD);
    assert(logic.phase == AUDIO_WAIT_RELEASE);
    assert(audio_test_logic_step(&logic, &button, true) == AUDIO_NO_ACTION);
    button.pressed = false;
    assert(audio_test_logic_step(&logic, &button, true) == AUDIO_START_PLAY);
}

static void test_no_retrigger_during_playback(void) {
    audio_test_logic_t logic = {0};
    joystick_button_t button = {.pressed = true, .presses = 1};
    assert(audio_test_logic_step(&logic, &button, false) == AUDIO_START_RECORD);
    button.pressed = false;
    assert(audio_test_logic_step(&logic, &button, false) == AUDIO_STOP_AND_PLAY);
    button.pressed = true;
    button.presses = 2;
    assert(audio_test_logic_step(&logic, &button, false) == AUDIO_NO_ACTION);
    logic.phase = AUDIO_WAIT_RESET;
    assert(audio_test_logic_step(&logic, &button, false) == AUDIO_NO_ACTION);
    button.pressed = false;
    assert(audio_test_logic_step(&logic, &button, false) == AUDIO_NO_ACTION);
    assert(logic.phase == AUDIO_IDLE);
    button.pressed = true;
    button.presses = 3;
    assert(audio_test_logic_step(&logic, &button, false) == AUDIO_START_RECORD);
}

int main(void) {
    test_release_before_limit();
    test_limit_and_release();
    test_no_retrigger_during_playback();
    puts("Audio recording button transitions passed.");
    return 0;
}
