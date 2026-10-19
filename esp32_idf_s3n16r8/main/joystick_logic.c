#include "joystick_logic.h"
#include <stdlib.h>

#define AXIS_SWITCH_MARGIN 15
#define DIAGONAL_ENTER_RATIO 65
#define DIAGONAL_HOLD_RATIO 45

bool joystick_center_valid(int center) {
    return center >= 256 && center <= 3839;
}

int joystick_percent(int raw, int center, bool inverted) {
    int delta = raw - center;
    int span = delta < 0 ? center : 4095 - center;
    int percent = delta * 100 / span;
    if (percent < -100) percent = -100;
    if (percent > 100) percent = 100;
    return inverted ? -percent : percent;
}

int joystick_direction(int percent, int previous) {
    if (percent <= -25) return -1;
    if (percent >= 25) return 1;
    if (previous < 0 && percent < -15) return -1;
    if (previous > 0 && percent > 15) return 1;
    return 0;
}

joystick_directions_t joystick_screen_directions(int percent_x, int percent_y,
                                                joystick_directions_t previous,
                                                bool allow_diagonal) {
    /* Physical Y navigates horizontally; physical X is inverted vertically. */
    int horizontal = percent_y;
    int vertical = -percent_x;
    joystick_directions_t result = {
        .x = joystick_direction(horizontal, previous.x),
        .y = joystick_direction(vertical, previous.y),
    };
    if (!result.x || !result.y) return result;
    int x = abs(horizontal), y = abs(vertical);
    if (allow_diagonal) {
        bool holding = result.x == previous.x && result.y == previous.y;
        int ratio = holding ? DIAGONAL_HOLD_RATIO : DIAGONAL_ENTER_RATIO;
        int major = x > y ? x : y;
        int minor = x > y ? y : x;
        if (minor * 100 >= major * ratio) return result;
    }
    if (previous.x == result.x && !previous.y && y < x + AXIS_SWITCH_MARGIN) {
        result.y = 0;
    } else if (previous.y == result.y && !previous.x && x < y + AXIS_SWITCH_MARGIN) {
        result.x = 0;
    } else if (x > y) {
        result.y = 0;
    } else {
        result.x = 0;
    }
    return result;
}

void joystick_button_update(joystick_button_t *button, bool pressed, int64_t now_ms) {
    if (pressed != button->candidate) {
        button->candidate = pressed;
        button->changed_ms = now_ms;
    }
    if (now_ms - button->changed_ms >= 30 && button->pressed != pressed) {
        button->pressed = pressed;
        if (pressed) ++button->presses;
    }
}
