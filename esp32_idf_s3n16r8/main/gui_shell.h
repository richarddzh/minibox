#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "gui_app.h"

void gui_shell_init(const gui_app_t *const *apps, size_t app_count,
                    size_t default_app);
gui_action_t gui_shell_handle_input(gui_input_t input);
gui_action_t gui_shell_tick(int64_t now_ms);
void gui_shell_render(const gui_model_t *model);
bool gui_shell_is_launcher(void);
bool gui_shell_current_app_is(const char *id);
