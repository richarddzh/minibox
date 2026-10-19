// Top-level orchestrator. Mirrors the Arduino AlarmApp::loop() state
// machine but cooperates with audio_io's Core-1 task.
#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_random.h>
#include <esp_timer.h>
#include <esp_system.h>
#include <esp_chip_info.h>
#include <nvs_flash.h>

#include "app_config.h"
#include "app_secret_store.h"
#include "buttons.h"
#include "onboard_led.h"
#include "ui_screen.h"
#include "gui_shell.h"
#include "app_chat_alarm.h"
#include "app_settings.h"
#include "app_radio.h"
#include "app_tetris.h"
#include "app_snake.h"
#include "app_nes.h"
#include "wifi_creds.h"
#include "wifi_provision.h"
#include "wifi_time.h"
#include "rtc_pcf8563.h"
#include "audio_io.h"
#include "voice_chat.h"
#include "chime_player.h"
#include "radio_player.h"
#include "mem_log.h"

static const char *TAG = "app";

// Voice agent cat face shown during/right after a voice interaction.
typedef struct {
    const char *title;
    const char *line1;
    const char *line2;
} voice_face_t;

// Linger time for terminal cat faces (DONE / ERROR) before falling back
// to the clock face.
#define VOICE_FACE_LINGER_MS 2000
#define RTC_POLL_MS          250
#define RANDOM_CHIME_SLOT_CAPACITY 5
#define RANDOM_CHIME_COUNT         0
#define RANDOM_CHIME_DIVISOR \
    (RANDOM_CHIME_COUNT > 0 ? RANDOM_CHIME_COUNT : 1)
#define HOUR_CHIME_WINDOW_SEC (5 * 60)
#define RANDOM_CHIME_START_SEC (5 * 60)
#define RANDOM_CHIME_END_SEC   (55 * 60)
#define RANDOM_CHIME_MIN_GAP_SEC (8 * 60)

typedef struct {
    int  hour_key;
    int  slots[RANDOM_CHIME_SLOT_CAPACITY];
    bool fired[RANDOM_CHIME_SLOT_CAPACITY];
    bool hourly_pending;
} chime_schedule_t;

_Static_assert(RANDOM_CHIME_COUNT <= RANDOM_CHIME_SLOT_CAPACITY,
               "random chime count exceeds slot capacity");

static wifi_time_result_t s_last_time;
static char  s_status_msg[48] = "boot";
static char  s_btn_msg[24]    = "Button idle";
static int64_t s_last_refresh_ms;
static int64_t s_last_wifi_retry_ms;
static int64_t s_last_mem_log_ms;
static int64_t s_last_rtc_poll_ms;
static int64_t s_button_press_ms;
static bool    s_button_long_handled;
static bool    s_setup_mode;
static bool    s_quiet_mode;
static volatile bool s_voice_status_dirty;
static volatile bool s_chime_status_dirty;
static int64_t s_voice_face_hide_at_ms;
static bool    s_face_was_visible;
static chime_schedule_t s_chime_sched = { .hour_key = -1 };

static inline int64_t now_ms(void) { return esp_timer_get_time() / 1000; }

static void render_home(void);
static void set_setup_ap(bool on);

static bool audio_activity_busy(void) {
    return app_nes_busy() || voice_chat_busy() || chime_player_busy() || radio_player_busy() ||
           audio_io_phase() != AUDIO_PHASE_IDLE;
}

// --- Voice cat face -----------------------------------------------------
static voice_face_t voice_face_for(voice_chat_status_t st) {
    switch (st) {
    case VC_CONNECTING:
    case VC_RECORDING:
    case VC_UPLOADING:
        return (voice_face_t){"语音: 录音",  "  ^ ^", "( o_o)?"};
    case VC_PLAYING:
    case VC_DRAINING:
        return (voice_face_t){"语音: 播放", "  ^ ^", "( >O<)<"};
    case VC_WAITING_REPLY:
        return (voice_face_t){"语音: 等待", "  ^ ^", "( @_@ )"};
    case VC_ERROR:
        return (voice_face_t){"语音: 错误",  "  ^ ^", "( x_x )"};
    case VC_DONE:
        return (voice_face_t){"语音: 完成", "  ^ ^", "( ._. )"};
    case VC_IDLE:
    default:
        return (voice_face_t){"闲聊闹钟", "  ^ ^", "( ._. )"};
    }
}

static bool voice_face_visible(void) {
    if (voice_chat_busy()) return true;
    voice_chat_status_t st = voice_chat_status();
    if ((st == VC_DONE || st == VC_ERROR) &&
        now_ms() < s_voice_face_hide_at_ms) return true;
    return false;
}

static void render_home(void) {
    if (app_nes_busy()) return;
    voice_face_t f = voice_face_for(voice_chat_status());
    gui_model_t model = {
        .date = s_last_time.ok ? s_last_time.date : "",
        .time = s_last_time.ok ? s_last_time.time : "",
        .status = s_status_msg,
        .voice_title = f.title,
        .voice_line1 = f.line1,
        .voice_line2 = f.line2,
        .time_valid = s_last_time.ok,
        .wifi_configured = wifi_time_has_credentials(),
        .wifi_connected = wifi_time_is_connected(),
        .setup_mode = s_setup_mode || wifi_provision_active(),
        .quiet_mode = s_quiet_mode,
        .audio_busy = audio_activity_busy(),
        .voice_visible = voice_face_visible(),
    };
    gui_shell_render(&model);
}

static void voice_status_changed_cb(void *ctx) {
    (void)ctx;
    s_voice_status_dirty = true;
    voice_chat_status_t st = voice_chat_status();
    if (st == VC_DONE || st == VC_ERROR) {
        s_voice_face_hide_at_ms = now_ms() + VOICE_FACE_LINGER_MS;
    } else if (st != VC_IDLE) {
        s_voice_face_hide_at_ms = now_ms() + 60000;
    }
}

static void chime_status_changed_cb(void *ctx) {
    (void)ctx;
    s_chime_status_dirty = true;
}

static void chime_text_received_cb(const char *text, void *ctx) {
    (void)ctx;
    app_chat_alarm_append_assistant(text);
}

static void copy_time_from_rtc(const rtc_pcf8563_time_t *rtc) {
    wifi_time_result_t prev = s_last_time;
    memset(&s_last_time, 0, sizeof(s_last_time));
    s_last_time.ok = rtc->ok;
    if (rtc->ok) {
        snprintf(s_last_time.date, sizeof(s_last_time.date), "%s", rtc->date);
        snprintf(s_last_time.time, sizeof(s_last_time.time), "%s", rtc->time);
    } else {
        snprintf(s_last_time.err, sizeof(s_last_time.err), "%s",
                 rtc->err[0] ? rtc->err : "RTC not set");
    }
    if (prev.ok != s_last_time.ok ||
        strcmp(prev.date, s_last_time.date) != 0 ||
        strcmp(prev.time, s_last_time.time) != 0 ||
        strcmp(prev.err, s_last_time.err) != 0) {
        render_home();
    }
}

static bool parse_local_time(int *year, int *month, int *day,
                             int *hour, int *minute, int *second) {
    if (!s_last_time.ok) return false;
    if (sscanf(s_last_time.date, "%d-%d-%d", year, month, day) != 3) return false;
    if (sscanf(s_last_time.time, "%d:%d:%d", hour, minute, second) != 3) return false;
    return true;
}

static int make_hour_key(int year, int month, int day, int hour) {
    return year * 1000000 + month * 10000 + day * 100 + hour;
}

static bool random_chime_active_hour(int hour) {
    return hour >= 7 && hour < 23;
}

static bool hourly_chime_active_hour(int hour) {
    return hour >= 7 && hour <= 23;
}

static void plan_random_slots(int current_sec_of_hour, int hour) {
    const int slot_count = RANDOM_CHIME_COUNT;
    for (int i = 0; i < RANDOM_CHIME_SLOT_CAPACITY; ++i) {
        s_chime_sched.slots[i] = -1;
        s_chime_sched.fired[i] = true;
    }
    if (slot_count == 0 || !random_chime_active_hour(hour)) return;

    int count = 0;
    int attempts = 0;
    while (count < slot_count && attempts++ < 200) {
        int span = RANDOM_CHIME_END_SEC - RANDOM_CHIME_START_SEC;
        int candidate = RANDOM_CHIME_START_SEC + (int)(esp_random() % span);
        bool too_close = false;
        for (int i = 0; i < count; ++i) {
            int delta = candidate - s_chime_sched.slots[i];
            if (delta < 0) delta = -delta;
            if (delta < RANDOM_CHIME_MIN_GAP_SEC) {
                too_close = true;
                break;
            }
        }
        if (too_close) continue;
        s_chime_sched.slots[count++] = candidate;
    }
    while (count < slot_count) {
        s_chime_sched.slots[count] =
            RANDOM_CHIME_START_SEC +
            ((RANDOM_CHIME_END_SEC - RANDOM_CHIME_START_SEC) * count) /
            RANDOM_CHIME_DIVISOR +
            (int)(esp_random() % 90);
        count++;
    }
    for (int i = 0; i < slot_count; ++i) {
        for (int j = i + 1; j < slot_count; ++j) {
            if (s_chime_sched.slots[j] < s_chime_sched.slots[i]) {
                int t = s_chime_sched.slots[i];
                s_chime_sched.slots[i] = s_chime_sched.slots[j];
                s_chime_sched.slots[j] = t;
            }
        }
        s_chime_sched.fired[i] = s_chime_sched.slots[i] <= current_sec_of_hour;
    }
}

static void refresh_chime_schedule(void) {
    int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
    if (!parse_local_time(&year, &month, &day, &hour, &minute, &second)) return;

    int key = make_hour_key(year, month, day, hour);
    if (key == s_chime_sched.hour_key) return;

    s_chime_sched.hour_key = key;
    s_chime_sched.hourly_pending =
        hourly_chime_active_hour(hour) &&
        (minute * 60 + second) < HOUR_CHIME_WINDOW_SEC;
    plan_random_slots(minute * 60 + second, hour);
}

static void format_ampm_time(int hour24, int minute, char out[16]) {
    const char *suffix = hour24 < 12 ? "AM" : "PM";
    int hour12 = hour24 % 12;
    if (hour12 == 0) hour12 = 12;
    snprintf(out, 16, "%d:%02d %s", hour12, minute, suffix);
}

static bool ensure_wifi_connected(const char *reason) {
    if (!wifi_time_has_credentials() || s_setup_mode || wifi_provision_active()) return false;
    if (wifi_time_is_connected()) return true;

    snprintf(s_status_msg, sizeof(s_status_msg), "%s wifi...", reason);
    render_home();

    if (wifi_time_connect() != ESP_OK) {
        uint8_t disc_reason = wifi_time_last_disconnect_reason();
        const char *disc_text = wifi_time_last_disconnect_reason_text();
        ESP_LOGW(TAG, "wifi connect failed for %s: %u (%s)",
                 reason, disc_reason, disc_text);
        if (!s_last_time.ok) {
            if (disc_reason) {
                snprintf(s_last_time.err, sizeof(s_last_time.err),
                         "WiFi %s (%u)", disc_text, disc_reason);
            } else {
                snprintf(s_last_time.err, sizeof(s_last_time.err),
                         "WiFi %s", disc_text);
            }
        }
        if (disc_reason) {
            snprintf(s_status_msg, sizeof(s_status_msg),
                     "wifi: %s (%u)", disc_text, disc_reason);
        } else {
            snprintf(s_status_msg, sizeof(s_status_msg),
                     "wifi: %s", disc_text);
        }
        s_last_wifi_retry_ms = now_ms();
        render_home();
        return false;
    }

    snprintf(s_status_msg, sizeof(s_status_msg), "wifi connected");
    s_last_wifi_retry_ms = now_ms();
    render_home();
    return true;
}

static bool sync_time_from_wifi(const char *reason) {
    if (audio_activity_busy()) {
        ESP_LOGI(TAG, "time sync deferred while audio is active");
        return false;
    }
    if (s_setup_mode || wifi_provision_active()) {
        snprintf(s_status_msg, sizeof(s_status_msg), "AP on, sync paused");
        render_home();
        return false;
    }
    if (!wifi_time_has_credentials()) {
        snprintf(s_status_msg, sizeof(s_status_msg), "time needs wifi");
        render_home();
        return false;
    }
    if (!ensure_wifi_connected(reason)) return false;

    snprintf(s_status_msg, sizeof(s_status_msg), "%s time...", reason);
    render_home();

    wifi_time_result_t fetched = wifi_time_fetch();
    s_last_refresh_ms = now_ms();
    if (!fetched.ok) {
        snprintf(s_status_msg, sizeof(s_status_msg), "time sync fail");
        if (!s_last_time.ok) {
            snprintf(s_last_time.err, sizeof(s_last_time.err), "%s",
                     fetched.err[0] ? fetched.err : "time sync fail");
        }
        render_home();
        return false;
    }

    esp_err_t err = rtc_pcf8563_set_from_strings(fetched.date, fetched.time);
    if (err != ESP_OK) {
        s_last_time = fetched;
        snprintf(s_status_msg, sizeof(s_status_msg), "rtc write fail");
        render_home();
        return false;
    }

    rtc_pcf8563_time_t rtc = rtc_pcf8563_read();
    copy_time_from_rtc(&rtc);
    snprintf(s_status_msg, sizeof(s_status_msg), "time synced");
    render_home();
    return true;
}

static void poll_rtc_time(void) {
    if (!rtc_pcf8563_ready()) return;
    if (now_ms() - s_last_rtc_poll_ms < RTC_POLL_MS) return;
    s_last_rtc_poll_ms = now_ms();
    rtc_pcf8563_time_t rtc = rtc_pcf8563_read();
    copy_time_from_rtc(&rtc);
}

static bool trigger_chime(const char *reason, const char *time_arg) {
    if (audio_activity_busy()) {
        return false;
    }
    if (!wifi_time_has_credentials()) {
        snprintf(s_status_msg, sizeof(s_status_msg), "chime needs wifi");
        render_home();
        return false;
    }
    if (!wifi_time_is_connected() &&
        (uint32_t)(now_ms() - s_last_wifi_retry_ms) < APP_WIFI_RETRY_MS) {
        return false;
    }
    if (!ensure_wifi_connected(reason)) return false;
    if (chime_player_start(time_arg) != ESP_OK) {
        snprintf(s_status_msg, sizeof(s_status_msg), "chime busy");
        render_home();
        return false;
    }
    if (time_arg && time_arg[0]) {
        char message[48];
        snprintf(message, sizeof(message), "定时播报 %s", time_arg);
        app_chat_alarm_append_user(message);
    } else if (strcmp(reason, "random chime") == 0) {
        app_chat_alarm_append_user("随机闲聊");
    } else {
        app_chat_alarm_append_user("请和我闲聊一下");
    }
    return true;
}

static void maybe_dispatch_scheduled_chime(void) {
    int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
    if (!parse_local_time(&year, &month, &day, &hour, &minute, &second)) return;
    if (s_setup_mode || wifi_provision_active()) return;
    if (s_quiet_mode) return;

    int sec_of_hour = minute * 60 + second;
    if (s_chime_sched.hourly_pending) {
        if (sec_of_hour >= HOUR_CHIME_WINDOW_SEC) {
            s_chime_sched.hourly_pending = false;
        } else {
        char time_arg[16];
        format_ampm_time(hour, 0, time_arg);
        if (trigger_chime("hour chime", time_arg)) {
            s_chime_sched.hourly_pending = false;
        }
        }
        return;
    }

    for (int i = 0; i < RANDOM_CHIME_COUNT; ++i) {
        if (s_chime_sched.fired[i]) continue;
        if (s_chime_sched.slots[i] > sec_of_hour) break;
        if (trigger_chime("random chime", NULL)) {
            s_chime_sched.fired[i] = true;
        }
        return;
    }
}

// --- Wi-Fi setup AP -----------------------------------------------------
static void set_setup_ap(bool on) {
    if (on == s_setup_mode) return;
    if (on) {
        radio_player_stop(4000);
        wifi_time_disconnect();
        mem_log("ap pre");
        if (wifi_provision_start() == ESP_OK) {
            s_setup_mode = true;
            snprintf(s_status_msg, sizeof(s_status_msg), "AP %s", APP_PROVISION_AP_SSID);
            mem_log("ap on");
        } else {
            s_setup_mode = false;
            snprintf(s_status_msg, sizeof(s_status_msg), "AP start fail");
        }
    } else {
        if (wifi_provision_stop() == ESP_OK) {
            s_setup_mode = false;
            mem_log("ap off");
            snprintf(s_status_msg, sizeof(s_status_msg), "AP off");
            if (wifi_time_has_credentials()) {
                if (ensure_wifi_connected("setup off")) {
                    mem_log("wifi back");
                }
            }
        } else {
            s_setup_mode = true;
            snprintf(s_status_msg, sizeof(s_status_msg), "AP stop fail");
        }
    }
    render_home();
}

// --- Single-button gestures --------------------------------------------
static void toggle_quiet_mode(void) {
    s_quiet_mode = !s_quiet_mode;
    snprintf(s_status_msg, sizeof(s_status_msg),
             "quiet mode %s", s_quiet_mode ? "on" : "off");
    ESP_LOGI(TAG, "%s", s_status_msg);
    render_home();
}

static void dispatch_gui_action(gui_action_t action) {
    switch (action) {
    case GUI_ACTION_REDRAW:
        render_home();
        break;
    case GUI_ACTION_CHIME:
        trigger_chime("app chat", NULL);
        break;
    case GUI_ACTION_TOGGLE_QUIET:
        toggle_quiet_mode();
        break;
    case GUI_ACTION_WIFI_SCAN: {
        if (s_setup_mode || wifi_provision_active()) {
            app_settings_set_wifi_scan(NULL, 0, ESP_ERR_INVALID_STATE);
            break;
        }
        if (audio_activity_busy()) {
            app_settings_set_wifi_status("音频使用中，稍后重试");
            break;
        }
        wifi_scan_ap_t results[WIFI_SCAN_MAX_RESULTS];
        size_t count = 0;
        esp_err_t err = wifi_time_scan(results, WIFI_SCAN_MAX_RESULTS, &count);
        app_settings_set_wifi_scan(results, count, err);
        break;
    }
    case GUI_ACTION_WIFI_CONNECT: {
        wifi_creds_t creds;
        if (!app_settings_take_wifi_credentials(&creds)) break;
        if (audio_activity_busy()) {
            app_settings_set_wifi_status("音频使用中，无法切换");
            break;
        }
        if (s_setup_mode) set_setup_ap(false);
        wifi_time_disconnect();
        wifi_time_set_credentials(&creds);
        snprintf(s_status_msg, sizeof(s_status_msg), "连接 %s", creds.ssid);
        render_home();
        esp_err_t err = wifi_time_connect();
        if (err == ESP_OK) {
            snprintf(s_status_msg, sizeof(s_status_msg), "WiFi connected");
            app_settings_set_wifi_status("连接成功");
            s_last_wifi_retry_ms = now_ms();
        } else {
            snprintf(s_status_msg, sizeof(s_status_msg), "WiFi: %s",
                     wifi_time_last_disconnect_reason_text());
            app_settings_set_wifi_status(wifi_time_last_disconnect_reason_text());
        }
        break;
    }
    case GUI_ACTION_API_SETUP_START:
        set_setup_ap(true);
        break;
    case GUI_ACTION_API_SETUP_STOP:
        set_setup_ap(false);
        break;
    case GUI_ACTION_SYNC_TIME:
        sync_time_from_wifi("settings");
        break;
    case GUI_ACTION_START_VOICE:
        if (voice_chat_busy() || chime_player_busy() || radio_player_busy() ||
            audio_io_phase() != AUDIO_PHASE_IDLE) {
            snprintf(s_status_msg, sizeof(s_status_msg), "audio busy");
            break;
        }
        if (!wifi_time_has_credentials()) {
            snprintf(s_status_msg, sizeof(s_status_msg), "请先设置 WiFi");
            break;
        }
        if (!ensure_wifi_connected("voice")) break;
        if (voice_chat_start() != ESP_OK) {
            snprintf(s_status_msg, sizeof(s_status_msg), "voice: %s",
                     voice_chat_message());
            break;
        }
        app_chat_alarm_set_recording(true, now_ms());
        snprintf(s_status_msg, sizeof(s_status_msg), "voice recording");
        break;
    case GUI_ACTION_STOP_VOICE:
        if (app_chat_alarm_is_recording()) {
            app_chat_alarm_set_recording(false, 0);
            voice_chat_stop_and_process();
        }
        break;
    case GUI_ACTION_RADIO_TOGGLE: {
        size_t station = app_radio_selected_station();
        radio_player_snapshot_t snapshot;
        radio_player_get_snapshot(&snapshot);
        if (radio_player_busy()) {
            if (radio_player_stop(4000) != ESP_OK) {
                snprintf(s_status_msg, sizeof(s_status_msg),
                         "radio stop timeout");
                break;
            }
            if (snapshot.station_index == station) break;
        }
        if (voice_chat_busy() || chime_player_busy() ||
            audio_io_phase() != AUDIO_PHASE_IDLE) {
            snprintf(s_status_msg, sizeof(s_status_msg), "audio busy");
            break;
        }
        if (!wifi_time_has_credentials()) {
            snprintf(s_status_msg, sizeof(s_status_msg), "请先设置 WiFi");
            break;
        }
        if (!ensure_wifi_connected("radio")) break;
        if (radio_player_start(station) != ESP_OK) {
            snprintf(s_status_msg, sizeof(s_status_msg), "radio start fail");
        }
        break;
    }
    case GUI_ACTION_NONE:
    default:
        break;
    }
}

static void handle_input_event(app_input_event_t event) {
    if (app_nes_captures_input()) {
        s_button_press_ms = 0;
        s_button_long_handled = false;
        return;
    }
    switch (event) {
    case APP_INPUT_LEFT:
        dispatch_gui_action(gui_shell_handle_input(GUI_INPUT_LEFT));
        return;
    case APP_INPUT_RIGHT:
        dispatch_gui_action(gui_shell_handle_input(GUI_INPUT_RIGHT));
        return;
    case APP_INPUT_UP:
        dispatch_gui_action(gui_shell_handle_input(GUI_INPUT_UP));
        return;
    case APP_INPUT_DOWN:
        dispatch_gui_action(gui_shell_handle_input(GUI_INPUT_DOWN));
        return;
    case APP_INPUT_JOYSTICK_PRESSED:
        dispatch_gui_action(gui_shell_handle_input(GUI_INPUT_AUX_ACTIVATE));
        return;
    case APP_INPUT_RECORD_PRESSED:
        if (gui_shell_current_app_is("chat")) {
            dispatch_gui_action(GUI_ACTION_START_VOICE);
        }
        return;
    case APP_INPUT_RECORD_RELEASED:
        dispatch_gui_action(GUI_ACTION_STOP_VOICE);
        return;
    case APP_INPUT_BACK:
        if (app_chat_alarm_is_recording()) dispatch_gui_action(GUI_ACTION_STOP_VOICE);
        dispatch_gui_action(gui_shell_handle_input(GUI_INPUT_BACK));
        return;
    case APP_INPUT_MAIN_PRESSED:
        s_button_press_ms = now_ms();
        s_button_long_handled = false;
        return;
    case APP_INPUT_MAIN_RELEASED:
        break;
    }

    if (s_button_press_ms == 0) return;
    bool handled = s_button_long_handled;
    s_button_press_ms = 0;
    s_button_long_handled = false;

    if (handled) {
        dispatch_gui_action(gui_shell_handle_input(GUI_INPUT_RELEASE));
        return;
    }
    dispatch_gui_action(gui_shell_handle_input(GUI_INPUT_ACTIVATE));
}

static void handle_button_long_press(void) {
    if (app_nes_captures_input()) return;
    if (!buttons_pressed() || s_button_press_ms == 0 ||
        s_button_long_handled ||
        (uint32_t)(now_ms() - s_button_press_ms) <
            APP_BUTTON_LONG_PRESS_MS) {
        return;
    }

    gui_action_t action = gui_shell_handle_input(GUI_INPUT_LONG_PRESS);
    if (action == GUI_ACTION_NONE) return;
    s_button_long_handled = true;
    dispatch_gui_action(action);
    render_home();
}

// --- Wi-Fi setup form consumption ---------------------------------------
static void poll_wifi_provision(void) {
    if (!wifi_provision_active()) return;

    const char *st = wifi_provision_take_status();
    if (st) {
        snprintf(s_status_msg, sizeof(s_status_msg), "%s", st);
        render_home();
    }
}

// --- Boot --------------------------------------------------------------
static void initialise(void) {
    ESP_ERROR_CHECK(audio_io_quiet_speaker_pins());
    esp_err_t e = nvs_flash_init();
    if (e == ESP_ERR_NVS_NO_FREE_PAGES || e == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        e = nvs_flash_init();
    }
    ESP_ERROR_CHECK(e);
    ESP_ERROR_CHECK(onboard_led_off());
    ESP_ERROR_CHECK(ui_init());
    ui_show_status("闲聊闹钟", "设备开启", "等待", "", "");
    ESP_ERROR_CHECK(wifi_creds_init());

    wifi_creds_t wc;
    int rc = wifi_creds_load(&wc);
    if (rc == 0) {
        wifi_time_set_credentials(&wc);
        snprintf(s_status_msg, sizeof(s_status_msg), "WiFi file %s", wc.ssid);
    } else {
        snprintf(s_status_msg, sizeof(s_status_msg), "WiFi not configured");
    }
    app_secrets_t secrets;
    if (app_secrets_load(&secrets) == 0) {
        app_secrets_set_current(&secrets);
    }

    buttons_init();
    ESP_ERROR_CHECK(wifi_time_init());
    ESP_ERROR_CHECK(wifi_provision_init());
    esp_err_t rtc_error = rtc_pcf8563_init();
    if (rtc_error != ESP_OK) {
        ESP_LOGE(TAG, "PCF8563 unavailable: %s", esp_err_to_name(rtc_error));
        snprintf(s_status_msg, sizeof(s_status_msg), "PCF8563 unavailable");
    }
    ESP_ERROR_CHECK(voice_chat_init());
    ESP_ERROR_CHECK(chime_player_init());
    ESP_ERROR_CHECK(radio_player_init());
    voice_chat_set_status_cb(voice_status_changed_cb, NULL);
    chime_player_set_status_cb(chime_status_changed_cb, NULL);
    chime_player_set_text_cb(chime_text_received_cb, NULL);

    const gui_app_t *apps[] = {
        app_chat_alarm_descriptor(),
        app_radio_descriptor(),
        app_settings_descriptor(),
        app_tetris_descriptor(),
        app_snake_descriptor(),
        app_nes_descriptor(),
    };
    gui_shell_init(apps, sizeof(apps) / sizeof(apps[0]), 0);

    poll_rtc_time();
    render_home();

    if (!wifi_time_has_credentials()) {
        snprintf(s_status_msg, sizeof(s_status_msg), "请在设置中配置 WiFi");
        render_home();
    } else if (ensure_wifi_connected("boot")) {
        sync_time_from_wifi("boot");
    }
    s_last_refresh_ms = now_ms();
    s_last_wifi_retry_ms = now_ms();
    s_last_mem_log_ms = now_ms();
    mem_log("boot done");
}

void app_main(void) {
    ESP_LOGI(TAG, "Minibox LVGL apps starting");
    initialise();

    for (;;) {
        ui_task_handler();
        app_input_event_t ev;
        while (buttons_poll(&ev)) {
            snprintf(s_btn_msg, sizeof(s_btn_msg), "%s",
                     buttons_event_name(ev));
            ESP_LOGI(TAG, "%s", s_btn_msg);
            handle_input_event(ev);
            render_home();
        }
        handle_button_long_press();
        dispatch_gui_action(gui_shell_tick(now_ms()));
        if (s_setup_mode) {
            poll_wifi_provision();
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

        poll_wifi_provision();
        poll_rtc_time();
        refresh_chime_schedule();

        bool face_now = voice_face_visible();
        if (face_now != s_face_was_visible) {
            s_face_was_visible = face_now;
            render_home();
        }

        if (s_voice_status_dirty) {
            s_voice_status_dirty = false;
            snprintf(s_status_msg, sizeof(s_status_msg), "%s", voice_chat_message());
            render_home();
        }
        if (s_chime_status_dirty) {
            s_chime_status_dirty = false;
            snprintf(s_status_msg, sizeof(s_status_msg), "%s", chime_player_message());
            render_home();
        }

        if (!wifi_time_has_credentials() || s_setup_mode || wifi_provision_active()) {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

        if (!audio_activity_busy() &&
            !wifi_time_is_connected() &&
            (uint32_t)(now_ms() - s_last_wifi_retry_ms) >= APP_WIFI_RETRY_MS) {
            ensure_wifi_connected("auto");
        }

        maybe_dispatch_scheduled_chime();

        if (!audio_activity_busy() &&
            (uint32_t)(now_ms() - s_last_refresh_ms) >= APP_TIME_REFRESH_MS) {
            sync_time_from_wifi("hourly");
        }

        if (now_ms() - s_last_mem_log_ms >= 300000) {
            mem_log("periodic");
            s_last_mem_log_ms = now_ms();
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
