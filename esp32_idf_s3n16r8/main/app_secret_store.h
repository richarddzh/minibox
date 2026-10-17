#pragma once
#include <stdbool.h>
#include "app_config.h"

#define APP_SECRET_VALUE_MAX 256

typedef struct {
    char asr_api_key[APP_SECRET_VALUE_MAX];
    char agent_api_key[APP_SECRET_VALUE_MAX];
    char tts_api_key[APP_SECRET_VALUE_MAX];
} app_secrets_t;

bool app_secrets_valid(const app_secrets_t *s);
int  app_secrets_load_partial(app_secrets_t *out);
int  app_secrets_load(app_secrets_t *out);
int  app_secrets_save_partial(const app_secrets_t *in);
int  app_secrets_save(const app_secrets_t *in);

void app_secrets_set_current(const app_secrets_t *s);
bool app_secrets_ready(void);
const char *app_secrets_asr_api_key(void);
const char *app_secrets_agent_api_key(void);
const char *app_secrets_tts_api_key(void);
