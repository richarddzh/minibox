#pragma once

#include <stdbool.h>
#include <stdint.h>

#define JOYSTICK_AXIS_DIFFERENCE_THRESHOLD 15
#define JOYSTICK_AXIS_RATIO_PERCENT 150
#define JOYSTICK_GAME_AXIS_DIFFERENCE_THRESHOLD 10
#define JOYSTICK_GAME_AXIS_RATIO_PERCENT 125
#define JOYSTICK_DIAGONAL_MIN_PERCENT 40
#define JOYSTICK_DIAGONAL_RATIO_PERCENT 85

typedef struct {
    bool candidate;
    bool pressed;
    int64_t changed_ms;
    uint32_t presses;
} joystick_button_t;

typedef struct {
    int x;
    int y;
} joystick_directions_t;

bool joystick_center_valid(int center);
int joystick_percent(int raw, int center, bool inverted);
int joystick_direction(int percent, int previous);
joystick_directions_t joystick_screen_directions(int percent_x, int percent_y,
                                                joystick_directions_t previous,
                                                bool allow_diagonal);
void joystick_button_update(joystick_button_t *button, bool pressed, int64_t now_ms);
