#include "app_chat_alarm.h"

#include "app_config.h"
#include "ui_cat_avatar.h"
#include "ui_screen.h"
#include "voice_chat.h"

#include <freertos/FreeRTOS.h>
#include <string.h>

#define CHAT_HISTORY_CAPACITY 10
#define CHAT_TEXT_BYTES 192
#define CHAT_VISIBLE_MESSAGES 3

typedef struct {
    bool from_user;
    char text[CHAT_TEXT_BYTES];
} chat_message_t;

static chat_message_t s_history[CHAT_HISTORY_CAPACITY];
static size_t s_history_count;
static uint32_t s_history_generation;
static uint32_t s_seen_generation;
static size_t s_history_scroll;
static portMUX_TYPE s_history_lock = portMUX_INITIALIZER_UNLOCKED;
static bool s_recording;
static int64_t s_recording_started_ms;

static void copy_utf8(char *dst, size_t capacity, const char *src) {
    if (!src || capacity == 0) return;
    size_t length = strlen(src);
    if (length >= capacity) {
        length = capacity - 1;
        while (length > 0 && ((uint8_t)src[length] & 0xC0) == 0x80) {
            --length;
        }
    }
    memcpy(dst, src, length);
    dst[length] = 0;
}

static void append_message_locked(bool from_user, const char *text) {
    if (!text || !text[0]) return;
    if (s_history_count == CHAT_HISTORY_CAPACITY) {
        memmove(&s_history[0], &s_history[1],
                sizeof(s_history[0]) * (CHAT_HISTORY_CAPACITY - 1));
        --s_history_count;
    }
    chat_message_t *message = &s_history[s_history_count++];
    message->from_user = from_user;
    copy_utf8(message->text, sizeof(message->text), text);
    ++s_history_generation;
}

void app_chat_alarm_append_user(const char *text) {
    taskENTER_CRITICAL(&s_history_lock);
    append_message_locked(true, text);
    taskEXIT_CRITICAL(&s_history_lock);
}

void app_chat_alarm_append_assistant(const char *text) {
    taskENTER_CRITICAL(&s_history_lock);
    append_message_locked(false, text);
    taskEXIT_CRITICAL(&s_history_lock);
}

static void turn_received(const char *user_text,
                          const char *assistant_text,
                          void *ctx) {
    (void)ctx;
    app_chat_alarm_append_user(user_text);
    app_chat_alarm_append_assistant(assistant_text);
}

static void enter(void) {
    voice_chat_set_turn_cb(turn_received, NULL);
    s_history_scroll = 0;
}

static void exit_app(void) {
}

static gui_action_t tick(int64_t now_ms) {
    taskENTER_CRITICAL(&s_history_lock);
    uint32_t generation = s_history_generation;
    taskEXIT_CRITICAL(&s_history_lock);
    bool updated = generation != s_seen_generation;
    if (updated) {
        s_seen_generation = generation;
        s_history_scroll = 0;
    }
    if (!s_recording) return updated ? GUI_ACTION_REDRAW : GUI_ACTION_NONE;
    if (voice_chat_pump() != ESP_OK) {
        s_recording = false;
        return updated ? GUI_ACTION_REDRAW : GUI_ACTION_NONE;
    }
    if ((uint32_t)(now_ms - s_recording_started_ms) >=
        APP_VOICE_MAX_RECORD_MS) {
        return GUI_ACTION_STOP_VOICE;
    }
    return GUI_ACTION_NONE;
}

void app_chat_alarm_set_recording(bool recording, int64_t started_ms) {
    s_recording = recording;
    if (recording) s_recording_started_ms = started_ms;
}

bool app_chat_alarm_is_recording(void) {
    return s_recording;
}

static void centered_text(int y, const char *text,
                          uint8_t size, ui_color_t color) {
    ui_draw_text((APP_UI_W - ui_text_width(text, size)) / 2,
                 y, text, size, color);
}

static void render_setup(void) {
    centered_text(18, "无线配网", 3, UI_COLOR_WARNING);
    ui_fill_rect(28, 58, 264, 126, UI_COLOR_PANEL);
    ui_draw_text(44, 76, APP_PROVISION_AP_SSID, 2, UI_COLOR_TEXT);
    ui_draw_text(44, 108, APP_PROVISION_AP_PASSWORD, 2, UI_COLOR_TEXT);
    ui_draw_text(44, 140, "地址: 192.168.4.1", 2, UI_COLOR_CYAN);
    centered_text(174, "请在设置中关闭", 2, UI_COLOR_MUTED);
}

static void draw_bubble(int y, const chat_message_t *message) {
    const int width = 238;
    const int height = 40;
    const int avatar_x = message->from_user ? 284 : 6;
    int x = message->from_user ? 38 : 44;
    ui_color_t background =
        message->from_user ? UI_COLOR_PRIMARY_DARK : UI_COLOR_PANEL;
    ui_color_t border =
        message->from_user ? UI_COLOR_PRIMARY : UI_COLOR_PANEL_ALT;

    ui_draw_cat_avatar(avatar_x, y + 4, message->from_user);
    ui_fill_round_rect(x, y, width, height, 8, background);
    if (message->from_user) {
        ui_fill_triangle(x + width - 2, y + 21, x + width + 9, y + 27,
                         x + width - 2, y + 32, background);
    } else {
        ui_fill_triangle(x + 2, y + 21, x - 9, y + 27,
                         x + 2, y + 32, background);
    }
    ui_draw_line(x + 8, y, x + width - 9, y, border);

    ui_draw_text_box(x + 8, y + 4, width - 16, height - 8,
                      message->text, 1, UI_COLOR_TEXT);
}

static void render_chat(const gui_model_t *model, int focused_item) {
    ui_fill_rect(0, 0, APP_UI_W, 32, UI_COLOR_NAVY);
    ui_draw_text(12, 8, "闲聊闹钟", 2, UI_COLOR_TEXT);
    if (model->voice_visible) {
        int status_x = APP_UI_W - ui_text_width(model->voice_title, 2) - 10;
        ui_draw_text(status_x, 8, model->voice_title, 2, UI_COLOR_CYAN);
    }

    chat_message_t snapshot[CHAT_HISTORY_CAPACITY];
    size_t count;
    taskENTER_CRITICAL(&s_history_lock);
    count = s_history_count;
    memcpy(snapshot, s_history, sizeof(snapshot));
    taskEXIT_CRITICAL(&s_history_lock);

    if (count == 0) {
        chat_message_t greeting = {
            .from_user = false,
            .text = "你好！A 短按闲聊，B 按住说话，松开发送。",
        };
        draw_bubble(45, &greeting);
    } else {
        size_t visible = count < CHAT_VISIBLE_MESSAGES
                             ? count : CHAT_VISIBLE_MESSAGES;
        size_t max_scroll = count - visible;
        if (s_history_scroll > max_scroll) s_history_scroll = max_scroll;
        size_t first = count - visible - s_history_scroll;
        for (size_t i = 0; i < visible; ++i) {
            draw_bubble(36 + (int)i * 45, &snapshot[first + i]);
        }
    }

    ui_fill_rect(44, 174, 232, 28,
                 focused_item == 0 ? UI_COLOR_PRIMARY_DARK : UI_COLOR_PANEL_ALT);
    ui_draw_rect(44, 174, 232, 28, focused_item == 0 ? 3 : 1,
                 focused_item == 0 ? UI_COLOR_FOCUS : UI_COLOR_MUTED);
    centered_text(180, s_recording ? "松开发送 · X 结束" : "B 按住说话 · 上下翻记录", 1,
                  focused_item == 0 ? UI_COLOR_FOCUS : UI_COLOR_TEXT);
}

static void render(const gui_model_t *model, int focused_item) {
    if (model->setup_mode) {
        render_setup();
    } else {
        render_chat(model, focused_item);
    }
}

static gui_action_t activate(uint8_t item) {
    return item == 0 ? GUI_ACTION_CHIME : GUI_ACTION_NONE;
}

static bool handle_input(gui_input_t input, gui_action_t *action) {
    if (input == GUI_INPUT_BACK && s_recording) {
        *action = GUI_ACTION_STOP_VOICE;
        return true;
    }
    if (input == GUI_INPUT_LONG_PRESS) {
        *action = GUI_ACTION_START_VOICE;
        return true;
    }
    if (input == GUI_INPUT_RELEASE && s_recording) {
        *action = GUI_ACTION_STOP_VOICE;
        return true;
    }
    if (s_recording) {
        *action = GUI_ACTION_NONE;
        return true;
    }
    if (input == GUI_INPUT_UP || input == GUI_INPUT_DOWN) {
        taskENTER_CRITICAL(&s_history_lock);
        size_t count = s_history_count;
        taskEXIT_CRITICAL(&s_history_lock);
        size_t max_scroll = count > CHAT_VISIBLE_MESSAGES
                                ? count - CHAT_VISIBLE_MESSAGES : 0;
        if (input == GUI_INPUT_UP && s_history_scroll < max_scroll) ++s_history_scroll;
        if (input == GUI_INPUT_DOWN && s_history_scroll) --s_history_scroll;
        *action = GUI_ACTION_REDRAW;
        return true;
    }
    return false;
}

static const gui_focus_node_t s_focus_grid[] = {
    {
        .left = GUI_FOCUS_HOME,
        .right = GUI_FOCUS_HOME,
        .up = GUI_FOCUS_HOME,
        .down = GUI_FOCUS_HOME,
    },
};

static const gui_app_t s_app = {
    .id = "chat",
    .label = "闲聊闹钟",
    .icon = GUI_ICON_CHAT,
    .focus_count = 1,
    .focus_grid = s_focus_grid,
    .enter = enter,
    .exit = exit_app,
    .tick = tick,
    .render = render,
    .activate = activate,
    .handle_input = handle_input,
};

const gui_app_t *app_chat_alarm_descriptor(void) {
    return &s_app;
}
