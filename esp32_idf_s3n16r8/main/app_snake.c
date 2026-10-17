#include "app_snake.h"

#include "ui_screen.h"

#include <esp_random.h>
#include <stdio.h>
#include <string.h>

#define GRID_W 19
#define GRID_H 15
#define CELL 12
#define FIELD_X 4
#define FIELD_Y 24
#define SNAKE_CAPACITY 128
#define MOVE_MS 220

typedef struct {
    int8_t x;
    int8_t y;
} point_t;

typedef enum {
    DIR_UP,
    DIR_RIGHT,
    DIR_DOWN,
    DIR_LEFT,
} direction_t;

static point_t s_snake[SNAKE_CAPACITY];
static int s_length;
static point_t s_apple;
static direction_t s_direction;
static direction_t s_pending_direction;
static int s_score;
static int64_t s_last_move_ms;
static bool s_running;
static bool s_game_over;
static bool s_paused;

static bool point_equal(point_t a, point_t b) {
    return a.x == b.x && a.y == b.y;
}

static bool on_snake(point_t point) {
    for (int i = 0; i < s_length; ++i) {
        if (point_equal(point, s_snake[i])) return true;
    }
    return false;
}

static void place_apple(void) {
    int start = esp_random() % (GRID_W * GRID_H);
    for (int attempt = 0; attempt < GRID_W * GRID_H; ++attempt) {
        int cell = (start + attempt) % (GRID_W * GRID_H);
        point_t point = {
            .x = (int8_t)(cell % GRID_W),
            .y = (int8_t)(cell / GRID_W),
        };
        if (!on_snake(point)) {
            s_apple = point;
            return;
        }
    }
}

static void start_game(void) {
    s_length = 5;
    for (int i = 0; i < s_length; ++i) {
        s_snake[i] = (point_t){.x = (int8_t)(9 - i), .y = 7};
    }
    s_direction = DIR_RIGHT;
    s_pending_direction = DIR_RIGHT;
    s_score = 0;
    s_running = true;
    s_game_over = false;
    s_paused = false;
    s_last_move_ms = 0;
    place_apple();
}

static bool directions_opposite(direction_t a, direction_t b) {
    return (a == DIR_UP && b == DIR_DOWN) ||
           (a == DIR_DOWN && b == DIR_UP) ||
           (a == DIR_LEFT && b == DIR_RIGHT) ||
           (a == DIR_RIGHT && b == DIR_LEFT);
}

static void set_direction(direction_t direction) {
    if (!directions_opposite(direction, s_direction)) {
        s_pending_direction = direction;
    }
}

static void move_snake(void) {
    s_direction = s_pending_direction;
    point_t next = s_snake[0];
    if (s_direction == DIR_UP) --next.y;
    if (s_direction == DIR_RIGHT) ++next.x;
    if (s_direction == DIR_DOWN) ++next.y;
    if (s_direction == DIR_LEFT) --next.x;

    bool ate = point_equal(next, s_apple);
    bool collision = false;
    int occupied = s_length - (ate ? 0 : 1);
    for (int i = 0; i < occupied; ++i) {
        if (point_equal(next, s_snake[i])) collision = true;
    }
    if (next.x < 0 || next.x >= GRID_W || next.y < 0 || next.y >= GRID_H ||
        collision) {
        s_running = false;
        s_game_over = true;
        return;
    }

    if (ate && s_length < SNAKE_CAPACITY) ++s_length;
    memmove(&s_snake[1], &s_snake[0],
            sizeof(s_snake[0]) * (size_t)(s_length - 1));
    s_snake[0] = next;
    if (ate) {
        s_score += 10;
        if (s_length == SNAKE_CAPACITY) {
            s_running = false;
            s_game_over = true;
            return;
        }
        place_apple();
    }
}

static gui_action_t tick(int64_t now_ms) {
    if (!s_running || s_paused) return GUI_ACTION_NONE;
    int interval = MOVE_MS - s_score;
    if (interval < 90) interval = 90;
    if (s_last_move_ms == 0) s_last_move_ms = now_ms;
    if (now_ms - s_last_move_ms >= interval) {
        move_snake();
        s_last_move_ms = now_ms;
        return GUI_ACTION_REDRAW;
    }
    return GUI_ACTION_NONE;
}

static direction_t direction_between(point_t from, point_t to) {
    if (to.x > from.x) return DIR_RIGHT;
    if (to.x < from.x) return DIR_LEFT;
    if (to.y > from.y) return DIR_DOWN;
    return DIR_UP;
}

static void draw_body_block(int x, int y, direction_t toward_head,
                            direction_t toward_tail) {
    ui_fill_circle(x + CELL / 2, y + CELL / 2,
                   CELL / 2 - 1, UI_COLOR_SUCCESS);
    direction_t directions[2] = {toward_head, toward_tail};
    for (int i = 0; i < 2; ++i) {
        if (directions[i] == DIR_LEFT) {
            ui_fill_rect(x, y + 2, CELL / 2 + 1, CELL - 4, UI_COLOR_SUCCESS);
        } else if (directions[i] == DIR_RIGHT) {
            ui_fill_rect(x + CELL / 2, y + 2,
                         CELL / 2, CELL - 4, UI_COLOR_SUCCESS);
        } else if (directions[i] == DIR_UP) {
            ui_fill_rect(x + 2, y, CELL - 4, CELL / 2 + 1, UI_COLOR_SUCCESS);
        } else {
            ui_fill_rect(x + 2, y + CELL / 2,
                         CELL - 4, CELL / 2, UI_COLOR_SUCCESS);
        }
    }
}

static void draw_head(point_t point) {
    int x = FIELD_X + point.x * CELL;
    int y = FIELD_Y + point.y * CELL;
    ui_fill_round_rect(x, y, CELL, CELL, 4, UI_COLOR_SUCCESS);
    if (s_direction == DIR_LEFT || s_direction == DIR_RIGHT) {
        int eye_x = s_direction == DIR_RIGHT ? x + CELL - 3 : x + 2;
        ui_fill_circle(eye_x, y + 3, 2, UI_COLOR_WHITE);
        ui_fill_circle(eye_x, y + CELL - 4, 2, UI_COLOR_WHITE);
        ui_set_pixel(eye_x, y + 3, UI_COLOR_BLACK);
        ui_set_pixel(eye_x, y + CELL - 4, UI_COLOR_BLACK);
        int tongue_x = s_direction == DIR_RIGHT ? x + CELL + 2 : x - 3;
        ui_draw_line(s_direction == DIR_RIGHT ? x + CELL : x,
                     y + CELL / 2, tongue_x, y + CELL / 2, UI_COLOR_DANGER);
    } else {
        int eye_y = s_direction == DIR_DOWN ? y + CELL - 3 : y + 2;
        ui_fill_circle(x + 3, eye_y, 2, UI_COLOR_WHITE);
        ui_fill_circle(x + CELL - 4, eye_y, 2, UI_COLOR_WHITE);
        ui_set_pixel(x + 3, eye_y, UI_COLOR_BLACK);
        ui_set_pixel(x + CELL - 4, eye_y, UI_COLOR_BLACK);
        int tongue_y = s_direction == DIR_DOWN ? y + CELL + 2 : y - 3;
        ui_draw_line(x + CELL / 2, s_direction == DIR_DOWN ? y + CELL : y,
                     x + CELL / 2, tongue_y, UI_COLOR_DANGER);
    }
}

static void draw_tail(point_t point, direction_t toward_head) {
    int x = FIELD_X + point.x * CELL;
    int y = FIELD_Y + point.y * CELL;
    ui_fill_round_rect(x + 2, y + 2, CELL - 4, CELL - 4, 3, UI_COLOR_SUCCESS);
    if (toward_head == DIR_LEFT || toward_head == DIR_RIGHT) {
        ui_fill_triangle(toward_head == DIR_RIGHT ? x + CELL - 2 : x + 2,
                         y + 2,
                         toward_head == DIR_RIGHT ? x + CELL - 2 : x + 2,
                         y + CELL - 2,
                         toward_head == DIR_RIGHT ? x + 1 : x + CELL - 1,
                         y + CELL / 2,
                         UI_COLOR_SUCCESS);
    } else {
        ui_fill_triangle(x + 2,
                         toward_head == DIR_DOWN ? y + CELL - 2 : y + 2,
                         x + CELL - 2,
                         toward_head == DIR_DOWN ? y + CELL - 2 : y + 2,
                         x + CELL / 2,
                         toward_head == DIR_DOWN ? y + 1 : y + CELL - 1,
                         UI_COLOR_SUCCESS);
    }
}

static void render(const gui_model_t *model, int focused_item) {
    (void)model;
    ui_draw_text(6, 5, "贪吃蛇", 2, UI_COLOR_TEXT);
    ui_fill_rect(FIELD_X, FIELD_Y, GRID_W * CELL,
                 GRID_H * CELL, UI_COLOR_BLACK);
    ui_draw_rect(FIELD_X - 2, FIELD_Y - 2,
                 GRID_W * CELL + 4, GRID_H * CELL + 4,
                 2, UI_COLOR_PANEL_ALT);

    int apple_x = FIELD_X + s_apple.x * CELL + CELL / 2;
    int apple_y = FIELD_Y + s_apple.y * CELL + CELL / 2;
    ui_fill_circle(apple_x, apple_y, 5, UI_COLOR_DANGER);
    ui_draw_line(apple_x, apple_y - 5, apple_x + 2, apple_y - 8,
                 UI_COLOR_SUCCESS);

    if (s_length > 0) {
        for (int i = 1; i < s_length - 1; ++i) {
            direction_t toward_head =
                direction_between(s_snake[i], s_snake[i - 1]);
            direction_t toward_tail =
                direction_between(s_snake[i], s_snake[i + 1]);
            draw_body_block(FIELD_X + s_snake[i].x * CELL,
                            FIELD_Y + s_snake[i].y * CELL,
                            toward_head, toward_tail);
        }
        if (s_length > 1) {
            draw_tail(s_snake[s_length - 1],
                      direction_between(s_snake[s_length - 1],
                                        s_snake[s_length - 2]));
        }
        draw_head(s_snake[0]);
    }

    char score[24];
    ui_draw_text(242, 44, "苹果", 1, UI_COLOR_MUTED);
    snprintf(score, sizeof(score), "%d", s_score / 10);
    ui_draw_text(244, 60, score, 2, UI_COLOR_FOCUS);
    ui_draw_text(240, 92, s_paused ? "已暂停" : "摇杆移动", 1, UI_COLOR_MUTED);
    ui_draw_text(240, 112, "GPIO7", 1, UI_COLOR_CYAN);
    ui_draw_text(240, 129, "暂停/继续", 1, UI_COLOR_MUTED);
    ui_fill_round_rect(239, 5, 74, 28, 5,
                       focused_item == 0 ? UI_COLOR_PRIMARY_DARK
                                         : UI_COLOR_PANEL);
    ui_draw_rect(239, 5, 74, 28, focused_item == 0 ? 3 : 1,
                 focused_item == 0 ? UI_COLOR_FOCUS : UI_COLOR_PANEL_ALT);
    ui_draw_text(260, 15, s_running ? "重开" : "开始", 1,
                 focused_item == 0 ? UI_COLOR_FOCUS : UI_COLOR_TEXT);
    if (s_game_over) {
        ui_draw_text(240, 154, "游戏", 1, UI_COLOR_DANGER);
        ui_draw_text(240, 168, "结束", 1, UI_COLOR_DANGER);
    }
}

static gui_action_t activate(uint8_t item) {
    if (item == 0) start_game();
    return GUI_ACTION_NONE;
}

static bool handle_input(gui_input_t input, gui_action_t *action) {
    if (!s_running) return false;
    *action = GUI_ACTION_REDRAW;
    if (input == GUI_INPUT_BACK) return false;
    if (input == GUI_INPUT_AUX_ACTIVATE) {
        s_paused = !s_paused;
        s_last_move_ms = 0;
        return true;
    }
    if (s_paused) return true;
    if (input == GUI_INPUT_UP) set_direction(DIR_UP);
    else if (input == GUI_INPUT_RIGHT) set_direction(DIR_RIGHT);
    else if (input == GUI_INPUT_DOWN) set_direction(DIR_DOWN);
    else if (input == GUI_INPUT_LEFT) set_direction(DIR_LEFT);
    else if (input == GUI_INPUT_ACTIVATE ||
             input == GUI_INPUT_AUX_ACTIVATE) return true;
    else return false;
    return true;
}

static void enter(void) {
    s_paused = false;
    s_last_move_ms = 0;
}

static const gui_focus_node_t s_focus_grid[] = {
    {.left = GUI_FOCUS_HOME, .right = GUI_FOCUS_HOME,
     .up = GUI_FOCUS_HOME, .down = GUI_FOCUS_HOME},
};

static const gui_app_t s_app = {
    .id = "snake",
    .label = "贪吃蛇",
    .icon = GUI_ICON_SNAKE,
    .focus_count = 1,
    .focus_grid = s_focus_grid,
    .enter = enter,
    .exit = NULL,
    .tick = tick,
    .render = render,
    .activate = activate,
    .handle_input = handle_input,
};

const gui_app_t *app_snake_descriptor(void) {
    return &s_app;
}
