#pragma once
#include <stdarg.h>
#include <stdio.h>
#define ESP_LOG_DEBUG 4
#define ESP_LOGE(tag, ...) do { fprintf(stderr, "%s: ", tag); fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } while (0)
#define ESP_LOGI(tag, ...) ((void)0)
static inline void esp_log_writev(int level, const char *tag, const char *format, va_list args) {
    (void)level; (void)tag; (void)format; (void)args;
}
