#undef NDEBUG
#include <assert.h>
#include <stdio.h>
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
                             bool diagonal, int x, int y) {
    joystick_directions_t actual = joystick_screen_directions(px, py, previous, diagonal);
    assert(actual.x == x && actual.y == y);
}

static void test_screen_directions(void) {
    joystick_directions_t center = {0}, right = {1, 0}, up = {0, -1}, up_right = {1, -1};
    expect_direction(100, 0, center, false, 0, -1);
    expect_direction(-100, 0, center, false, 0, 1);
    expect_direction(0, 100, center, false, 1, 0);
    expect_direction(0, -100, center, false, -1, 0);
    expect_direction(24, 24, center, true, 0, 0);
    expect_direction(15, 15, up_right, true, 0, 0);
    expect_direction(16, 16, up_right, true, 1, -1);
    expect_direction(35, 90, center, false, 1, 0);
    expect_direction(35, 90, center, true, 1, 0);
    expect_direction(90, 35, center, true, 0, -1);
    expect_direction(75, 65, right, false, 1, 0);
    expect_direction(80, 65, right, false, 0, -1);
    expect_direction(65, 75, up, false, 0, -1);
    expect_direction(65, 80, up, false, 1, 0);
    expect_direction(65, 100, right, true, 1, -1);
    expect_direction(64, 100, right, true, 1, 0);
    expect_direction(45, 100, up_right, true, 1, -1);
    expect_direction(44, 100, up_right, true, 1, 0);
    expect_direction(-100, -100, up_right, true, -1, 1);
    expect_direction(0, -100, right, false, -1, 0);
    expect_direction(0, 0, right, false, 0, 0);

    for (int px = -100; px <= 100; ++px) {
        for (int py = -100; py <= 100; ++py) {
            joystick_directions_t result = joystick_screen_directions(px, py, center, false);
            assert(!(result.x && result.y));
            if (result.x) assert((result.x < 0) == (py < 0));
            if (result.y) assert((result.y < 0) == (px > 0));
            result = joystick_screen_directions(px, py, center, true);
            if (result.x) assert((result.x < 0) == (py < 0));
            if (result.y) assert((result.y < 0) == (px > 0));
        }
    }
}

int main(void) {
    test_normalization();
    test_hysteresis();
    test_debounce();
    test_screen_directions();
    puts("Joystick normalization, debounce, screen mapping, snapping and hysteresis tests passed.");
    return 0;
}
