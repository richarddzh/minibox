#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "joystick_logic.h"

static void test_normalization(void) {
    assert(!joystick_center_valid(0));
    assert(!joystick_center_valid(255));
    assert(joystick_center_valid(256));
    assert(joystick_center_valid(3839));
    assert(!joystick_center_valid(3840));
    assert(!joystick_center_valid(4095));
    const int centers[] = {256, 1700, 2048, 2300, 3839};
    for (unsigned i = 0; i < sizeof(centers) / sizeof(centers[0]); ++i) {
        int center = centers[i];
        assert(joystick_percent(0, center, false) == -100);
        assert(joystick_percent(center, center, false) == 0);
        assert(joystick_percent(4095, center, false) == 100);
        assert(joystick_percent(0, center, true) == 100);
        assert(joystick_percent(4095, center, true) == -100);
        int last = -100;
        for (int raw = 0; raw <= 4095; ++raw) {
            int value = joystick_percent(raw, center, false);
            assert(value >= -100 && value <= 100 && value >= last);
            assert(joystick_percent(raw, center, true) == -value);
            last = value;
        }
    }
}

static void test_hysteresis(void) {
    assert(joystick_direction(-24, 0) == 0);
    assert(joystick_direction(-25, 0) == -1);
    assert(joystick_direction(-16, -1) == -1);
    assert(joystick_direction(-15, -1) == 0);
    assert(joystick_direction(24, 0) == 0);
    assert(joystick_direction(25, 0) == 1);
    assert(joystick_direction(16, 1) == 1);
    assert(joystick_direction(15, 1) == 0);
    assert(joystick_direction(-100, 1) == -1);
    assert(joystick_direction(100, -1) == 1);
    assert(joystick_direction(0, 1) == 0);
    assert(joystick_direction(0, -1) == 0);
}

static void test_debounce(void) {
    joystick_button_t button = {0};
    joystick_button_update(&button, true, 10);
    joystick_button_update(&button, false, 20);
    joystick_button_update(&button, true, 25);
    joystick_button_update(&button, true, 54);
    assert(!button.pressed && button.presses == 0);
    joystick_button_update(&button, true, 55);
    assert(button.pressed && button.presses == 1);
    joystick_button_update(&button, true, 1000);
    assert(button.presses == 1);
    joystick_button_update(&button, false, 1010);
    joystick_button_update(&button, false, 1039);
    assert(button.pressed);
    joystick_button_update(&button, false, 1040);
    assert(!button.pressed && button.presses == 1);
    joystick_button_update(&button, true, 1050);
    joystick_button_update(&button, true, 1080);
    assert(button.pressed && button.presses == 2);

    joystick_button_t held_at_boot = {.candidate = true, .pressed = true};
    joystick_button_update(&held_at_boot, true, 100);
    assert(held_at_boot.presses == 0);
}

static void expect_direction(int px, int py, joystick_directions_t previous,
                             int x, int y) {
    joystick_directions_t actual = joystick_screen_directions(px, py, previous, false);
    assert(actual.x == x && actual.y == y);
}

static void test_screen_directions(void) {
    joystick_directions_t center = {0}, right = {1, 0};
    expect_direction(100, 0, center, 0, -1);
    expect_direction(-100, 0, center, 0, 1);
    expect_direction(0, 100, center, 1, 0);
    expect_direction(0, -100, center, -1, 0);
    expect_direction(0, 24, center, 0, 0);
    expect_direction(0, 25, center, 1, 0);
    expect_direction(0, 16, right, 1, 0);
    expect_direction(0, 15, right, 0, 0);
    expect_direction(10, 25, center, 0, 0); /* Difference exactly 15. */
    expect_direction(10, 26, center, 1, 0);
    expect_direction(40, 60, center, 0, 0); /* Ratio exactly 1.5. */
    expect_direction(40, 61, center, 1, 0);
    expect_direction(65, 100, center, 1, 0);
    expect_direction(67, 100, right, 0, 0);
    expect_direction(100, 100, right, 0, 0);
    expect_direction(0, -100, right, -1, 0);

    /* Entering isolation releases immediately; it does not latch the old axis. */
    joystick_directions_t state = joystick_screen_directions(0, 100, center, false);
    state = joystick_screen_directions(80, 100, state, false);
    assert(state.x == 0 && state.y == 0);
    state = joystick_screen_directions(0, 24, state, false);
    assert(state.x == 0 && state.y == 0);
    state = joystick_screen_directions(100, 60, state, false);
    assert(state.x == 0 && state.y == -1);
    for (int i = 0; i < 20; ++i) {
        state = joystick_screen_directions(80 + i % 2, 81 - i % 2, state, false);
        assert(state.x == 0 && state.y == 0);
    }

    const joystick_directions_t histories[] = {{0, 0}, {1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    for (unsigned h = 0; h < sizeof(histories) / sizeof(histories[0]); ++h) {
        for (int px = -100; px <= 100; ++px) {
            for (int py = -100; py <= 100; ++py) {
                joystick_directions_t result = joystick_screen_directions(px, py, histories[h], false);
                assert(!(result.x && result.y));
                if (result.x) assert((result.x < 0) == (py < 0));
                if (result.y) assert((result.y < 0) == (px > 0));
                int major = abs(px) > abs(py) ? abs(px) : abs(py);
                int minor = abs(px) > abs(py) ? abs(py) : abs(px);
                bool eligible = major - minor > 15 &&
                                (minor == 0 ? major > 0 : (double)major / minor > 1.5);
                int expected_x = 0, expected_y = 0;
                if (eligible) {
                    if (abs(py) > abs(px))
                        expected_x = joystick_direction(py, histories[h].x);
                    else
                        expected_y = joystick_direction(-px, histories[h].y);
                }
                assert(result.x == expected_x && result.y == expected_y);
            }
        }
    }
}

static void test_diagonal_center_exclusion(void) {
    const joystick_directions_t center = {0}, up_right = {1, -1};
    joystick_directions_t state = joystick_screen_directions(39, 39, up_right, true);
    assert(state.x == 0 && state.y == 0);
    state = joystick_screen_directions(40, 40, center, true);
    assert(state.x == 1 && state.y == -1);
    state = joystick_screen_directions(85, 100, center, true);
    assert(state.x == 1 && state.y == -1);
    state = joystick_screen_directions(79, 100, center, true);
    assert(state.x == 1 && state.y == 0);
    state = joystick_screen_directions(84, 100, up_right, true);
    assert(state.x == 0 && state.y == 0);
    state = joystick_screen_directions(80, 100, center, true);
    assert(state.x == 0 && state.y == 0);
    state = joystick_screen_directions(15, 25, center, true);
    assert(state.x == 0 && state.y == 0);
    state = joystick_screen_directions(14, 25, center, true);
    assert(state.x == 1 && state.y == 0);
    state = joystick_screen_directions(39, 60, up_right, true);
    assert(state.x == 1 && state.y == 0);
    state = joystick_screen_directions(-45, 100, up_right, true);
    assert(state.x == 1 && state.y == 0);

    for (int old_x = -1; old_x <= 1; ++old_x) {
        for (int old_y = -1; old_y <= 1; ++old_y) {
            joystick_directions_t previous = {old_x, old_y};
            for (int px = -100; px <= 100; ++px) {
                for (int py = -100; py <= 100; ++py) {
                    joystick_directions_t actual =
                        joystick_screen_directions(px, py, previous, true);
                    joystick_directions_t expected = {0};
                    int major = abs(px) > abs(py) ? abs(px) : abs(py);
                    int minor = abs(px) > abs(py) ? abs(py) : abs(px);
                    int dx = py < 0 ? -1 : 1, dy = px > 0 ? -1 : 1;
                    if (minor >= 40 && (double)minor / major >= 0.85) {
                        expected.x = dx;
                        expected.y = dy;
                    } else if (major - minor > 10 &&
                               (minor == 0 ? major > 0 : (double)major / minor > 1.25)) {
                        if (abs(py) > abs(px))
                            expected.x = joystick_direction(py, previous.x);
                        else
                            expected.y = joystick_direction(-px, previous.y);
                    }
                    assert(actual.x == expected.x && actual.y == expected.y);
                    if (actual.x && actual.y) assert(abs(px) >= 40 && abs(py) >= 40);
                }
            }
        }
    }
}

int main(void) {
    test_normalization();
    test_hysteresis();
    test_debounce();
    test_screen_directions();
    test_diagonal_center_exclusion();
    puts("Joystick four-way isolation and eight-way center exclusion: all thresholds and histories passed.");
    return 0;
}
