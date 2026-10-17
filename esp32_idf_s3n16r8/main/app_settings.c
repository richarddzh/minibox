#include "app_settings.h"

#include "app_config.h"
#include "ui_screen.h"
#include "wifi_creds.h"
#include "wifi_time.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define WIFI_VISIBLE_ROWS 4
#define KEY_COUNT 32
#define KEY_ROW_COUNT 4

typedef enum {
    SETTINGS_PAGE_ROOT = 0,
    SETTINGS_PAGE_WIFI,
    SETTINGS_PAGE_API,
    SETTINGS_PAGE_KEYBOARD,
} settings_page_t;

typedef enum {
    KEYBOARD_SSID = 0,
    KEYBOARD_PASSWORD,
} keyboard_purpose_t;

typedef enum {
    KEY_MODE_LOWER = 0,
    KEY_MODE_UPPER,
    KEY_MODE_SYMBOL,
    KEY_MODE_SYMBOL_EXTRA,
    KEY_MODE_SYMBOL_MORE,
} keyboard_mode_t;

static settings_page_t s_page;
static wifi_creds_t s_profiles[WIFI_CREDS_MAX_PROFILES];
static size_t s_profile_count;
static wifi_scan_ap_t s_scan[WIFI_SCAN_MAX_RESULTS];
static size_t s_scan_count;
static int s_wifi_focus;
static int s_wifi_scroll;
static char s_wifi_status[48] = "选择已保存网络或扫描";

static wifi_creds_t s_draft_wifi;
static wifi_creds_t s_pending_wifi;
static bool s_has_pending_wifi;

static keyboard_purpose_t s_keyboard_purpose;
static keyboard_mode_t s_keyboard_mode;
static char s_edit[65];
static size_t s_edit_capacity;
static size_t s_edit_cursor;
static int s_key_focus;
static char s_keyboard_error[48];
static bool s_draft_secured;

static const char s_lower_chars[] = "qwertyuiopasdfghjklzxcvbnm";
static const char s_upper_chars[] = "QWERTYUIOPASDFGHJKLZXCVBNM";
static const char s_symbol_chars[] = "0123456789!@#$%&*()-_=+?. ";
static const char s_extra_chars[] = "[]{}<>/\\|:;\"'`~^,!.?@#$%& ";
static const char s_more_chars[] = "0123456789!@#$%^&*()_+-=/? ";
static const uint8_t s_key_row_start[KEY_ROW_COUNT] = {0, 10, 19, 26};
static const uint8_t s_key_row_keys[KEY_ROW_COUNT] = {10, 9, 7, 6};

static void draw_row(int y, const char *label, const char *value,
                     bool focused, ui_color_t value_color) {
    ui_fill_rect(18, y, 284, 36,
                 focused ? UI_COLOR_PRIMARY_DARK : UI_COLOR_PANEL);
    ui_draw_rect(18, y, 284, 36, focused ? 3 : 1,
                 focused ? UI_COLOR_FOCUS : UI_COLOR_PANEL_ALT);
    ui_draw_text(28, y + 9, label, 2,
                 focused ? UI_COLOR_FOCUS : UI_COLOR_TEXT);
    if (value && value[0]) {
        int value_x = 292 - ui_text_width(value, 1);
        ui_draw_text(value_x, y + 13, value, 1, value_color);
    }
}

static void render_root(const gui_model_t *model, int focused_item) {
    ui_fill_rect(0, 0, APP_UI_W, 32, UI_COLOR_NAVY);
    ui_draw_text(12, 8, "设置", 2, UI_COLOR_TEXT);
    ui_draw_text(18, 38, "设备控制", 1, UI_COLOR_MUTED);

    draw_row(51, "安静模式", model->quiet_mode ? "开启" : "关闭",
             focused_item == 0,
             model->quiet_mode ? UI_COLOR_WARNING : UI_COLOR_SUCCESS);
    draw_row(89, "WiFi 设置",
             model->wifi_connected ? "已连接" :
             (model->wifi_configured ? "已保存" : "未设置"),
             focused_item == 1,
             model->wifi_connected ? UI_COLOR_SUCCESS : UI_COLOR_MUTED);
    draw_row(127, "API 设置", model->setup_mode ? "配置中" : "网页",
             focused_item == 2,
             model->setup_mode ? UI_COLOR_WARNING : UI_COLOR_CYAN);
    draw_row(165, "时间同步", "执行",
             focused_item == 3, UI_COLOR_CYAN);
}

static bool scan_is_saved(size_t scan_index) {
    for (size_t i = 0; i < s_profile_count; ++i) {
        if (strcmp(s_profiles[i].ssid, s_scan[scan_index].ssid) == 0) {
            return true;
        }
    }
    return false;
}

static size_t unsaved_scan_count(void) {
    size_t count = 0;
    for (size_t i = 0; i < s_scan_count; ++i) {
        if (!scan_is_saved(i)) count++;
    }
    return count;
}

static int wifi_item_count(void) {
    return 2 + (int)s_profile_count + (int)unsaved_scan_count() + 1;
}

static const wifi_scan_ap_t *unsaved_scan_at(size_t ordinal) {
    for (size_t i = 0; i < s_scan_count; ++i) {
        if (scan_is_saved(i)) continue;
        if (ordinal == 0) return &s_scan[i];
        ordinal--;
    }
    return NULL;
}

static void shorten_text(const char *source, char *dest, size_t capacity) {
    if (capacity == 0) return;
    const unsigned char *p = (const unsigned char *)(source ? source : "");
    size_t written = 0;
    while (*p && written + 1 < capacity) {
        size_t char_len = (*p & 0x80) == 0 ? 1 :
                          ((*p & 0xe0) == 0xc0 ? 2 :
                           ((*p & 0xf0) == 0xe0 ? 3 : 4));
        if (written + char_len >= capacity) break;
        memcpy(dest + written, p, char_len);
        written += char_len;
        p += char_len;
    }
    dest[written] = 0;
}

static void wifi_item_text(int item, char *label, size_t label_capacity,
                           char *value, size_t value_capacity,
                           ui_color_t *value_color) {
    value[0] = 0;
    *value_color = UI_COLOR_MUTED;
    if (item == 0) {
        snprintf(label, label_capacity, "重新扫描");
        snprintf(value, value_capacity, "%u 个", (unsigned)s_scan_count);
        *value_color = UI_COLOR_CYAN;
        return;
    }
    if (item == 1) {
        snprintf(label, label_capacity, "手动填写");
        snprintf(value, value_capacity, "隐藏网络");
        return;
    }

    int index = item - 2;
    if (index < (int)s_profile_count) {
        shorten_text(s_profiles[index].ssid, label, label_capacity);
        bool active = strcmp(wifi_time_ssid(), s_profiles[index].ssid) == 0;
        snprintf(value, value_capacity, "%s", active ? "当前" : "已保存");
        *value_color = active ? UI_COLOR_SUCCESS : UI_COLOR_WARNING;
        return;
    }
    index -= (int)s_profile_count;
    size_t unsaved_count = unsaved_scan_count();
    if (index < (int)unsaved_count) {
        const wifi_scan_ap_t *ap = unsaved_scan_at((size_t)index);
        shorten_text(ap ? ap->ssid : "", label, label_capacity);
        if (ap) {
            snprintf(value, value_capacity, "%ddBm %s", ap->rssi,
                     ap->secured ? "加密" : "开放");
            *value_color = ap->rssi >= -65 ? UI_COLOR_SUCCESS :
                           (ap->rssi >= -78 ? UI_COLOR_WARNING :
                                             UI_COLOR_DANGER);
        }
        return;
    }
    snprintf(label, label_capacity, "返回设置");
}

static void render_wifi(void) {
    ui_fill_rect(0, 0, APP_UI_W, 32, UI_COLOR_NAVY);
    ui_draw_text(12, 8, "WiFi 设置", 2, UI_COLOR_TEXT);
    ui_draw_text(16, 36, s_wifi_status, 1, UI_COLOR_MUTED);

    int item_count = wifi_item_count();
    if (s_wifi_focus < s_wifi_scroll) s_wifi_scroll = s_wifi_focus;
    if (s_wifi_focus >= s_wifi_scroll + WIFI_VISIBLE_ROWS) {
        s_wifi_scroll = s_wifi_focus - WIFI_VISIBLE_ROWS + 1;
    }
    int max_scroll = item_count > WIFI_VISIBLE_ROWS
                         ? item_count - WIFI_VISIBLE_ROWS : 0;
    if (s_wifi_scroll > max_scroll) s_wifi_scroll = max_scroll;

    for (int row = 0; row < WIFI_VISIBLE_ROWS; ++row) {
        int item = s_wifi_scroll + row;
        if (item >= item_count) break;
        char label[24];
        char value[20];
        ui_color_t color;
        wifi_item_text(item, label, sizeof(label), value, sizeof(value), &color);
        draw_row(54 + row * 38, label, value, item == s_wifi_focus, color);
    }

    if (s_wifi_scroll > 0) ui_draw_text(305, 55, "^", 1, UI_COLOR_MUTED);
    if (s_wifi_scroll + WIFI_VISIBLE_ROWS < item_count) {
        ui_draw_text(305, 190, "v", 1, UI_COLOR_MUTED);
    }
}

static void render_api(const gui_model_t *model) {
    ui_fill_rect(0, 0, APP_UI_W, 32, UI_COLOR_NAVY);
    ui_draw_text(12, 8, "API 设置", 2, UI_COLOR_TEXT);
    ui_draw_text(16, 42, model->setup_mode ? "配置热点已开启" : "正在开启配置热点",
                 2, model->setup_mode ? UI_COLOR_SUCCESS : UI_COLOR_WARNING);
    ui_draw_text(18, 76, "WiFi 名称", 1, UI_COLOR_MUTED);
    ui_draw_text(18, 91, APP_PROVISION_AP_SSID, 2, UI_COLOR_TEXT);
    ui_draw_text(18, 118, "WiFi 密码", 1, UI_COLOR_MUTED);
    ui_draw_text(18, 133, APP_PROVISION_AP_PASSWORD, 2, UI_COLOR_TEXT);
    ui_draw_text(18, 153, "设置网页  http://192.168.4.1/", 1, UI_COLOR_CYAN);
    draw_row(169, "退出设置模式", "", true, UI_COLOR_WARNING);
}

static const char *keyboard_chars(void) {
    switch (s_keyboard_mode) {
    case KEY_MODE_UPPER: return s_upper_chars;
    case KEY_MODE_SYMBOL: return s_symbol_chars;
    case KEY_MODE_SYMBOL_EXTRA: return s_extra_chars;
    case KEY_MODE_SYMBOL_MORE: return s_more_chars;
    default: return s_lower_chars;
    }
}

static const char *key_label(int key, char label[8]) {
    if (key < 26) {
        const char *chars = keyboard_chars();
        label[0] = chars[key];
        if (label[0] == ' ') return "SP";
        label[1] = 0;
        return label;
    }
    switch (key) {
    case 26: return "<";
    case 27: return ">";
    case 28: return "DEL";
    case 29:
        return s_keyboard_mode == KEY_MODE_LOWER ? "ABC" :
               s_keyboard_mode == KEY_MODE_UPPER ? "123" :
               s_keyboard_mode == KEY_MODE_SYMBOL ? "#+=" :
               s_keyboard_mode == KEY_MODE_SYMBOL_EXTRA ? "更多" : "abc";
    case 30: return "确定";
    default: return "取消";
    }
}

static void render_keyboard(void) {
    const bool password = s_keyboard_purpose == KEYBOARD_PASSWORD;
    ui_draw_text(10, 5, password ? "输入 WiFi 密码" : "输入 WiFi 名称",
                 1, UI_COLOR_TEXT);
    ui_draw_text(158, 5, s_keyboard_error[0] ? s_keyboard_error : "X 取消",
                 1, s_keyboard_error[0] ? UI_COLOR_DANGER : UI_COLOR_MUTED);

    char visible[33] = {0};
    size_t length = strlen(s_edit);
    size_t start = s_edit_cursor > 14 ? s_edit_cursor - 14 : 0;
    size_t shown = length - start;
    if (shown > 16) shown = 16;
    for (size_t i = 0; i < shown; ++i) {
        visible[i] = password ? '*' : s_edit[start + i];
    }
    size_t cursor = s_edit_cursor - start;
    if (cursor > shown) cursor = shown;

    ui_fill_rect(8, 27, 304, 34, UI_COLOR_PANEL);
    ui_draw_rect(8, 27, 304, 34, 2, UI_COLOR_CYAN);
    ui_draw_text(14, 35, visible, 2, UI_COLOR_TEXT);
    char before_cursor[33] = {0};
    memcpy(before_cursor, visible, cursor);
    int cursor_x = 14 + ui_text_width(before_cursor, 2);
    ui_draw_line(cursor_x, 32, cursor_x, 56, UI_COLOR_FOCUS);

    const int row_y[KEY_ROW_COUNT] = {66, 97, 128, 160};
    const int key_width[KEY_ROW_COUNT] = {30, 32, 40, 49};
    const int key_gap[KEY_ROW_COUNT] = {1, 2, 2, 2};
    const int key_height[KEY_ROW_COUNT] = {28, 28, 28, 41};
    for (int row = 0; row < KEY_ROW_COUNT; ++row) {
        int count = s_key_row_keys[row];
        int width = key_width[row];
        int gap = key_gap[row];
        int total_width = count * width + (count - 1) * gap;
        int start_x = (APP_UI_W - total_width) / 2;
        for (int column = 0; column < count; ++column) {
            int key = s_key_row_start[row] + column;
            int x = start_x + column * (width + gap);
            int y = row_y[row];
            bool focused = key == s_key_focus;
            ui_fill_rect(x, y, width, key_height[row],
                         focused ? UI_COLOR_PRIMARY_DARK : UI_COLOR_PANEL);
            ui_draw_rect(x, y, width, key_height[row], focused ? 2 : 1,
                         focused ? UI_COLOR_FOCUS : UI_COLOR_PANEL_ALT);
            char one_char[8];
            const char *label = key_label(key, one_char);
            uint8_t text_size = key < 26 ? 2 : 1;
            int text_x = x + (width - ui_text_width(label, text_size)) / 2;
            int text_y = y + (key_height[row] -
                              (text_size == 2 ? 14 : 7)) / 2;
            ui_draw_text(text_x, text_y, label, text_size,
                         focused ? UI_COLOR_FOCUS : UI_COLOR_TEXT);
        }
    }
}

static int key_row(int key) {
    for (int row = 0; row < KEY_ROW_COUNT; ++row) {
        int start = s_key_row_start[row];
        if (key >= start && key < start + s_key_row_keys[row]) return row;
    }
    return 0;
}

static int move_key_vertical(int key, int direction) {
    int row = key_row(key);
    int target_row = (row + direction + KEY_ROW_COUNT) % KEY_ROW_COUNT;
    int source_column = key - s_key_row_start[row];
    int source_count = s_key_row_keys[row];
    int target_count = s_key_row_keys[target_row];
    int target_column = source_count <= 1
                            ? 0
                            : (source_column * (target_count - 1) +
                               (source_count - 1) / 2) /
                                  (source_count - 1);
    return s_key_row_start[target_row] + target_column;
}

static void render(const gui_model_t *model, int focused_item) {
    switch (s_page) {
    case SETTINGS_PAGE_WIFI:
        render_wifi();
        break;
    case SETTINGS_PAGE_API:
        render_api(model);
        break;
    case SETTINGS_PAGE_KEYBOARD:
        render_keyboard();
        break;
    case SETTINGS_PAGE_ROOT:
    default:
        render_root(model, focused_item);
        break;
    }
}

static void load_profiles(void) {
    s_profile_count = 0;
    wifi_creds_list(s_profiles, WIFI_CREDS_MAX_PROFILES, &s_profile_count);
}

static void begin_keyboard(keyboard_purpose_t purpose, const char *initial) {
    s_page = SETTINGS_PAGE_KEYBOARD;
    s_keyboard_purpose = purpose;
    s_keyboard_mode = KEY_MODE_LOWER;
    s_edit_capacity = purpose == KEYBOARD_SSID
                          ? sizeof(s_draft_wifi.ssid)
                          : sizeof(s_draft_wifi.password);
    snprintf(s_edit, sizeof(s_edit), "%s", initial ? initial : "");
    s_edit[s_edit_capacity - 1] = 0;
    s_edit_cursor = strlen(s_edit);
    s_key_focus = 0;
    s_keyboard_error[0] = 0;
}

static gui_action_t queue_wifi(const wifi_creds_t *creds) {
    if (wifi_creds_save(creds) != 0) {
        snprintf(s_wifi_status, sizeof(s_wifi_status), "保存 WiFi 失败");
        s_page = SETTINGS_PAGE_WIFI;
        return GUI_ACTION_REDRAW;
    }
    s_pending_wifi = *creds;
    s_has_pending_wifi = true;
    load_profiles();
    s_wifi_focus = 2;
    s_page = SETTINGS_PAGE_WIFI;
    snprintf(s_wifi_status, sizeof(s_wifi_status), "正在连接 %s", creds->ssid);
    return GUI_ACTION_WIFI_CONNECT;
}

static gui_action_t activate_wifi_item(void) {
    if (s_wifi_focus == 0) {
        snprintf(s_wifi_status, sizeof(s_wifi_status), "正在扫描...");
        return GUI_ACTION_WIFI_SCAN;
    }
    if (s_wifi_focus == 1) {
        memset(&s_draft_wifi, 0, sizeof(s_draft_wifi));
        s_draft_secured = false;
        begin_keyboard(KEYBOARD_SSID, "");
        return GUI_ACTION_REDRAW;
    }

    int index = s_wifi_focus - 2;
    if (index < (int)s_profile_count) {
        return queue_wifi(&s_profiles[index]);
    }
    index -= (int)s_profile_count;
    size_t unsaved_count = unsaved_scan_count();
    if (index < (int)unsaved_count) {
        const wifi_scan_ap_t *ap = unsaved_scan_at((size_t)index);
        if (!ap) return GUI_ACTION_NONE;
        memset(&s_draft_wifi, 0, sizeof(s_draft_wifi));
        s_draft_secured = ap->secured;
        snprintf(s_draft_wifi.ssid, sizeof(s_draft_wifi.ssid), "%s", ap->ssid);
        if (!ap->secured) return queue_wifi(&s_draft_wifi);
        begin_keyboard(KEYBOARD_PASSWORD, "");
        return GUI_ACTION_REDRAW;
    }

    s_page = SETTINGS_PAGE_ROOT;
    return GUI_ACTION_REDRAW;
}

static gui_action_t activate_keyboard_key(void) {
    s_keyboard_error[0] = 0;
    if (s_key_focus < 26) {
        size_t length = strlen(s_edit);
        if (length + 1 >= s_edit_capacity) return GUI_ACTION_NONE;
        const char *chars = keyboard_chars();
        memmove(s_edit + s_edit_cursor + 1, s_edit + s_edit_cursor,
                length - s_edit_cursor + 1);
        s_edit[s_edit_cursor++] = chars[s_key_focus];
        return GUI_ACTION_REDRAW;
    }
    if (s_key_focus == 26) {
        if (s_edit_cursor > 0) s_edit_cursor--;
        return GUI_ACTION_REDRAW;
    }
    if (s_key_focus == 27) {
        if (s_edit_cursor < strlen(s_edit)) s_edit_cursor++;
        return GUI_ACTION_REDRAW;
    }
    if (s_key_focus == 28) {
        size_t length = strlen(s_edit);
        if (s_edit_cursor > 0) {
            memmove(s_edit + s_edit_cursor - 1, s_edit + s_edit_cursor,
                    length - s_edit_cursor + 1);
            s_edit_cursor--;
        }
        return GUI_ACTION_REDRAW;
    }
    if (s_key_focus == 29) {
        s_keyboard_mode = (keyboard_mode_t)((s_keyboard_mode + 1) % 5);
        return GUI_ACTION_REDRAW;
    }
    if (s_key_focus == 31) {
        memset(s_edit, 0, sizeof(s_edit));
        memset(&s_draft_wifi, 0, sizeof(s_draft_wifi));
        s_page = SETTINGS_PAGE_WIFI;
        return GUI_ACTION_REDRAW;
    }

    if (s_keyboard_purpose == KEYBOARD_SSID) {
        if (!s_edit[0]) return GUI_ACTION_NONE;
        size_t ssid_len = strlen(s_edit);
        if (ssid_len >= sizeof(s_draft_wifi.ssid)) {
            ssid_len = sizeof(s_draft_wifi.ssid) - 1;
        }
        memcpy(s_draft_wifi.ssid, s_edit, ssid_len);
        s_draft_wifi.ssid[ssid_len] = 0;
        begin_keyboard(KEYBOARD_PASSWORD, "");
        return GUI_ACTION_REDRAW;
    }
    size_t length = strlen(s_edit);
    bool hex_key = length == 64;
    for (size_t i = 0; hex_key && i < length; ++i) {
        if (!isxdigit((unsigned char)s_edit[i])) hex_key = false;
    }
    if ((s_draft_secured && length == 0) ||
        (length > 0 && length < 8) || (length == 64 && !hex_key)) {
        snprintf(s_keyboard_error, sizeof(s_keyboard_error), "密码需8-63位");
        return GUI_ACTION_REDRAW;
    }
    snprintf(s_draft_wifi.password, sizeof(s_draft_wifi.password), "%s", s_edit);
    memset(s_edit, 0, sizeof(s_edit));
    return queue_wifi(&s_draft_wifi);
}

static gui_action_t activate(uint8_t item) {
    if (s_page != SETTINGS_PAGE_ROOT) return GUI_ACTION_NONE;
    switch (item) {
    case 0:
        return GUI_ACTION_TOGGLE_QUIET;
    case 1:
        load_profiles();
        s_page = SETTINGS_PAGE_WIFI;
        s_wifi_focus = 0;
        s_wifi_scroll = 0;
        snprintf(s_wifi_status, sizeof(s_wifi_status), "正在扫描...");
        return GUI_ACTION_WIFI_SCAN;
    case 2:
        s_page = SETTINGS_PAGE_API;
        return GUI_ACTION_API_SETUP_START;
    case 3:
        return GUI_ACTION_SYNC_TIME;
    default:
        return GUI_ACTION_NONE;
    }
}

static bool handle_input(gui_input_t input, gui_action_t *action) {
    if (s_page == SETTINGS_PAGE_ROOT) return false;
    *action = GUI_ACTION_NONE;
    if (input == GUI_INPUT_BACK) {
        if (s_page == SETTINGS_PAGE_KEYBOARD) {
            memset(s_edit, 0, sizeof(s_edit));
            memset(&s_draft_wifi, 0, sizeof(s_draft_wifi));
            s_page = SETTINGS_PAGE_WIFI;
            *action = GUI_ACTION_REDRAW;
        } else {
            *action = s_page == SETTINGS_PAGE_API
                          ? GUI_ACTION_API_SETUP_STOP : GUI_ACTION_REDRAW;
            s_page = SETTINGS_PAGE_ROOT;
        }
        return true;
    }

    if (s_page == SETTINGS_PAGE_WIFI) {
        int count = wifi_item_count();
        if (input == GUI_INPUT_UP || input == GUI_INPUT_PREVIOUS) {
            s_wifi_focus = s_wifi_focus > 0 ? s_wifi_focus - 1 : count - 1;
            *action = GUI_ACTION_REDRAW;
        } else if (input == GUI_INPUT_DOWN || input == GUI_INPUT_NEXT) {
            s_wifi_focus = (s_wifi_focus + 1) % count;
            *action = GUI_ACTION_REDRAW;
        } else if (input == GUI_INPUT_LEFT) {
            s_page = SETTINGS_PAGE_ROOT;
            *action = GUI_ACTION_REDRAW;
        } else if (input == GUI_INPUT_ACTIVATE ||
                   input == GUI_INPUT_AUX_ACTIVATE) {
            *action = activate_wifi_item();
        }
        return true;
    }

    if (s_page == SETTINGS_PAGE_API) {
        if (input == GUI_INPUT_ACTIVATE || input == GUI_INPUT_AUX_ACTIVATE ||
            input == GUI_INPUT_LONG_PRESS || input == GUI_INPUT_LEFT) {
            s_page = SETTINGS_PAGE_ROOT;
            *action = GUI_ACTION_API_SETUP_STOP;
        }
        return true;
    }

    if (input == GUI_INPUT_LEFT) {
        int row = key_row(s_key_focus);
        int start = s_key_row_start[row];
        s_key_focus = s_key_focus == start
                          ? start + s_key_row_keys[row] - 1
                          : s_key_focus - 1;
        *action = GUI_ACTION_REDRAW;
    } else if (input == GUI_INPUT_RIGHT) {
        int row = key_row(s_key_focus);
        int start = s_key_row_start[row];
        s_key_focus = s_key_focus == start + s_key_row_keys[row] - 1
                          ? start : s_key_focus + 1;
        *action = GUI_ACTION_REDRAW;
    } else if (input == GUI_INPUT_UP) {
        s_key_focus = move_key_vertical(s_key_focus, -1);
        *action = GUI_ACTION_REDRAW;
    } else if (input == GUI_INPUT_DOWN) {
        s_key_focus = move_key_vertical(s_key_focus, 1);
        *action = GUI_ACTION_REDRAW;
    } else if (input == GUI_INPUT_ACTIVATE ||
               input == GUI_INPUT_AUX_ACTIVATE) {
        *action = activate_keyboard_key();
    }
    return true;
}

static void enter(void) {
    s_page = SETTINGS_PAGE_ROOT;
    load_profiles();
}

static const gui_focus_node_t s_focus_grid[] = {
    {.left = GUI_FOCUS_NONE, .right = GUI_FOCUS_NONE,
     .up = GUI_FOCUS_HOME, .down = 1},
    {.left = GUI_FOCUS_NONE, .right = GUI_FOCUS_NONE,
     .up = 0, .down = 2},
    {.left = GUI_FOCUS_NONE, .right = GUI_FOCUS_NONE,
     .up = 1, .down = 3},
    {.left = GUI_FOCUS_NONE, .right = GUI_FOCUS_NONE,
     .up = 2, .down = GUI_FOCUS_HOME},
};

static const gui_app_t s_app = {
    .id = "settings",
    .label = "设置",
    .icon = GUI_ICON_SETTINGS,
    .focus_count = 4,
    .focus_grid = s_focus_grid,
    .enter = enter,
    .exit = NULL,
    .tick = NULL,
    .render = render,
    .activate = activate,
    .handle_input = handle_input,
};

const gui_app_t *app_settings_descriptor(void) {
    return &s_app;
}

void app_settings_set_wifi_scan(const wifi_scan_ap_t *results, size_t count,
                                esp_err_t result) {
    if (result != ESP_OK) {
        s_scan_count = 0;
        snprintf(s_wifi_status, sizeof(s_wifi_status), "扫描失败: %s",
                 esp_err_to_name(result));
        return;
    }
    if (!results) count = 0;
    if (count > WIFI_SCAN_MAX_RESULTS) count = WIFI_SCAN_MAX_RESULTS;
    if (count > 0 && results) {
        memcpy(s_scan, results, count * sizeof(s_scan[0]));
    }
    s_scan_count = count;
    snprintf(s_wifi_status, sizeof(s_wifi_status), "发现 %u 个网络",
             (unsigned)count);
}

bool app_settings_take_wifi_credentials(wifi_creds_t *out) {
    if (!s_has_pending_wifi) return false;
    if (out) *out = s_pending_wifi;
    s_has_pending_wifi = false;
    return true;
}

void app_settings_set_wifi_status(const char *status) {
    snprintf(s_wifi_status, sizeof(s_wifi_status), "%s",
             status ? status : "");
}
