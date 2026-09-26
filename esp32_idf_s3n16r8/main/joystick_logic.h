#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool candidate;
    bool pressed;
    int64_t changed_ms;
    uint32_t presses;
} joystick_button_t;

bool joystick_center_valid(int center);
int joystick_percent(int raw, int center, bool inverted);
int joystick_direction(int percent, int previous);
void joystick_button_update(joystick_button_t *button, bool pressed, int64_t now_ms);
