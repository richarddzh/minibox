#pragma once

#include "gui_app.h"

const gui_app_t *app_chat_alarm_descriptor(void);
void app_chat_alarm_set_recording(bool recording, int64_t started_ms);
bool app_chat_alarm_is_recording(void);
void app_chat_alarm_append_user(const char *text);
void app_chat_alarm_append_assistant(const char *text);
