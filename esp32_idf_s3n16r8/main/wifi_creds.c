#include "wifi_creds.h"
#include "app_config.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <esp_log.h>
#include <esp_spiffs.h>

static const char *TAG = "wifi_creds";

#define WIFI_PROFILES_MAGIC   0x57465031U
#define WIFI_PROFILES_VERSION 1U
#define WIFI_CONFIG_TEMP_PATH   "/spiffs/wifi_config.tmp"
#define WIFI_CONFIG_BACKUP_PATH "/spiffs/wifi_config.bak"
#define WIFI_PROFILES_TEMP_PATH "/spiffs/wifi_profiles.tmp"
#define WIFI_PROFILES_BACKUP_PATH "/spiffs/wifi_profiles.bak"

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t count;
    wifi_creds_t profiles[WIFI_CREDS_MAX_PROFILES];
} wifi_profiles_file_t;

static void rstrip(char *s) {
    size_t n = strlen(s);
    while (n > 0 && (s[n-1] == '\r' || s[n-1] == '\n')) {
        s[--n] = 0;
    }
}

static bool read_line(FILE *file, char *value, size_t capacity) {
    if (!fgets(value, capacity, file)) return false;
    if (!strchr(value, '\n')) {
        int next = fgetc(file);
        if (next == '\r') next = fgetc(file);
        if (next != '\n' && next != EOF) return false;
    }
    rstrip(value);
    return !ferror(file);
}

static bool replace_file(const char *temp_path, const char *target_path,
                         const char *backup_path) {
    remove(backup_path);
    if (rename(target_path, backup_path) != 0 && errno != ENOENT) {
        remove(temp_path);
        return false;
    }
    if (rename(temp_path, target_path) == 0) {
        remove(backup_path);
        return true;
    }
    rename(backup_path, target_path);
    remove(temp_path);
    return false;
}

bool wifi_creds_valid(const wifi_creds_t *c) {
    return c && c->ssid[0] &&
           strnlen(c->ssid, sizeof(c->ssid)) < sizeof(c->ssid) &&
           strnlen(c->password, sizeof(c->password)) < sizeof(c->password) &&
           !strpbrk(c->ssid, "\r\n") && !strpbrk(c->password, "\r\n");
}

esp_err_t wifi_creds_init(void) {
    esp_vfs_spiffs_conf_t conf = {
        .base_path = APP_SPIFFS_BASE_PATH,
        .partition_label = "storage",
        .max_files = 4,
        .format_if_mount_failed = false,
    };
    esp_err_t e = esp_vfs_spiffs_register(&conf);
    if (e != ESP_OK) {
        ESP_LOGE(TAG, "spiffs mount: %s", esp_err_to_name(e));
    }
    return e;
}

int wifi_creds_load(wifi_creds_t *out) {
    if (!out) return -2;
    memset(out, 0, sizeof(*out));
    FILE *f = fopen(APP_WIFI_CONFIG_PATH, "r");
    if (!f) f = fopen(WIFI_CONFIG_BACKUP_PATH, "r");
    if (!f) return -1;
    if (!read_line(f, out->ssid, sizeof(out->ssid)) ||
        !read_line(f, out->password, sizeof(out->password))) {
        fclose(f);
        ESP_LOGE(TAG, "Invalid Wi-Fi configuration file");
        return -2;
    }
    fclose(f);
    return wifi_creds_valid(out) ? 0 : -2;
}

int wifi_creds_save(const wifi_creds_t *in) {
    if (!wifi_creds_valid(in)) return -2;
    FILE *f = fopen(WIFI_CONFIG_TEMP_PATH, "w");
    if (!f) return -2;
    bool active_ok = fprintf(f, "%s\n%s\n", in->ssid, in->password) >= 0;
    if (fclose(f) != 0) active_ok = false;
    if (!active_ok ||
        !replace_file(WIFI_CONFIG_TEMP_PATH, APP_WIFI_CONFIG_PATH,
                      WIFI_CONFIG_BACKUP_PATH)) {
        remove(WIFI_CONFIG_TEMP_PATH);
        return -2;
    }

    wifi_profiles_file_t store = {
        .magic = WIFI_PROFILES_MAGIC,
        .version = WIFI_PROFILES_VERSION,
    };
    FILE *profiles = fopen(APP_WIFI_PROFILES_PATH, "rb");
    if (!profiles) profiles = fopen(WIFI_PROFILES_BACKUP_PATH, "rb");
    if (profiles) {
        wifi_profiles_file_t saved = {0};
        if (fread(&saved, 1, sizeof(saved), profiles) == sizeof(saved) &&
            saved.magic == WIFI_PROFILES_MAGIC &&
            saved.version == WIFI_PROFILES_VERSION &&
            saved.count <= WIFI_CREDS_MAX_PROFILES) {
            store = saved;
        }
        fclose(profiles);
    }

    size_t existing = store.count;
    for (size_t i = 0; i < store.count; ++i) {
        if (strcmp(store.profiles[i].ssid, in->ssid) == 0) {
            existing = i;
            break;
        }
    }
    if (existing < store.count) {
        memmove(&store.profiles[1], &store.profiles[0],
                existing * sizeof(store.profiles[0]));
    } else {
        size_t move_count = store.count < WIFI_CREDS_MAX_PROFILES
                                ? store.count
                                : WIFI_CREDS_MAX_PROFILES - 1;
        memmove(&store.profiles[1], &store.profiles[0],
                move_count * sizeof(store.profiles[0]));
        if (store.count < WIFI_CREDS_MAX_PROFILES) store.count++;
    }
    store.profiles[0] = *in;

    profiles = fopen(WIFI_PROFILES_TEMP_PATH, "wb");
    if (!profiles) return -2;
    bool ok = fwrite(&store, 1, sizeof(store), profiles) == sizeof(store);
    if (fclose(profiles) != 0) ok = false;
    if (!ok ||
        !replace_file(WIFI_PROFILES_TEMP_PATH, APP_WIFI_PROFILES_PATH,
                      WIFI_PROFILES_BACKUP_PATH)) {
        remove(WIFI_PROFILES_TEMP_PATH);
        return -2;
    }
    return 0;
}

static int load_profiles(wifi_profiles_file_t *store) {
    memset(store, 0, sizeof(*store));
    FILE *f = fopen(APP_WIFI_PROFILES_PATH, "rb");
    if (!f) f = fopen(WIFI_PROFILES_BACKUP_PATH, "rb");
    if (f) {
        bool ok = fread(store, 1, sizeof(*store), f) == sizeof(*store);
        fclose(f);
        if (ok && store->magic == WIFI_PROFILES_MAGIC &&
            store->version == WIFI_PROFILES_VERSION &&
            store->count <= WIFI_CREDS_MAX_PROFILES) {
            return 0;
        }
    }

    wifi_creds_t legacy = {0};
    if (wifi_creds_load(&legacy) != 0) return -1;
    store->magic = WIFI_PROFILES_MAGIC;
    store->version = WIFI_PROFILES_VERSION;
    store->count = 1;
    store->profiles[0] = legacy;
    return 0;
}

int wifi_creds_list(wifi_creds_t *out, size_t capacity, size_t *count) {
    if (!count || (capacity > 0 && !out)) return -2;
    wifi_profiles_file_t store;
    if (load_profiles(&store) != 0) {
        *count = 0;
        return 0;
    }
    size_t copied = store.count < capacity ? store.count : capacity;
    if (copied > 0) {
        memcpy(out, store.profiles, copied * sizeof(store.profiles[0]));
    }
    *count = copied;
    return 0;
}

int wifi_creds_find(const char *ssid, wifi_creds_t *out) {
    if (!ssid || !ssid[0] || !out) return -2;
    wifi_profiles_file_t store;
    if (load_profiles(&store) != 0) return -1;
    for (size_t i = 0; i < store.count; ++i) {
        if (strcmp(store.profiles[i].ssid, ssid) == 0) {
            *out = store.profiles[i];
            return 0;
        }
    }
    return -1;
}
