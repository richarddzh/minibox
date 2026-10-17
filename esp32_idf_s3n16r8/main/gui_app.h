#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    GUI_ICON_CHAT = 0,
    GUI_ICON_SETTINGS,
    GUI_ICON_RADIO,
    GUI_ICON_TETRIS,
    GUI_ICON_SNAKE,
} gui_icon_t;

typedef enum {
    GUI_INPUT_PREVIOUS = 0,
    GUI_INPUT_NEXT,
    GUI_INPUT_UP,
    GUI_INPUT_DOWN,
    GUI_INPUT_LEFT,
    GUI_INPUT_RIGHT,
    GUI_INPUT_ACTIVATE,
    GUI_INPUT_AUX_ACTIVATE,
    GUI_INPUT_LONG_PRESS,
    GUI_INPUT_RELEASE,
    GUI_INPUT_BACK,
} gui_input_t;

typedef enum {
    GUI_ACTION_NONE = 0,
    GUI_ACTION_REDRAW,
    GUI_ACTION_CHIME,
    GUI_ACTION_TOGGLE_QUIET,
    GUI_ACTION_WIFI_SCAN,
    GUI_ACTION_WIFI_CONNECT,
    GUI_ACTION_API_SETUP_START,
    GUI_ACTION_API_SETUP_STOP,
    GUI_ACTION_SYNC_TIME,
    GUI_ACTION_START_VOICE,
    GUI_ACTION_STOP_VOICE,
    GUI_ACTION_RADIO_TOGGLE,
} gui_action_t;

#define GUI_FOCUS_HOME (-1)
#define GUI_FOCUS_NONE (-2)

typedef struct {
    int8_t left;
    int8_t right;
    int8_t up;
    int8_t down;
} gui_focus_node_t;

typedef struct {
    const char *date;
    const char *time;
    const char *status;
    const char *voice_title;
    const char *voice_line1;
    const char *voice_line2;
    bool time_valid;
    bool wifi_configured;
    bool wifi_connected;
    bool setup_mode;
    bool quiet_mode;
    bool audio_busy;
    bool voice_visible;
} gui_model_t;

typedef struct {
    const char *id;
    const char *label;
    gui_icon_t icon;
    uint8_t focus_count;
    const gui_focus_node_t *focus_grid;
    void (*enter)(void);
    void (*exit)(void);
    gui_action_t (*tick)(int64_t now_ms);
    void (*render)(const gui_model_t *model, int focused_item);
    gui_action_t (*activate)(uint8_t item);
    bool (*handle_input)(gui_input_t input, gui_action_t *action);
} gui_app_t;
