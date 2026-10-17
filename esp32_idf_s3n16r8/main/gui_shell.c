#include "gui_shell.h"

#include "app_config.h"
#include "launcher_icons_data.h"
#include "ui_screen.h"

#include <string.h>

#define STATUS_Y 208
#define STATUS_H 32
#define CONTENT_BOTTOM 207
#define MAX_GUI_APPS 8

static const gui_app_t *s_apps[MAX_GUI_APPS];
static size_t s_app_count;
static size_t s_current_app;
static int s_focus = GUI_FOCUS_HOME;
static bool s_launcher;

static void draw_centered_text(int center_x, int y, const char *text,
                               uint8_t size, ui_color_t color) {
    ui_draw_text(center_x - ui_text_width(text, size) / 2,
                 y, text, size, color);
}

static void draw_home_icon(int x, int y, ui_color_t color) {
    ui_draw_line(x, y + 8, x + 10, y, color);
    ui_draw_line(x + 10, y, x + 20, y + 8, color);
    ui_draw_rect(x + 3, y + 8, 14, 11, 2, color);
    ui_fill_rect(x + 9, y + 13, 4, 6, color);
}

static void draw_wifi_icon(int x, int y, bool connected) {
    ui_color_t color = connected ? UI_COLOR_SUCCESS : UI_COLOR_DANGER;
    for (int bar = 0; bar < 3; ++bar) {
        int height = 5 + bar * 5;
        ui_fill_round_rect(x + bar * 6, y + 20 - height,
                            4, height, 1, color);
    }
}

static void draw_status_bar(const gui_model_t *model) {
    ui_fill_rect(0, STATUS_Y, APP_UI_W, STATUS_H, UI_COLOR_NAVY);
    ui_fill_rect(0, STATUS_Y, APP_UI_W, 2, UI_COLOR_PRIMARY);

    if (s_focus == GUI_FOCUS_HOME) {
        ui_draw_rect(4, STATUS_Y + 4, 76, 24, 2, UI_COLOR_FOCUS);
    }
    draw_home_icon(10, STATUS_Y + 6,
                   s_focus == GUI_FOCUS_HOME ? UI_COLOR_FOCUS : UI_COLOR_TEXT);
    ui_draw_text(36, STATUS_Y + 6, "首页", 2,
                 s_focus == GUI_FOCUS_HOME ? UI_COLOR_FOCUS : UI_COLOR_TEXT);

    const char *time = model->time_valid ? model->time : "--:--";
    char hhmm[6] = "--:--";
    if (time && strlen(time) >= 5) memcpy(hhmm, time, 5);
    ui_draw_text(224, STATUS_Y + 7, hhmm, 2, UI_COLOR_TEXT);
    ui_draw_text(89, STATUS_Y + 9, "GPIO6 返回/首页", 1, UI_COLOR_MUTED);
    draw_wifi_icon(294, STATUS_Y + 5,
                   model->wifi_connected || model->setup_mode);
}

static void render_launcher(void) {
    ui_fill_rect(0, 0, APP_UI_W, 32, UI_COLOR_NAVY);
    ui_draw_text(12, 8, "应用", 2, UI_COLOR_TEXT);
    ui_draw_text(12, 34, "摇杆选择 · GPIO4 确认", 1, UI_COLOR_MUTED);

    const int card_w = 94;
    const int card_h = 70;
    const int gap = 9;
    for (size_t i = 0; i < s_app_count; ++i) {
        int column = (int)(i % 3);
        int row = (int)(i / 3);
        int x = 9 + column * (card_w + gap);
        int y = 50 + row * (card_h + 3);
        if (y + card_h > CONTENT_BOTTOM) break;
        bool selected = s_focus == (int)i;
        ui_fill_round_rect(x, y, card_w, card_h, 9,
                     selected ? UI_COLOR_PRIMARY_DARK : UI_COLOR_PANEL);
        ui_draw_rect(x, y, card_w, card_h, selected ? 4 : 2,
                     selected ? UI_COLOR_FOCUS : UI_COLOR_PANEL_ALT);
        draw_centered_text(x + card_w / 2, y + 53, s_apps[i]->label,
                           1, selected ? UI_COLOR_FOCUS : UI_COLOR_TEXT);
    }
}

static esp_err_t draw_launcher_icons(void) {
    const int card_w = 94;
    const int card_h = 70;
    const int gap = 9;
    for (size_t i = 0; i < s_app_count; ++i) {
        int column = (int)(i % 3);
        int row = (int)(i / 3);
        int x = 9 + column * (card_w + gap);
        int y = 50 + row * (card_h + 3);
        if (y + card_h > CONTENT_BOTTOM) break;

        gui_icon_t icon = s_apps[i]->icon;
        if ((size_t)icon >= sizeof(s_launcher_icon_rgb565) /
                            sizeof(s_launcher_icon_rgb565[0])) {
            icon = GUI_ICON_CHAT;
        }
        ui_color_t background = s_focus == (int)i
                                    ? UI_COLOR_PRIMARY_DARK
                                    : UI_COLOR_PANEL;
        esp_err_t err = ui_draw_rgb565_bitmap(
            x + (card_w - LAUNCHER_ICON_W) / 2, y + 4,
            LAUNCHER_ICON_W, LAUNCHER_ICON_H,
            s_launcher_icon_rgb565[icon],
            s_launcher_icon_alpha[icon],
            background);
        if (err != ESP_OK) return err;
    }
    return ESP_OK;
}

void gui_shell_init(const gui_app_t *const *apps, size_t app_count,
                    size_t default_app) {
    if (app_count > MAX_GUI_APPS) app_count = MAX_GUI_APPS;
    for (size_t i = 0; i < app_count; ++i) s_apps[i] = apps[i];
    s_app_count = app_count;
    s_current_app = default_app < app_count ? default_app : 0;
    s_focus = app_count > 0 && s_apps[s_current_app]->focus_count > 0
                  ? 0 : GUI_FOCUS_HOME;
    s_launcher = false;
    if (s_app_count > 0 && s_apps[s_current_app]->enter) {
        s_apps[s_current_app]->enter();
    }
}

gui_action_t gui_shell_handle_input(gui_input_t input) {
    if (s_app_count == 0) return GUI_ACTION_NONE;
    if (input == GUI_INPUT_BACK) {
        if (!s_launcher && s_apps[s_current_app]->handle_input) {
            gui_action_t action = GUI_ACTION_NONE;
            if (s_apps[s_current_app]->handle_input(input, &action)) return action;
        }
        if (!s_launcher && s_apps[s_current_app]->exit) {
            s_apps[s_current_app]->exit();
        }
        s_launcher = true;
        s_focus = (int)s_current_app;
        return GUI_ACTION_REDRAW;
    }
    if (!s_launcher && s_apps[s_current_app]->handle_input) {
        gui_action_t action = GUI_ACTION_NONE;
        if (s_apps[s_current_app]->handle_input(input, &action)) return action;
    }

    if (input == GUI_INPUT_PREVIOUS) input = GUI_INPUT_LEFT;
    if (input == GUI_INPUT_NEXT) input = GUI_INPUT_RIGHT;

    if (input == GUI_INPUT_LEFT || input == GUI_INPUT_RIGHT ||
        input == GUI_INPUT_UP || input == GUI_INPUT_DOWN) {
        if (s_focus == GUI_FOCUS_HOME) {
            if (s_launcher) {
                s_focus = s_app_count > 0 ? 0 : GUI_FOCUS_HOME;
            } else {
                s_focus = s_apps[s_current_app]->focus_count > 0
                              ? 0 : GUI_FOCUS_HOME;
            }
            return GUI_ACTION_NONE;
        }

        if (s_launcher) {
            int column = s_focus % 3;
            int row = s_focus / 3;
            int target = s_focus;
            if (input == GUI_INPUT_LEFT) target = s_focus - 1;
            if (input == GUI_INPUT_RIGHT) target = s_focus + 1;
            if (input == GUI_INPUT_UP) target = s_focus - 3;
            if (input == GUI_INPUT_DOWN) target = s_focus + 3;
            bool invalid =
                target < 0 || target >= (int)s_app_count ||
                (input == GUI_INPUT_LEFT && target / 3 != row) ||
                (input == GUI_INPUT_RIGHT && target / 3 != row) ||
                (input == GUI_INPUT_UP && column != target % 3) ||
                (input == GUI_INPUT_DOWN && column != target % 3);
            s_focus = invalid ? GUI_FOCUS_HOME : target;
            return GUI_ACTION_NONE;
        }

        const gui_app_t *app = s_apps[s_current_app];
        if (!app->focus_grid || s_focus >= app->focus_count) return GUI_ACTION_NONE;
        const gui_focus_node_t *node = &app->focus_grid[s_focus];
        int target = GUI_FOCUS_NONE;
        if (input == GUI_INPUT_LEFT) target = node->left;
        if (input == GUI_INPUT_RIGHT) target = node->right;
        if (input == GUI_INPUT_UP) target = node->up;
        if (input == GUI_INPUT_DOWN) target = node->down;
        if (target != GUI_FOCUS_NONE) s_focus = target;
        return GUI_ACTION_NONE;
    }
    if (input == GUI_INPUT_AUX_ACTIVATE) input = GUI_INPUT_ACTIVATE;
    if (input != GUI_INPUT_ACTIVATE) return GUI_ACTION_NONE;

    if (s_focus == GUI_FOCUS_HOME) {
        if (!s_launcher && s_apps[s_current_app]->exit) {
            s_apps[s_current_app]->exit();
        }
        s_launcher = true;
        s_focus = s_app_count > 0 ? 0 : GUI_FOCUS_HOME;
        return GUI_ACTION_NONE;
    }
    if (s_launcher) {
        s_current_app = (size_t)s_focus;
        s_launcher = false;
        s_focus = s_apps[s_current_app]->focus_count > 0
                      ? 0 : GUI_FOCUS_HOME;
        if (s_apps[s_current_app]->enter) s_apps[s_current_app]->enter();
        return GUI_ACTION_NONE;
    }
    return s_apps[s_current_app]->activate((uint8_t)s_focus);
}

gui_action_t gui_shell_tick(int64_t now_ms) {
    if (s_launcher || s_app_count == 0 || !s_apps[s_current_app]->tick) {
        return GUI_ACTION_NONE;
    }
    return s_apps[s_current_app]->tick(now_ms);
}

void gui_shell_render(const gui_model_t *model) {
    if (!model) return;
    ui_begin(UI_COLOR_BG);
    if (s_launcher) {
        render_launcher();
    } else {
        if (s_app_count) s_apps[s_current_app]->render(model, s_focus);
    }
    draw_status_bar(model);
    if (s_launcher) ESP_ERROR_CHECK_WITHOUT_ABORT(draw_launcher_icons());
    ESP_ERROR_CHECK_WITHOUT_ABORT(ui_flush());
}

bool gui_shell_is_launcher(void) {
    return s_launcher;
}

bool gui_shell_current_app_is(const char *id) {
    return !s_launcher && id && s_app_count > 0 &&
           strcmp(s_apps[s_current_app]->id, id) == 0;
}
