#include "joystick_logic.h"

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
