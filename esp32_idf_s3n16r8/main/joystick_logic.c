#include "joystick_logic.h"
#include <stdlib.h>

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
    int x = abs(horizontal), y = abs(vertical);
    int major = x > y ? x : y;
    int minor = x > y ? y : x;
    joystick_directions_t result = {0};
    if (allow_diagonal && minor >= JOYSTICK_DIAGONAL_MIN_PERCENT) {
        int dx = horizontal < 0 ? -1 : 1;
        int dy = vertical < 0 ? -1 : 1;
        if (minor * 100 >= major * JOYSTICK_DIAGONAL_RATIO_PERCENT) {
            result.x = dx;
            result.y = dy;
            return result;
        }
    }
    int difference = allow_diagonal ? JOYSTICK_GAME_AXIS_DIFFERENCE_THRESHOLD
                                   : JOYSTICK_AXIS_DIFFERENCE_THRESHOLD;
    int ratio = allow_diagonal ? JOYSTICK_GAME_AXIS_RATIO_PERCENT
                              : JOYSTICK_AXIS_RATIO_PERCENT;
    /* Apply both gates even when the minor axis is below its own deadzone.
     * Multiplication preserves strict ratio boundaries without division by zero. */
    if (major - minor <= difference || major * 100 <= minor * ratio) return result;
    if (x > y) {
        result.x = joystick_direction(horizontal, previous.x);
    } else {
        result.y = joystick_direction(vertical, previous.y);
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
