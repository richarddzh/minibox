#include "buttons.h"
#include "app_config.h"
#include "joystick.h"
#include <stdatomic.h>

#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

static const char *TAG = "buttons";
static QueueHandle_t s_events;
static joystick_state_t s_stick;
static volatile bool s_main_pressed;
static atomic_uchar s_gamepad;

static void emit(app_input_event_t event) {
    if (xQueueSend(s_events, &event, 0) != pdTRUE) {
        ESP_LOGE(TAG, "Input queue full: %s", buttons_event_name(event));
    }
}

static void input_task(void *arg) {
    (void)arg;
    const int pins[] = {BUTTON_1_PIN, RECORD_BUTTON_PIN, BUTTON_3_PIN, BUTTON_4_PIN};
    joystick_button_t buttons[BUTTON_COUNT] = {0};
    joystick_directions_t navigation = {0};
    joystick_directions_t gameplay = {0};
    int previous_x = 0, previous_y = 0;
    int64_t repeat_x = 0, repeat_y = 0;
    TickType_t wake = xTaskGetTickCount();
    for (;;) {
        int64_t now = esp_timer_get_time() / 1000;
        for (unsigned i = 0; i < BUTTON_COUNT; ++i) {
            bool before = buttons[i].pressed;
            bool reading = gpio_get_level(pins[i]) ==
#ifdef CONFIG_MINIBOX_BUTTON_ACTIVE_LOW
                           0;
#else
                           1;
#endif
            joystick_button_update(&buttons[i], reading, now);
            if (before == buttons[i].pressed) continue;
            switch (i) {
            case 0:
                s_main_pressed = buttons[i].pressed;
                emit(buttons[i].pressed ? APP_INPUT_MAIN_PRESSED : APP_INPUT_MAIN_RELEASED);
                break;
            case 1:
                emit(buttons[i].pressed ? APP_INPUT_RECORD_PRESSED : APP_INPUT_RECORD_RELEASED);
                break;
            case 2:
                if (buttons[i].pressed) emit(APP_INPUT_BACK);
                break;
            case 3:
                if (buttons[i].pressed) emit(APP_INPUT_JOYSTICK_PRESSED);
                break;
            }
        }
        ESP_ERROR_CHECK(joystick_read(&s_stick));
        navigation = joystick_screen_directions(s_stick.percent_x, s_stick.percent_y,
                                                navigation, false);
        gameplay = joystick_screen_directions(s_stick.percent_x, s_stick.percent_y,
                                              gameplay, true);
        int directions[] = {navigation.y, navigation.x};
        uint8_t pad = 0;
        for (unsigned i = 0; i < BUTTON_COUNT; ++i) {
            if (buttons[i].pressed) pad |= 1U << i;
        }
        if (gameplay.y < 0) pad |= BUTTON_PAD_UP;
        if (gameplay.y > 0) pad |= BUTTON_PAD_DOWN;
        if (gameplay.x < 0) pad |= BUTTON_PAD_LEFT;
        if (gameplay.x > 0) pad |= BUTTON_PAD_RIGHT;
        atomic_store(&s_gamepad, pad);
        int *previous[] = {&previous_x, &previous_y};
        int64_t *repeat[] = {&repeat_x, &repeat_y};
        for (unsigned axis = 0; axis < 2; ++axis) {
            int direction = directions[axis];
            bool changed = direction != *previous[axis];
            if (direction && (changed || now >= *repeat[axis])) {
                /* Reserve queue capacity for key releases during network waits. */
                if (uxQueueMessagesWaiting(s_events) < 16) {
                    emit(axis == 1 ? (direction < 0 ? APP_INPUT_LEFT : APP_INPUT_RIGHT)
                                   : (direction < 0 ? APP_INPUT_UP : APP_INPUT_DOWN));
                }
                *repeat[axis] = now + (changed ? 350 : 140);
            }
            *previous[axis] = direction;
        }
        xTaskDelayUntil(&wake, pdMS_TO_TICKS(10));
    }
}

void buttons_init(void) {
    const gpio_config_t keys = {
        .pin_bit_mask = (1ULL << BUTTON_1_PIN) | (1ULL << RECORD_BUTTON_PIN) |
                        (1ULL << BUTTON_3_PIN) | (1ULL << BUTTON_4_PIN),
        .mode = GPIO_MODE_INPUT,
#ifdef CONFIG_MINIBOX_BUTTON_ACTIVE_LOW
        .pull_up_en = GPIO_PULLUP_ENABLE,
#else
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
#endif
    };
    ESP_ERROR_CHECK(gpio_config(&keys));
    ESP_ERROR_CHECK(joystick_init(&s_stick));
    s_events = xQueueCreate(64, sizeof(app_input_event_t));
    ESP_ERROR_CHECK(s_events ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(xTaskCreate(input_task, "minibox_input", 4096, NULL, 5, NULL)
                    == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
}

bool buttons_poll(app_input_event_t *event) {
    return event && xQueueReceive(s_events, event, 0) == pdTRUE;
}

bool buttons_pressed(void) {
    return s_main_pressed;
}

uint8_t buttons_gamepad(void) {
    return atomic_load(&s_gamepad);
}

const char *buttons_event_name(app_input_event_t event) {
    static const char *const names[] = {
        "KEY1 pressed", "KEY1 released", "KEY4 activate",
        "Joystick left", "Joystick right", "Joystick up", "Joystick down",
        "KEY2 record", "KEY2 released", "KEY3 back",
    };
    return event >= APP_INPUT_MAIN_PRESSED && event <= APP_INPUT_BACK ? names[event] : "?";
}
