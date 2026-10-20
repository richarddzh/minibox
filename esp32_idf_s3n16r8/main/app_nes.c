#include "app_nes.h"

#include <dirent.h>
#include <errno.h>
#include <inttypes.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include "app_config.h"
#include "audio_io.h"
#include "buttons.h"
#include "chime_player.h"
#include "nes_engine.h"
#include "radio_player.h"
#include "ui_screen.h"
#include "voice_chat.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#define ROM_LIMIT 32
#define ROM_NAME_BYTES 25
#define ROM_ROWS 5
#define EXIT_HOLD_MS 1000
#define VIDEO_FRAME_INTERVAL 2
#define VIDEO_DISPLAY_WIDTH 426
#define VIDEO_DISPLAY_HEIGHT 320
#define AUDIO_FRAME_BYTES (NES_FRAME_SAMPLES * sizeof(int16_t))
#define AUDIO_QUEUE_BYTES (3 * AUDIO_FRAME_BYTES)
#define AUDIO_DRAW_MIN_BYTES (2 * AUDIO_FRAME_BYTES)
#define AUDIO_PREFILL_FRAMES 4

static const char *TAG = "nes";
static char s_roms[ROM_LIMIT][ROM_NAME_BYTES];
static int s_count, s_selected;
static char s_path[64];
static char s_message[128];
static char s_worker_error[128];
static bool s_busy, s_wait_release;
static int64_t s_exit_pressed_ms;
static atomic_bool s_stop;
static SemaphoreHandle_t s_done;

bool app_nes_busy(void) { return s_busy; }
bool app_nes_captures_input(void) { return s_busy || s_wait_release; }

static void report(const char *message) {
    snprintf(s_message, sizeof(s_message), "%s", message);
    ESP_LOGW(TAG, "%s", message);
}

static int compare_names(const void *a, const void *b) {
    return strcasecmp(a, b);
}

static void scan_roms(void) {
    s_count = 0;
    s_selected = 0;
    s_message[0] = 0;
    DIR *directory = opendir(APP_SPIFFS_BASE_PATH);
    if (!directory) {
        report("Cannot open /spiffs");
        return;
    }
    struct dirent *entry;
    int skipped = 0;
    for (;;) {
        errno = 0;
        entry = readdir(directory);
        if (!entry) {
            if (errno) report("Cannot read ROM directory");
            break;
        }
        const char *name = entry->d_name;
        const char *extension = strrchr(name, '.');
        if (!extension || strcasecmp(extension, ".nes") != 0) continue;
        if (strlen(name) >= ROM_NAME_BYTES || strchr(name, '/') || s_count == ROM_LIMIT) {
            skipped++;
            continue;
        }
        char path[64];
        struct stat st;
        snprintf(path, sizeof(path), "%s/%.*s", APP_SPIFFS_BASE_PATH, ROM_NAME_BYTES - 1, name);
        if (stat(path, &st) != 0) {
            report("Cannot inspect ROM file");
            continue;
        }
        if (!S_ISREG(st.st_mode)) continue;
        strcpy(s_roms[s_count++], name);
    }
    closedir(directory);
    qsort(s_roms, s_count, sizeof(s_roms[0]), compare_names);
    if (skipped) report("Some ROMs omitted: max 32, name <=24 bytes");
}

static void emulation_task(void *argument) {
    (void)argument;
    esp_err_t err = nes_engine_open(s_path);
    bool audio_prepared = false;
    bool audio_started = false;
    if (err != ESP_OK) {
        snprintf(s_worker_error, sizeof(s_worker_error), "%s", nes_engine_error());
        goto finished;
    }
    err = audio_io_prepare_realtime_speaker(NES_AUDIO_RATE, 1);
    if (err != ESP_OK) goto finished;
    audio_prepared = true;
    int16_t samples[NES_FRAME_SAMPLES];
    unsigned frame = 0;
    unsigned submitted = 0, video_busy = 0;
    unsigned audio_skips = 0;
    int64_t max_frame_us = 0, max_present_us = 0;
    unsigned last_frame = 0;
    int64_t last_stats = esp_timer_get_time();
    while (!atomic_load(&s_stop)) {
        /* Let I2S be the clock: a wall-clock sleep can slowly drain the queue. */
        if (audio_started) {
            if (audio_io_phase() != AUDIO_PHASE_PLAY) {
                err = ESP_ERR_INVALID_STATE;
                break;
            }
            if (audio_io_spk_pending() >= AUDIO_QUEUE_BYTES) {
                vTaskDelay(1);
                continue;
            }
        }
        uint8_t pad = buttons_gamepad();
        bool draw = false;
        if (audio_started && frame % VIDEO_FRAME_INTERVAL == 0) {
            if (audio_io_spk_pending() < AUDIO_DRAW_MIN_BYTES) audio_skips++;
            else if (!ui_video_ready()) video_busy++;
            else draw = true;
        }
        int64_t frame_start = esp_timer_get_time();
        nes_engine_frame(pad, draw, samples);
        int64_t frame_us = esp_timer_get_time() - frame_start;
        if (frame_us > max_frame_us) max_frame_us = frame_us;
        for (int i = 0; i < NES_FRAME_SAMPLES; ++i) samples[i] /= 2;
        if (audio_started && audio_io_phase() != AUDIO_PHASE_PLAY) {
            err = ESP_ERR_INVALID_STATE;
            break;
        }
        if (audio_io_write_speaker((const uint8_t *)samples, sizeof(samples), 100) != sizeof(samples)) {
            err = ESP_ERR_TIMEOUT;
            break;
        }
        /* A short prefill absorbs scheduling jitter without a multi-second ring backlog. */
        if (!audio_started && frame + 1 == AUDIO_PREFILL_FRAMES) {
            err = audio_io_start_playback(1500);
            if (err != ESP_OK) break;
            audio_started = true;
        }
        if (draw) {
            int pitch;
            const uint8_t *pixels = nes_engine_pixels(&pitch);
            int64_t present_start = esp_timer_get_time();
            err = ui_video_present(pixels, pitch, nes_engine_palette());
            int64_t present_us = esp_timer_get_time() - present_start;
            if (present_us > max_present_us) max_present_us = present_us;
            if (err == ESP_ERR_NOT_FINISHED) {
                video_busy++;
                err = ESP_OK;
            } else if (err == ESP_OK) {
                submitted++;
            }
            if (err != ESP_OK) break;
        }
        frame++;
        int64_t now = esp_timer_get_time();
        if (now - last_stats >= 5000000) {
            ESP_LOGI(TAG, "emulation=%u fps, submitted=%u, video_busy=%u, audio_skips=%u, "
                     "max_frame_us=%" PRId64 ", max_present_us=%" PRId64 ", audio_underrun=%" PRIu32,
                     (unsigned)((frame - last_frame) * 1000000ULL / (now - last_stats)),
                     submitted, video_busy, audio_skips, max_frame_us, max_present_us,
                     audio_io_spk_underrun());
            last_stats = now;
            last_frame = frame;
            submitted = video_busy = 0;
            audio_skips = 0;
            max_frame_us = max_present_us = 0;
        }
        /* Let idle service the watchdog even when a ROM cannot keep up. */
        vTaskDelay(1);
    }
finished:
    if (audio_prepared) {
        esp_err_t stop_err = audio_io_stop_playback(2000);
        if (stop_err != ESP_OK) {
            audio_io_abort(2000);
            if (err == ESP_OK) err = stop_err;
        }
    }
    esp_err_t close_err = nes_engine_close();
    if (close_err != ESP_OK) {
        snprintf(s_worker_error, sizeof(s_worker_error), "%s", nes_engine_error());
        err = close_err;
    }
    if (err != ESP_OK && !s_worker_error[0]) {
        snprintf(s_worker_error, sizeof(s_worker_error), "NES audio/video: %s", esp_err_to_name(err));
    }
    if (err != ESP_OK) ESP_LOGE(TAG, "%s", s_worker_error);
    xSemaphoreGive(s_done);
    vTaskDelete(NULL);
}

static void start_game(void) {
    if (!s_count || s_busy) return;
    if (voice_chat_busy() || chime_player_busy() || radio_player_busy() ||
        audio_io_phase() != AUDIO_PHASE_IDLE) {
        report("Audio busy: stop radio/voice before NES");
        return;
    }
    if (!s_done) s_done = xSemaphoreCreateBinary();
    if (!s_done) { report("Not enough memory for NES task"); return; }
    esp_err_t err = ui_video_begin(NES_VIDEO_WIDTH, NES_VIDEO_HEIGHT,
                                  VIDEO_DISPLAY_WIDTH, VIDEO_DISPLAY_HEIGHT);
    if (err != ESP_OK) { report(esp_err_to_name(err)); return; }
    snprintf(s_path, sizeof(s_path), "%s/%s", APP_SPIFFS_BASE_PATH, s_roms[s_selected]);
    s_worker_error[0] = 0;
    s_exit_pressed_ms = 0;
    atomic_store(&s_stop, false);
    s_busy = true;
    if (xTaskCreatePinnedToCore(emulation_task, "nes", 8192, NULL, 3, NULL, 1) != pdPASS) {
        s_busy = false;
        ui_video_end();
        report("Cannot create NES task");
    }
}

static gui_action_t tick(int64_t now_ms) {
    if (!s_busy) {
        if (s_wait_release && buttons_gamepad() == 0) s_wait_release = false;
        return GUI_ACTION_NONE;
    }
    uint8_t chord = BUTTON_PAD_SELECT | BUTTON_PAD_START;
    if ((buttons_gamepad() & chord) == chord) {
        if (!s_exit_pressed_ms) s_exit_pressed_ms = now_ms;
        if (now_ms - s_exit_pressed_ms >= EXIT_HOLD_MS) atomic_store(&s_stop, true);
    } else {
        s_exit_pressed_ms = 0;
    }
    if (xSemaphoreTake(s_done, 0) != pdTRUE) return GUI_ACTION_NONE;
    ui_video_end();
    s_busy = false;
    s_wait_release = true;
    if (s_worker_error[0]) report(s_worker_error);
    else snprintf(s_message, sizeof(s_message), "Game closed. Battery RAM saved if present.");
    return GUI_ACTION_REDRAW;
}

static bool handle_input(gui_input_t input, gui_action_t *action) {
    *action = GUI_ACTION_REDRAW;
    if (app_nes_captures_input()) return true;
    if (input == GUI_INPUT_BACK) return false;
    if (input == GUI_INPUT_UP && s_count) s_selected = (s_selected + s_count - 1) % s_count;
    else if (input == GUI_INPUT_DOWN && s_count) s_selected = (s_selected + 1) % s_count;
    else if (input == GUI_INPUT_ACTIVATE) start_game();
    else if (input == GUI_INPUT_AUX_ACTIVATE) scan_roms();
    return true;
}

static void render(const gui_model_t *model, int focused_item) {
    (void)model;
    (void)focused_item;
    ui_draw_text(10, 5, "NES", 2, UI_COLOR_FOCUS);
    ui_draw_text(62, 10, "A 运行  Y 刷新  X 返回", 1, UI_COLOR_TEXT);
    if (!s_count) {
        ui_draw_text(10, 42, "未找到 .nes 游戏", 2, UI_COLOR_WARNING);
        ui_draw_text(10, 72, "放入 config_data 后构建并写入 storage", 1, UI_COLOR_MUTED);
    } else {
        int first = s_selected / ROM_ROWS * ROM_ROWS;
        for (int row = 0; row < ROM_ROWS && first + row < s_count; ++row) {
            bool selected = first + row == s_selected;
            int y = 32 + row * 23;
            ui_fill_rect(8, y, 304, 22, selected ? UI_COLOR_PRIMARY_DARK : UI_COLOR_PANEL);
            ui_draw_text(12, y + 4, s_roms[first + row], 1,
                         selected ? UI_COLOR_FOCUS : UI_COLOR_TEXT);
        }
        char count[24];
        snprintf(count, sizeof(count), "%d / %d", s_selected + 1, s_count);
        ui_draw_text(260, 150, count, 1, UI_COLOR_MUTED);
    }
    ui_draw_text(10, 151, "游戏: X Select / Y Start", 1, UI_COLOR_CYAN);
    ui_draw_text(10, 166, "同时长按 X+Y 1秒退出", 1, UI_COLOR_MUTED);
    ui_draw_text_box(10, 182, 302, 25, s_message, 1, UI_COLOR_WARNING);
}

static const gui_app_t s_app = {
    .id = "nes", .label = "NES", .icon = GUI_ICON_NES,
    .enter = scan_roms, .tick = tick, .render = render, .handle_input = handle_input,
};

const gui_app_t *app_nes_descriptor(void) { return &s_app; }
