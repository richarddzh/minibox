#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <esp_err.h>

typedef enum {
    UI_COLOR_BLACK = 0,
    UI_COLOR_WHITE,
    UI_COLOR_BG,
    UI_COLOR_PANEL,
    UI_COLOR_PANEL_ALT,
    UI_COLOR_PRIMARY,
    UI_COLOR_PRIMARY_DARK,
    UI_COLOR_FOCUS,
    UI_COLOR_SUCCESS,
    UI_COLOR_WARNING,
    UI_COLOR_DANGER,
    UI_COLOR_TEXT,
    UI_COLOR_MUTED,
    UI_COLOR_CYAN,
    UI_COLOR_MAGENTA,
    UI_COLOR_NAVY,
} ui_color_t;

esp_err_t ui_init(void);
/* ui_init owns ST7796 and starts a locked LVGL service task. Do not run
 * another display writer. Each ui_begin/ui_flush pair is one transaction. */
void ui_task_handler(void);
void ui_begin(ui_color_t color);
void ui_set_pixel(int x, int y, ui_color_t color);
void ui_fill_rect(int x, int y, int width, int height, ui_color_t color);
void ui_draw_rect(int x, int y, int width, int height,
                  int thickness, ui_color_t color);
void ui_fill_round_rect(int x, int y, int width, int height,
                        int radius, ui_color_t color);
void ui_draw_round_rect(int x, int y, int width, int height,
                        int radius, int thickness, ui_color_t color);
void ui_fill_circle(int center_x, int center_y, int radius, ui_color_t color);
void ui_fill_triangle(int x0, int y0, int x1, int y1,
                      int x2, int y2, ui_color_t color);
void ui_draw_line(int x0, int y0, int x1, int y1, ui_color_t color);
void ui_draw_text(int x, int y, const char *text,
                  uint8_t size, ui_color_t color);
void ui_draw_text_box(int x, int y, int width, int height, const char *text,
                      uint8_t size, ui_color_t color);
int ui_text_width(const char *text, uint8_t size);
esp_err_t ui_flush(void);
void ui_invalidate(void);
esp_err_t ui_draw_rgb565_bitmap(int x, int y, int width, int height,
                                const uint16_t *pixels,
                                const uint8_t *alpha,
                                ui_color_t background);
/* Bitmap sources are immutable RGB565 assets; alpha is one 0..255 byte/pixel.
 * Images are retained and scaled by LVGL, never sent around its display driver. */

// Big clock face with date on top, large HH:MM in middle, small SS on
// the right, and two small footer lines.
void ui_show_clock(const char *date, const char *time,
                   const char *footer1, const char *footer2);

// Stacked 5-line text screen (title + 4 lines). NULL is treated as "".
void ui_show_status(const char *title,
                    const char *line1, const char *line2,
                    const char *line3, const char *line4);

// Rounded face panel with title, two large rows and a footer.
// NULL fields are blank.
void ui_show_face(const char *title,
                  const char *face_line1,
                  const char *face_line2,
                  const char *footer);
