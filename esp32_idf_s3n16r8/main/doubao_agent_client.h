#pragma once

#include <esp_err.h>

esp_err_t doubao_agent_chat(const char *user_text, char **out_text);
esp_err_t doubao_agent_chime(const char *time_arg, char **out_text);
