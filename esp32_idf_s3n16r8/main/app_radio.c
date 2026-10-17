#include "app_radio.h"

#include "app_config.h"
#include "radio_player.h"
#include "radio_stations.h"
#include "ui_screen.h"

#include <stdio.h>

#define RADIO_ROW_Y 42
#define RADIO_ROW_H 25
#define RADIO_VISIBLE_ROWS 4
#define RADIO_SPECTRUM_Y 164
#define RADIO_SPECTRUM_H 27

static uint32_t s_seen_generation;
static size_t s_selected_station;
static gui_focus_node_t s_focus_grid[RADIO_STATION_COUNT];

static const char *state_label(radio_player_state_t state) {
    switch (state) {
    case RADIO_PLAYER_CONNECTING: return "连接";
    case RADIO_PLAYER_BUFFERING: return "缓冲";
    case RADIO_PLAYER_PLAYING: return "播放";
    case RADIO_PLAYER_STOPPING: return "停止";
    case RADIO_PLAYER_ERROR: return "错误";
    case RADIO_PLAYER_IDLE:
    default: return "空闲";
    }
}

static void enter(void) {
    radio_player_snapshot_t snapshot;
    radio_player_get_snapshot(&snapshot);
    s_seen_generation = snapshot.generation;
    if (snapshot.station_index < radio_station_count()) {
        s_selected_station = snapshot.station_index;
    }
    for (size_t i = 0; i < radio_station_count(); ++i) {
        s_focus_grid[i] = (gui_focus_node_t){
            .left = GUI_FOCUS_NONE,
            .right = GUI_FOCUS_NONE,
            .up = i == 0 ? GUI_FOCUS_HOME : (int8_t)(i - 1),
            .down = i + 1 == radio_station_count()
                        ? GUI_FOCUS_HOME
                        : (int8_t)(i + 1),
        };
    }
}

static void exit_app(void) {
    radio_player_stop(4000);
}

static gui_action_t tick(int64_t now_ms) {
    (void)now_ms;
    radio_player_snapshot_t snapshot;
    radio_player_get_snapshot(&snapshot);
    if (snapshot.generation != s_seen_generation) {
        s_seen_generation = snapshot.generation;
        return GUI_ACTION_REDRAW;
    }
    return GUI_ACTION_NONE;
}

static void render(const gui_model_t *model, int focused_item) {
    (void)model;
    radio_player_snapshot_t snapshot;
    radio_player_get_snapshot(&snapshot);

    ui_fill_rect(0, 0, APP_UI_W, 32, UI_COLOR_NAVY);
    ui_draw_text(12, 8, "网络电台", 2, UI_COLOR_TEXT);
    ui_draw_text(148, 8, state_label(snapshot.state), 2,
                 snapshot.state == RADIO_PLAYER_ERROR
                     ? UI_COLOR_DANGER
                     : snapshot.state == RADIO_PLAYER_PLAYING
                           ? UI_COLOR_SUCCESS
                           : UI_COLOR_CYAN);
    ui_draw_text_box(194, 10, 116, 21, snapshot.message, 1, UI_COLOR_MUTED);

    size_t station_count = radio_station_count();
    size_t visible = station_count < RADIO_VISIBLE_ROWS ? station_count : RADIO_VISIBLE_ROWS;
    size_t first = 0;
    if (focused_item >= RADIO_VISIBLE_ROWS) {
        first = (size_t)focused_item - RADIO_VISIBLE_ROWS + 1;
    }
    if (first + visible > station_count) {
        first = station_count - visible;
    }
    for (size_t row = 0; row < visible; ++row) {
        size_t i = first + row;
        const radio_station_t *station = radio_station_get(i);
        int y = RADIO_ROW_Y + (int)row * RADIO_ROW_H;
        bool focused = focused_item == (int)i;
        bool active = radio_player_busy() &&
                      snapshot.station_index == i;
        ui_fill_round_rect(14, y, 292, RADIO_ROW_H - 3, 5,
                           focused ? UI_COLOR_PRIMARY_DARK : UI_COLOR_PANEL);
        ui_draw_round_rect(14, y, 292, RADIO_ROW_H - 3, 5,
                           focused ? 3 : 1,
                           focused ? UI_COLOR_FOCUS : UI_COLOR_PANEL_ALT);
        ui_fill_circle(28, y + 12, 5,
                       active ? UI_COLOR_SUCCESS : UI_COLOR_MUTED);
        ui_draw_text_box(42, y + 4, 220, 18, station->name, 1,
                     focused ? UI_COLOR_FOCUS : UI_COLOR_TEXT);
        ui_draw_text(269, y + 5, station->region, 1, UI_COLOR_MUTED);
    }

    char volume[32];
    snprintf(volume, sizeof(volume), "音量 %u/5  左右调节",
             (unsigned)snapshot.volume_level);
    ui_draw_text(16, 143, volume, 1, UI_COLOR_CYAN);

    const int bar_w = 22;
    const int gap = 5;
    for (size_t i = 0; i < RADIO_SPECTRUM_BANDS; ++i) {
        int height = snapshot.spectrum[i] * RADIO_SPECTRUM_H / 100;
        if (snapshot.state == RADIO_PLAYER_PLAYING && height < 2) height = 2;
        int x = 20 + (int)i * (bar_w + gap);
        ui_fill_rect(x, RADIO_SPECTRUM_Y, bar_w, RADIO_SPECTRUM_H,
                     UI_COLOR_PANEL);
        ui_fill_rect(x, RADIO_SPECTRUM_Y + RADIO_SPECTRUM_H - height,
                     bar_w, height,
                     i < 3 ? UI_COLOR_SUCCESS
                           : i < 7 ? UI_COLOR_CYAN : UI_COLOR_MAGENTA);
    }

    char format[56];
    if (snapshot.sample_rate) {
        snprintf(format, sizeof(format), "%u Hz  %u ch  %u kbps  %u-%u/%u",
                 (unsigned)snapshot.sample_rate,
                 (unsigned)snapshot.channels,
                 (unsigned)(snapshot.bitrate / 1000),
                 (unsigned)(first + 1),
                 (unsigned)(first + visible),
                 (unsigned)radio_station_count());
    } else {
        snprintf(format, sizeof(format), "GPIO4 播放/停止  %u-%u/%u",
                 (unsigned)(station_count ? first + 1 : 0),
                 (unsigned)(first + visible),
                 (unsigned)radio_station_count());
    }
    ui_draw_text(16, 196, format, 1, UI_COLOR_MUTED);
}

static gui_action_t activate(uint8_t item) {
    if (item >= radio_station_count()) return GUI_ACTION_NONE;
    s_selected_station = item;
    return GUI_ACTION_RADIO_TOGGLE;
}

static bool handle_input(gui_input_t input, gui_action_t *action) {
    if (input != GUI_INPUT_LEFT && input != GUI_INPUT_RIGHT) return false;
    radio_player_snapshot_t snapshot;
    radio_player_get_snapshot(&snapshot);
    int level = snapshot.volume_level +
                (input == GUI_INPUT_RIGHT ? 1 : -1);
    if (level < RADIO_VOLUME_MIN) level = RADIO_VOLUME_MIN;
    if (level > RADIO_VOLUME_MAX) level = RADIO_VOLUME_MAX;
    radio_player_set_volume((uint8_t)level);
    *action = GUI_ACTION_REDRAW;
    return true;
}

static const gui_app_t s_app = {
    .id = "radio",
    .label = "网络电台",
    .icon = GUI_ICON_RADIO,
    .focus_count = RADIO_STATION_COUNT,
    .focus_grid = s_focus_grid,
    .enter = enter,
    .exit = exit_app,
    .tick = tick,
    .render = render,
    .activate = activate,
    .handle_input = handle_input,
};

const gui_app_t *app_radio_descriptor(void) {
    return &s_app;
}

size_t app_radio_selected_station(void) {
    return s_selected_station;
}
