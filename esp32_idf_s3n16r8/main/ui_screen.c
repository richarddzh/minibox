#include "ui_screen.h"

#include "app_config.h"
#include "st7796.h"
#include "ui_font.h"
#include "lvgl.h"

#include <stdatomic.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#if LV_COLOR_DEPTH != 16
#error "Minibox requires LV_COLOR_DEPTH=16"
#endif

#define PANEL_W 480
#define PANEL_H 320
#define DRAW_ROWS 24
#define VIDEO_DIRTY_ROWS 16
#define MAX_NODES 512

typedef enum { NODE_RECT, NODE_LABEL, NODE_LINE, NODE_TRIANGLE, NODE_IMAGE } node_kind_t;

typedef struct {
    node_kind_t kind;
    int x, y, w, h, radius, border, size;
    ui_color_t color;
    lv_point_t points[3];
    const void *source;
    const void *alpha;
    ui_color_t background;
} command_t;

typedef struct {
    lv_obj_t *obj;
    command_t command;
    lv_point_t points[3];
    lv_img_dsc_t image;
    uint8_t *image_data;
    bool valid;
} node_t;

static const char *TAG = "ui_lvgl";
static const uint32_t s_palette[] = {
    0x080c14, 0xffffff, 0x101724, 0x1b283b,
    0x30415b, 0x4a9cff, 0x194c78, 0xffd85a,
    0x67d6aa, 0xffb45b, 0xff6c80, 0xf0f5ff,
    0xa0afc6, 0x64d9ee, 0xd39af7, 0x131f32,
};

static SemaphoreHandle_t s_lock;
static lv_disp_t *s_display;
static lv_obj_t *s_root;
static lv_disp_draw_buf_t s_draw_buffer;
static lv_color_t *s_pixels;
static node_t *s_nodes;
static size_t s_used, s_previous_used;
static bool s_frame_open;
static bool s_overflow;
static int s_background = -1;
static int64_t s_tick_us;
static esp_err_t s_transfer_error;
static lv_obj_t *s_video;
static lv_img_dsc_t s_video_image;
static lv_color_t *s_video_pixels;
static int s_video_width, s_video_height;
static uint16_t s_video_x[PANEL_W], s_video_y[PANEL_H];
static SemaphoreHandle_t s_video_lock;
static uint8_t *s_video_indexed[2];
static uint16_t s_video_palette[2][256];
static unsigned s_video_front, s_video_back;
static bool s_video_pending;
static atomic_int s_video_error;

static bool update_video(void);

static int px(int x) { return x * PANEL_W / APP_UI_W; }
static int py(int y) { return y * PANEL_H / APP_UI_H; }
static lv_color_t color(ui_color_t value) {
    return lv_color_hex(s_palette[(unsigned)value < 16 ? value : UI_COLOR_TEXT]);
}

static void flush_display(lv_disp_drv_t *driver, const lv_area_t *area,
                           lv_color_t *pixels) {
    size_t count = (size_t)lv_area_get_width(area) * lv_area_get_height(area);
#if !LV_COLOR_16_SWAP
    for (size_t i = 0; i < count; ++i) {
        uint16_t value = pixels[i].full;
        pixels[i].full = (value >> 8) | (value << 8);
    }
#endif
    esp_err_t err = st7796_draw_region(area->x1, area->y1,
        lv_area_get_width(area), lv_area_get_height(area),
        (const uint8_t *)pixels, lv_area_get_width(area) * 2);
    if (err != ESP_OK) {
        s_transfer_error = err;
        if (s_video) atomic_store(&s_video_error, err);
        ESP_LOGE(TAG, "display transfer: %s", esp_err_to_name(err));
    }
#if !LV_COLOR_16_SWAP
    for (size_t i = 0; i < count; ++i) {
        uint16_t value = pixels[i].full;
        pixels[i].full = (value >> 8) | (value << 8);
    }
#endif
    (void)count;
    /* The panel driver returns only once DMA has completed. */
    lv_disp_flush_ready(driver);
}

void ui_task_handler(void) {
    if (!s_display || !s_lock) return;
    xSemaphoreTakeRecursive(s_lock, portMAX_DELAY);
    int64_t now = esp_timer_get_time();
    uint32_t elapsed = (uint32_t)((now - s_tick_us) / 1000);
    if (elapsed) {
        lv_tick_inc(elapsed);
        s_tick_us += (int64_t)elapsed * 1000;
    }
    if (!s_frame_open) {
        if (s_transfer_error != ESP_OK) {
            lv_obj_invalidate(s_root);
            s_transfer_error = ESP_OK;
        }
        bool video_updated = update_video();
        lv_timer_handler();
        if (video_updated) lv_refr_now(s_display);
    }
    xSemaphoreGiveRecursive(s_lock);
}

static void display_task(void *argument) {
    (void)argument;
    for (;;) {
        ui_task_handler();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

esp_err_t ui_init(void) {
    if (s_display) return ESP_OK;
    esp_err_t err = ui_font_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "font asset /fonts/minibox_sans_24.bin: %s", esp_err_to_name(err));
        return err;
    }
    err = st7796_init();
    if (err != ESP_OK) return err;
    s_lock = xSemaphoreCreateRecursiveMutex();
    s_video_lock = xSemaphoreCreateMutex();
    s_nodes = heap_caps_calloc(MAX_NODES, sizeof(*s_nodes),
                              MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    s_pixels = heap_caps_malloc(PANEL_W * DRAW_ROWS * sizeof(lv_color_t),
                               MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (!s_lock || !s_video_lock || !s_nodes || !s_pixels) {
        err = ESP_ERR_NO_MEM;
        goto failed;
    }
    lv_init();
    lv_disp_draw_buf_init(&s_draw_buffer, s_pixels, NULL, PANEL_W * DRAW_ROWS);
    static lv_disp_drv_t driver;
    lv_disp_drv_init(&driver);
    driver.hor_res = PANEL_W;
    driver.ver_res = PANEL_H;
    driver.draw_buf = &s_draw_buffer;
    driver.flush_cb = flush_display;
    s_display = lv_disp_drv_register(&driver);
    if (!s_display) { err = ESP_ERR_NO_MEM; goto failed; }
    s_root = lv_disp_get_scr_act(s_display);
    lv_obj_remove_style_all(s_root);
    lv_obj_clear_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(s_root, color(UI_COLOR_BG), 0);
    s_tick_us = esp_timer_get_time();
    err = st7796_backlight_on();
    if (err != ESP_OK) goto failed;
    if (xTaskCreatePinnedToCore(display_task, "lvgl", 6144, NULL, 2, NULL, 0) != pdPASS) {
        err = ESP_ERR_NO_MEM;
        goto failed;
    }
    return ESP_OK;
failed:
    if (s_display) { lv_disp_remove(s_display); s_display = NULL; }
    heap_caps_free(s_pixels);
    heap_caps_free(s_nodes);
    s_pixels = NULL;
    s_nodes = NULL;
    if (s_lock) { vSemaphoreDelete(s_lock); s_lock = NULL; }
    if (s_video_lock) { vSemaphoreDelete(s_video_lock); s_video_lock = NULL; }
    return err;
}

static void triangle_draw(lv_event_t *event) {
    node_t *node = lv_event_get_user_data(event);
    lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(event);
    lv_area_t coords;
    lv_obj_get_coords(node->obj, &coords);
    lv_point_t points[3];
    for (int i = 0; i < 3; ++i) {
        points[i].x = coords.x1 + node->points[i].x;
        points[i].y = coords.y1 + node->points[i].y;
    }
    lv_draw_rect_dsc_t descriptor;
    lv_draw_rect_dsc_init(&descriptor);
    descriptor.bg_color = color(node->command.color);
    descriptor.bg_opa = LV_OPA_COVER;
    lv_draw_polygon(ctx, &descriptor, points, 3);
}

static node_t *node_for(const command_t *command, const char *text, bool *changed) {
    if (!s_frame_open || s_used >= MAX_NODES) {
        if (s_frame_open) s_overflow = true;
        return NULL;
    }
    node_t *node = &s_nodes[s_used++];
    if (node->obj && node->command.kind != command->kind) {
        lv_obj_del(node->obj);
        heap_caps_free(node->image_data);
        memset(node, 0, sizeof(*node));
    }
    if (!node->obj) {
        if (command->kind == NODE_LABEL) node->obj = lv_label_create(s_root);
        else if (command->kind == NODE_LINE) node->obj = lv_line_create(s_root);
        else if (command->kind == NODE_IMAGE) node->obj = lv_img_create(s_root);
        else node->obj = lv_obj_create(s_root);
        if (!node->obj) { s_overflow = true; return NULL; }
        lv_obj_move_to_index(node->obj, (int32_t)s_used - 1);
        lv_obj_remove_style_all(node->obj);
        lv_obj_clear_flag(node->obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
        if (command->kind == NODE_TRIANGLE) {
            lv_obj_add_event_cb(node->obj, triangle_draw, LV_EVENT_DRAW_MAIN, node);
        }
    }
    *changed = !node->valid || memcmp(&node->command, command, sizeof(*command)) != 0;
    if (text && strcmp(lv_label_get_text(node->obj), text) != 0) *changed = true;
    if (lv_obj_has_flag(node->obj, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_clear_flag(node->obj, LV_OBJ_FLAG_HIDDEN);
    }
    if (*changed) {
        node->command = *command;
        node->valid = true;
        lv_obj_set_pos(node->obj, command->x, command->y);
    }
    return node;
}

void ui_begin(ui_color_t background) {
    if (!s_display) return;
    xSemaphoreTakeRecursive(s_lock, portMAX_DELAY);
    s_frame_open = true;
    s_used = 0;
    s_overflow = false;
    if (s_background != (int)background) {
        lv_obj_set_style_bg_color(s_root, color(background), 0);
        s_background = background;
    }
}

static void rect(int x, int y, int w, int h, int radius, int border, ui_color_t value) {
    if (w <= 0 || h <= 0) return;
    command_t command = {
        .kind = NODE_RECT, .x = px(x), .y = py(y),
        .w = px(x + w) - px(x), .h = py(y + h) - py(y),
        .radius = py(radius), .border = border > 0 ? py(border) : 0, .color = value,
    };
    bool changed;
    node_t *node = node_for(&command, NULL, &changed);
    if (!node || !changed) return;
    lv_obj_set_size(node->obj, command.w, command.h);
    lv_obj_set_style_radius(node->obj, command.radius, 0);
    lv_obj_set_style_bg_opa(node->obj, border ? LV_OPA_TRANSP : LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(node->obj, color(value), 0);
    lv_obj_set_style_border_width(node->obj, command.border, 0);
    lv_obj_set_style_border_color(node->obj, color(value), 0);
    lv_obj_set_style_border_opa(node->obj, LV_OPA_COVER, 0);
}

void ui_fill_rect(int x, int y, int w, int h, ui_color_t value) {
    rect(x, y, w, h, 0, 0, value);
}
void ui_set_pixel(int x, int y, ui_color_t value) { rect(x, y, 1, 1, 0, 0, value); }
void ui_draw_rect(int x, int y, int w, int h, int thickness, ui_color_t value) {
    rect(x, y, w, h, 5, thickness > 0 ? thickness : 1, value);
}
void ui_fill_round_rect(int x, int y, int w, int h, int radius, ui_color_t value) {
    rect(x, y, w, h, radius, 0, value);
}
void ui_draw_round_rect(int x, int y, int w, int h, int radius,
                        int thickness, ui_color_t value) {
    rect(x, y, w, h, radius, thickness > 0 ? thickness : 1, value);
}
void ui_fill_circle(int x, int y, int radius, ui_color_t value) {
    rect(x - radius, y - radius, radius * 2 + 1, radius * 2 + 1, radius + 1, 0, value);
}

void ui_draw_line(int x0, int y0, int x1, int y1, ui_color_t value) {
    int left = x0 < x1 ? x0 : x1, top = y0 < y1 ? y0 : y1;
    command_t command = {
        .kind = NODE_LINE, .x = px(left), .y = py(top), .color = value,
        .points = {{px(x0) - px(left), py(y0) - py(top)},
                   {px(x1) - px(left), py(y1) - py(top)}},
    };
    bool changed;
    node_t *node = node_for(&command, NULL, &changed);
    if (!node || !changed) return;
    memcpy(node->points, command.points, sizeof(node->points));
    lv_line_set_points(node->obj, node->points, 2);
    lv_obj_set_style_line_color(node->obj, color(value), 0);
    lv_obj_set_style_line_width(node->obj, 2, 0);
    lv_obj_set_style_line_rounded(node->obj, true, 0);
}

void ui_fill_triangle(int x0, int y0, int x1, int y1,
                      int x2, int y2, ui_color_t value) {
    int left = LV_MIN(x0, LV_MIN(x1, x2)), top = LV_MIN(y0, LV_MIN(y1, y2));
    int right = LV_MAX(x0, LV_MAX(x1, x2)), bottom = LV_MAX(y0, LV_MAX(y1, y2));
    command_t command = {
        .kind = NODE_TRIANGLE, .x = px(left), .y = py(top), .color = value,
        .w = px(right) - px(left) + 1, .h = py(bottom) - py(top) + 1,
        .points = {{px(x0) - px(left), py(y0) - py(top)},
                   {px(x1) - px(left), py(y1) - py(top)},
                   {px(x2) - px(left), py(y2) - py(top)}},
    };
    bool changed;
    node_t *node = node_for(&command, NULL, &changed);
    if (!node || !changed) return;
    memcpy(node->points, command.points, sizeof(node->points));
    lv_obj_set_size(node->obj, command.w, command.h);
    lv_obj_invalidate(node->obj);
}

static const lv_font_t *text_font(unsigned size) {
    return ui_font_get(size <= 1 ? 16 : size == 2 ? 24 : size == 3 ? 32 : 48);
}

void ui_draw_text_box(int x, int y, int w, int h, const char *text,
                      uint8_t size, ui_color_t value) {
    if (!text) text = "";
    command_t command = {
        .kind = NODE_LABEL, .x = px(x), .y = py(y), .w = px(w),
        .h = h > 0 ? py(h) : LV_SIZE_CONTENT, .size = size, .color = value,
    };
    bool changed;
    node_t *node = node_for(&command, text, &changed);
    if (!node || !changed) return;
    lv_obj_set_style_text_font(node->obj, text_font(size), 0);
    lv_obj_set_style_text_color(node->obj, color(value), 0);
    lv_label_set_long_mode(node->obj, h > 0 ? LV_LABEL_LONG_WRAP : LV_LABEL_LONG_CLIP);
    lv_obj_set_size(node->obj, command.w, command.h);
    if (strcmp(lv_label_get_text(node->obj), text)) lv_label_set_text(node->obj, text);
}

void ui_draw_text(int x, int y, const char *text, uint8_t size, ui_color_t value) {
    ui_draw_text_box(x, y, APP_UI_W - x, 0, text, size, value);
}

int ui_text_width(const char *text, uint8_t size) {
    if (!text) return 0;
    lv_point_t result;
    lv_txt_get_size(&result, text, text_font(size), 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    return (result.x * APP_UI_W + PANEL_W - 1) / PANEL_W;
}

esp_err_t ui_draw_rgb565_bitmap(int x, int y, int w, int h,
                                const uint16_t *pixels, const uint8_t *alpha,
                                ui_color_t background) {
    if (!pixels || w <= 0 || h <= 0) return ESP_ERR_INVALID_ARG;
    command_t command = {
        .kind = NODE_IMAGE, .x = px(x), .y = py(y),
        .w = px(x + w) - px(x), .h = py(y + h) - py(y),
        .source = pixels, .alpha = alpha, .background = background,
        .radius = w, .border = h,
    };
    bool changed;
    node_t *node = node_for(&command, NULL, &changed);
    if (!node) return ESP_ERR_INVALID_STATE;
    if (!changed) return ESP_OK;
    size_t bytes = (size_t)command.w * command.h * sizeof(lv_color_t);
    uint8_t *data = heap_caps_realloc(node->image_data, bytes,
                                    MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!data) { node->valid = false; return ESP_ERR_NO_MEM; }
    node->image_data = data;
    lv_color_t *output = (lv_color_t *)data;
    for (int yy = 0; yy < command.h; ++yy) {
        for (int xx = 0; xx < command.w; ++xx) {
            size_t src = (size_t)(yy * h / command.h) * w + xx * w / command.w;
            uint16_t rgb = pixels[src];
            lv_color_t foreground = lv_color_make(((rgb >> 11) & 31) * 255 / 31,
                ((rgb >> 5) & 63) * 255 / 63, (rgb & 31) * 255 / 31);
            output[yy * command.w + xx] = alpha
                ? lv_color_mix(foreground, color(background), alpha[src])
                : foreground;
        }
    }
    lv_img_cache_invalidate_src(&node->image);
    node->image = (lv_img_dsc_t){
        .header = {.cf = LV_IMG_CF_TRUE_COLOR, .w = command.w, .h = command.h},
        .data_size = bytes, .data = data,
    };
    lv_img_set_src(node->obj, &node->image);
    lv_obj_invalidate(node->obj);
    return ESP_OK;
}

esp_err_t ui_flush(void) {
    if (!s_frame_open) return ESP_ERR_INVALID_STATE;
    for (size_t i = s_used; i < s_previous_used; ++i) {
        if (s_nodes[i].obj) lv_obj_add_flag(s_nodes[i].obj, LV_OBJ_FLAG_HIDDEN);
    }
    s_previous_used = s_used;
    s_frame_open = false;
    esp_err_t err = s_overflow ? ESP_ERR_NO_MEM : s_transfer_error;
    if (s_overflow) ESP_LOGE(TAG, "scene exceeds %u retained nodes", MAX_NODES);
    xSemaphoreGiveRecursive(s_lock);
    return err;
}

void ui_invalidate(void) {
    if (!s_display) return;
    xSemaphoreTakeRecursive(s_lock, portMAX_DELAY);
    lv_obj_invalidate(s_root);
    xSemaphoreGiveRecursive(s_lock);
}

esp_err_t ui_video_begin(int width, int height, int display_width, int display_height) {
    if (!s_display || width <= 0 || height <= 0 ||
        width > PANEL_W || height > PANEL_H || display_width <= 0 ||
        display_height <= 0 || display_width > PANEL_W ||
        display_height > PANEL_H) return ESP_ERR_INVALID_ARG;
    xSemaphoreTakeRecursive(s_lock, portMAX_DELAY);
    if (s_video || s_frame_open) {
        xSemaphoreGiveRecursive(s_lock);
        return ESP_ERR_INVALID_STATE;
    }
    xSemaphoreTake(s_video_lock, portMAX_DELAY);
    size_t bytes = (size_t)display_width * display_height * sizeof(lv_color_t);
    s_video_pixels = heap_caps_calloc(1, bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    for (int i = 0; i < 2; ++i) {
        s_video_indexed[i] = heap_caps_malloc((size_t)width * height,
                                            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
    if (s_video_pixels && s_video_indexed[0] && s_video_indexed[1])
        s_video = lv_img_create(s_root);
    if (!s_video) {
        heap_caps_free(s_video_pixels);
        s_video_pixels = NULL;
        for (int i = 0; i < 2; ++i) {
            heap_caps_free(s_video_indexed[i]);
            s_video_indexed[i] = NULL;
        }
        xSemaphoreGive(s_video_lock);
        xSemaphoreGiveRecursive(s_lock);
        return ESP_ERR_NO_MEM;
    }
    for (size_t i = 0; i < s_previous_used; ++i) {
        if (s_nodes[i].obj) lv_obj_add_flag(s_nodes[i].obj, LV_OBJ_FLAG_HIDDEN);
    }
    s_background = -1;
    lv_obj_set_style_bg_color(s_root, lv_color_black(), 0);
    lv_obj_remove_style_all(s_video);
    s_video_width = width;
    s_video_height = height;
    s_video_front = 0;
    s_video_back = 1;
    s_video_pending = false;
    atomic_store(&s_video_error, ESP_OK);
    /* Pixel-center nearest-neighbor sampling keeps indexed pixel art sharp. */
    for (int x = 0; x < display_width; ++x)
        s_video_x[x] = (2 * x + 1) * width / (2 * display_width);
    for (int y = 0; y < display_height; ++y)
        s_video_y[y] = (2 * y + 1) * height / (2 * display_height);
    s_video_image = (lv_img_dsc_t){
        .header = {.cf = LV_IMG_CF_TRUE_COLOR, .w = display_width, .h = display_height},
        .data = (const uint8_t *)s_video_pixels, .data_size = bytes,
    };
    lv_img_set_src(s_video, &s_video_image);
    lv_obj_center(s_video);
    xSemaphoreGive(s_video_lock);
    xSemaphoreGiveRecursive(s_lock);
    return ESP_OK;
}

bool ui_video_ready(void) {
    if (!s_video_lock || xSemaphoreTake(s_video_lock, 0) != pdTRUE) return false;
    bool ready = s_video && !s_video_pending;
    xSemaphoreGive(s_video_lock);
    return ready;
}

esp_err_t ui_video_present(const uint8_t *pixels, int pitch, const uint16_t *palette) {
    if (!pixels || !palette) return ESP_ERR_INVALID_ARG;
    if (!s_video_lock) return ESP_ERR_INVALID_STATE;
    if (xSemaphoreTake(s_video_lock, 0) != pdTRUE) return ESP_ERR_NOT_FINISHED;
    if (!s_video || pitch < s_video_width) {
        xSemaphoreGive(s_video_lock);
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t err = atomic_load(&s_video_error);
    if (err == ESP_OK) {
        for (int y = 0; y < s_video_height; ++y) {
            memcpy(s_video_indexed[s_video_back] + y * s_video_width,
                   pixels + y * pitch, s_video_width);
        }
        memcpy(s_video_palette[s_video_back], palette, sizeof(s_video_palette[0]));
        s_video_pending = true;
    }
    xSemaphoreGive(s_video_lock);
    return err;
}

/* Only the display task scales pixels. The producer owns the other indexed
 * buffer, so neither conversion nor SPI transfers hold the mailbox lock. */
static bool update_video(void) {
    if (!s_video || xSemaphoreTake(s_video_lock, 0) != pdTRUE) return false;
    if (!s_video_pending) {
        xSemaphoreGive(s_video_lock);
        return false;
    }
    unsigned previous = s_video_front;
    s_video_front = s_video_back;
    s_video_back = previous;
    s_video_pending = false;
    xSemaphoreGive(s_video_lock);

    lv_color_t colors[256];
    for (int i = 0; i < 256; ++i) {
        uint16_t rgb = s_video_palette[s_video_front][i];
#if LV_COLOR_16_SWAP
        rgb = (rgb >> 8) | (rgb << 8);
#endif
        colors[i].full = rgb;
    }
    int width = s_video_image.header.w;
    int height = s_video_image.header.h;
    lv_area_t bounds;
    lv_obj_get_coords(s_video, &bounds);
    bool changed = false;
    for (int top = 0; top < height; top += VIDEO_DIRTY_ROWS) {
        int bottom = LV_MIN(top + VIDEO_DIRTY_ROWS, height);
        int left = width, right = -1;
        int first = bottom, last = top;
        for (int y = top; y < bottom; ++y) {
            const uint8_t *source = s_video_indexed[s_video_front] + s_video_y[y] * s_video_width;
            lv_color_t *output = s_video_pixels + y * width;
            for (int x = 0; x < width; ++x) {
                lv_color_t pixel = colors[source[s_video_x[x]]];
                if (output[x].full != pixel.full) {
                    output[x] = pixel;
                    left = LV_MIN(left, x);
                    right = LV_MAX(right, x);
                    first = LV_MIN(first, y);
                    last = y;
                }
            }
        }
        if (right >= left) {
            lv_area_t dirty = {
                .x1 = bounds.x1 + left, .x2 = bounds.x1 + right,
                .y1 = bounds.y1 + first, .y2 = bounds.y1 + last,
            };
            lv_obj_invalidate_area(s_video, &dirty);
            changed = true;
        }
    }
    if (changed) lv_img_cache_invalidate_src(&s_video_image);
    return changed;
}

void ui_video_end(void) {
    xSemaphoreTakeRecursive(s_lock, portMAX_DELAY);
    xSemaphoreTake(s_video_lock, portMAX_DELAY);
    if (s_video) {
        lv_img_cache_invalidate_src(&s_video_image);
        lv_obj_del(s_video);
        s_video = NULL;
        heap_caps_free(s_video_pixels);
        s_video_pixels = NULL;
        for (int i = 0; i < 2; ++i) {
            heap_caps_free(s_video_indexed[i]);
            s_video_indexed[i] = NULL;
        }
        s_video_pending = false;
        memset(&s_video_image, 0, sizeof(s_video_image));
        lv_obj_invalidate(s_root);
    }
    xSemaphoreGive(s_video_lock);
    xSemaphoreGiveRecursive(s_lock);
}

void ui_show_status(const char *title, const char *a, const char *b,
                    const char *c, const char *d) {
    ui_begin(UI_COLOR_BG);
    ui_fill_round_rect(10, 10, 300, 190, 12, UI_COLOR_PANEL);
    ui_draw_text(22, 22, title, 3, UI_COLOR_FOCUS);
    ui_draw_text(22, 67, a, 2, UI_COLOR_TEXT);
    ui_draw_text(22, 97, b, 2, UI_COLOR_TEXT);
    ui_draw_text(22, 127, c, 2, UI_COLOR_TEXT);
    ui_draw_text(22, 157, d, 2, UI_COLOR_MUTED);
    ESP_ERROR_CHECK_WITHOUT_ABORT(ui_flush());
}

void ui_show_clock(const char *date, const char *time,
                   const char *footer1, const char *footer2) {
    char major[6] = "--:--";
    char seconds[3] = "--";
    if (time && strlen(time) >= 5) memcpy(major, time, 5);
    if (time && strlen(time) >= 8) memcpy(seconds, time + 6, 2);
    ui_begin(UI_COLOR_BG);
    ui_draw_text(20, 20, date, 2, UI_COLOR_MUTED);
    ui_fill_round_rect(12, 58, 296, 94, 14, UI_COLOR_PANEL);
    ui_draw_text(26, 76, major, 4, UI_COLOR_TEXT);
    ui_draw_text(244, 99, seconds, 2, UI_COLOR_CYAN);
    ui_draw_text(20, 174, footer1, 2, UI_COLOR_CYAN);
    ui_draw_text(20, 207, footer2, 1, UI_COLOR_MUTED);
    ESP_ERROR_CHECK_WITHOUT_ABORT(ui_flush());
}

void ui_show_face(const char *title, const char *line1,
                  const char *line2, const char *footer) {
    ui_begin(UI_COLOR_BG);
    ui_draw_text(20, 16, title, 2, UI_COLOR_CYAN);
    ui_fill_round_rect(20, 54, 280, 118, 16, UI_COLOR_PANEL);
    ui_draw_text(36, 73, line1, 3, UI_COLOR_TEXT);
    ui_draw_text(36, 116, line2, 3, UI_COLOR_TEXT);
    ui_draw_text(20, 194, footer, 1, UI_COLOR_MUTED);
    ESP_ERROR_CHECK_WITHOUT_ABORT(ui_flush());
}
