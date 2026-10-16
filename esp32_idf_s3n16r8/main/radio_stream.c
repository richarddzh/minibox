#include "radio_stream.h"
#include "app_config.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "esp_audio_dec_default.h"
#include "esp_audio_simple_dec.h"
#include "esp_audio_simple_dec_default.h"
#include "esp_check.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_wifi.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#define RADIO_URL "https://lhttp.qtfm.cn/live/274/64k.mp3"
#define INPUT_BYTES 2048
#define OUTPUT_BYTES 8192

static const char *TAG = "radio";
static EventGroupHandle_t s_network;
#define WIFI_CONNECTED BIT0

typedef enum {
    RADIO_CONNECTING_WIFI,
    RADIO_SYNCING_TIME,
    RADIO_CONNECTING_STREAM,
    RADIO_PLAYING,
    RADIO_RETRYING,
    RADIO_ERROR,
} radio_state_t;

static atomic_int s_state = RADIO_CONNECTING_WIFI;

const char *radio_stream_status(void) {
    switch (atomic_load(&s_state)) {
    case RADIO_CONNECTING_WIFI: return "101: CONNECTING WIFI";
    case RADIO_SYNCING_TIME: return "101: SYNCING TIME";
    case RADIO_CONNECTING_STREAM: return "101: CONNECTING STREAM";
    case RADIO_PLAYING: return "101: PLAYING DONGGAN";
    case RADIO_RETRYING: return "101: RETRY IN 5s";
    default: return "101: ERROR - SEE SERIAL";
    }
}

static esp_err_t speaker_stop(i2s_chan_handle_t *channel) {
    esp_err_t error = gpio_set_level(SPEAKER_SD_MODE_PIN, 0);
    if (*channel) {
        esp_err_t stopped = i2s_channel_disable(*channel);
        if (error == ESP_OK) error = stopped;
        esp_err_t deleted = i2s_del_channel(*channel);
        if (error == ESP_OK) error = deleted;
        *channel = NULL;
    }
    esp_err_t quiet = gpio_set_level(SPEAKER_DIN_PIN, 0);
    return error != ESP_OK ? error : quiet;
}

static esp_err_t speaker_start(i2s_chan_handle_t *channel, uint32_t rate) {
    i2s_chan_config_t chan = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    chan.dma_desc_num = 8;
    chan.dma_frame_num = 512;
    ESP_RETURN_ON_ERROR(i2s_new_channel(&chan, channel, NULL), TAG, "speaker channel");
    i2s_std_config_t config = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(rate),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
            I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = AUDIO_BCLK_PIN,
            .ws = AUDIO_WS_PIN,
            .dout = SPEAKER_DIN_PIN,
            .din = I2S_GPIO_UNUSED,
        },
    };
    esp_err_t error = i2s_channel_init_std_mode(*channel, &config);
    if (error == ESP_OK) error = i2s_channel_enable(*channel);
    bool enabled = error == ESP_OK;
    if (error == ESP_OK) error = gpio_set_level(SPEAKER_SD_MODE_PIN, 1);
    if (error == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(10));
    } else {
        ESP_LOGE(TAG, "Speaker start: %s", esp_err_to_name(error));
        esp_err_t quiet = gpio_set_level(SPEAKER_SD_MODE_PIN, 0);
        esp_err_t stopped = enabled ? i2s_channel_disable(*channel) : ESP_OK;
        esp_err_t deleted = i2s_del_channel(*channel);
        *channel = NULL;
        if (quiet != ESP_OK) ESP_LOGE(TAG, "Speaker shutdown: %s", esp_err_to_name(quiet));
        if (stopped != ESP_OK) ESP_LOGE(TAG, "Speaker disable: %s", esp_err_to_name(stopped));
        if (deleted != ESP_OK) ESP_LOGE(TAG, "Speaker cleanup: %s", esp_err_to_name(deleted));
    }
    return error;
}

esp_err_t radio_stream_init(void) {
    ESP_RETURN_ON_ERROR(gpio_set_level(SPEAKER_SD_MODE_PIN, 0), TAG, "speaker shutdown");
    const gpio_config_t shutdown = {
        .pin_bit_mask = 1ULL << SPEAKER_SD_MODE_PIN,
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&shutdown), TAG, "speaker shutdown pin");
    const gpio_config_t gain = {
        .pin_bit_mask = 1ULL << SPEAKER_GAIN_PIN,
#ifdef CONFIG_MINIBOX_AMP_GAIN_12DB
        .mode = GPIO_MODE_OUTPUT,
#else
        .mode = GPIO_MODE_INPUT,
#endif
    };
#ifdef CONFIG_MINIBOX_AMP_GAIN_12DB
    ESP_RETURN_ON_ERROR(gpio_set_level(SPEAKER_GAIN_PIN, 0), TAG, "speaker gain");
#endif
    ESP_RETURN_ON_ERROR(gpio_config(&gain), TAG, "speaker gain pin");
    const gpio_config_t din = {
        .pin_bit_mask = 1ULL << SPEAKER_DIN_PIN,
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&din), TAG, "speaker data pin");
    ESP_RETURN_ON_ERROR(gpio_set_level(SPEAKER_DIN_PIN, 0), TAG, "speaker data low");
    esp_audio_err_t error = esp_audio_dec_register_default();
    if (error != ESP_AUDIO_ERR_OK) {
        ESP_LOGE(TAG, "Audio decoder registration: %d", error);
        return ESP_FAIL;
    }
    error = esp_audio_simple_dec_register_default();
    if (error != ESP_AUDIO_ERR_OK) {
        ESP_LOGE(TAG, "MP3 decoder registration: %d", error);
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t play_pcm(i2s_chan_handle_t channel, const uint8_t *pcm,
                          size_t size, uint8_t channels, int16_t *stereo) {
    if (size % (channels * sizeof(int16_t))) return ESP_ERR_INVALID_SIZE;
    size_t frames = size / (channels * sizeof(int16_t));
    const int16_t *samples = (const int16_t *)pcm;
    for (size_t i = 0; i < frames; ++i) {
        stereo[2 * i] = samples[channels * i] / 2;
        stereo[2 * i + 1] = samples[channels * i + channels - 1] / 2;
    }
    size_t bytes = frames * 2 * sizeof(int16_t);
    size_t written = 0;
    esp_err_t error = i2s_channel_write(channel, stereo, bytes, &written, 2000);
    if (error != ESP_OK) return error;
    return written == bytes ? ESP_OK : ESP_ERR_TIMEOUT;
}

static esp_err_t decode_stream(esp_http_client_handle_t client) {
    uint8_t *input = malloc(INPUT_BYTES);
    uint8_t *output = malloc(OUTPUT_BYTES);
    int16_t *stereo = malloc(OUTPUT_BYTES * 2);
    if (!input || !output || !stereo) {
        free(input);
        free(output);
        free(stereo);
        return ESP_ERR_NO_MEM;
    }

    esp_audio_simple_dec_handle_t decoder = NULL;
    esp_audio_simple_dec_cfg_t config = {
        .dec_type = ESP_AUDIO_SIMPLE_DEC_TYPE_MP3,
        .use_frame_dec = false,
    };
    esp_audio_err_t decoded = esp_audio_simple_dec_open(&config, &decoder);
    if (decoded != ESP_AUDIO_ERR_OK) {
        ESP_LOGE(TAG, "MP3 decoder open: %d", decoded);
        free(input);
        free(output);
        free(stereo);
        return ESP_FAIL;
    }

    i2s_chan_handle_t speaker = NULL;
    uint8_t channels = 0;
    esp_err_t result = ESP_OK;
    unsigned idle_reads = 0;
    uint64_t played_bytes = 0;
    while (result == ESP_OK) {
        int read = esp_http_client_read(client, (char *)input, INPUT_BYTES);
        if (read == -ESP_ERR_HTTP_EAGAIN) {
            if (++idle_reads < 5) continue;
            result = ESP_ERR_TIMEOUT;
            ESP_LOGW(TAG, "Stream timed out");
            break;
        }
        if (read <= 0) {
            result = read == 0 ? ESP_ERR_INVALID_RESPONSE : ESP_FAIL;
            ESP_LOGW(TAG, "Stream read returned %d", read);
            break;
        }
        idle_reads = 0;
        esp_audio_simple_dec_raw_t raw = {
            .buffer = input,
            .len = (uint32_t)read,
        };
        while (raw.len && result == ESP_OK) {
            esp_audio_simple_dec_out_t frame = {
                .buffer = output,
                .len = OUTPUT_BYTES,
            };
            decoded = esp_audio_simple_dec_process(decoder, &raw, &frame);
            if (decoded != ESP_AUDIO_ERR_OK) {
                ESP_LOGE(TAG, "MP3 decoder: %d (need %u bytes)",
                         decoded, (unsigned)frame.needed_size);
                result = ESP_FAIL;
                break;
            }
            if (frame.decoded_size) {
                if (!speaker) {
                    esp_audio_simple_dec_info_t info = {0};
                    decoded = esp_audio_simple_dec_get_info(decoder, &info);
                    if (decoded != ESP_AUDIO_ERR_OK || info.bits_per_sample != 16 ||
                        (info.channel != 1 && info.channel != 2) ||
                        info.sample_rate < 8000 || info.sample_rate > 96000) {
                        ESP_LOGE(TAG, "Unsupported MP3 format (decoder status %d)", decoded);
                        result = ESP_ERR_NOT_SUPPORTED;
                        break;
                    }
                    channels = info.channel;
                    ESP_LOGI(TAG, "MP3: %u Hz, %u channels, %u bit/s",
                             (unsigned)info.sample_rate, channels, (unsigned)info.bitrate);
                    result = speaker_start(&speaker, info.sample_rate);
                    if (result != ESP_OK) break;
                }
                result = play_pcm(speaker, frame.buffer, frame.decoded_size,
                                  channels, stereo);
                if (result != ESP_OK) {
                    ESP_LOGE(TAG, "I2S write: %s", esp_err_to_name(result));
                    break;
                }
                atomic_store(&s_state, RADIO_PLAYING);
                size_t bytes = frame.decoded_size * 2 / channels;
                if (played_bytes == 0 || played_bytes / (1024 * 1024) !=
                                         (played_bytes + bytes) / (1024 * 1024)) {
                    ESP_LOGI(TAG, "I2S audio output: %" PRIu64 " KiB",
                             (played_bytes + bytes) / 1024);
                }
                played_bytes += bytes;
            }
            if (!raw.consumed) {
                if (result == ESP_OK) result = ESP_ERR_INVALID_RESPONSE;
                ESP_LOGE(TAG, "MP3 decoder made no input progress");
                break;
            }
            if (raw.consumed > raw.len) {
                result = ESP_ERR_INVALID_SIZE;
                ESP_LOGE(TAG, "MP3 decoder consumed beyond input");
                break;
            }
            raw.buffer += raw.consumed;
            raw.len -= raw.consumed;
        }
    }
    esp_err_t stopped = speaker_stop(&speaker);
    if (stopped != ESP_OK) ESP_LOGE(TAG, "Speaker stop: %s", esp_err_to_name(stopped));
    if (result == ESP_OK) result = stopped;
    esp_audio_simple_dec_close(decoder);
    free(input);
    free(output);
    free(stereo);
    return result;
}

esp_err_t radio_stream_play(void) {
    const esp_http_client_config_t config = {
        .url = RADIO_URL,
        .timeout_ms = 3000,
        .buffer_size = INPUT_BYTES,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .keep_alive_enable = true,
        .max_redirection_count = 4,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    ESP_RETURN_ON_FALSE(client, ESP_ERR_NO_MEM, TAG, "HTTP client");
    esp_err_t result = esp_http_client_set_header(client, "Accept", "audio/mpeg");
    if (result == ESP_OK) result = esp_http_client_set_header(client, "Icy-MetaData", "0");
    if (result == ESP_OK) result = esp_http_client_open(client, 0);
    if (result == ESP_OK) {
        int64_t headers = esp_http_client_fetch_headers(client);
        if (headers < 0) {
            ESP_LOGE(TAG, "Donggan 101 response headers failed: %" PRId64, headers);
            result = ESP_ERR_INVALID_RESPONSE;
        } else {
            int status = esp_http_client_get_status_code(client);
            ESP_LOGI(TAG, "Donggan 101 HTTP %d", status);
            result = status == 200 || status == 206 ? decode_stream(client) :
                                                     ESP_ERR_INVALID_RESPONSE;
        }
    }
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return result;
}

static void wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data) {
    (void)arg;
    if (base == WIFI_EVENT &&
        (id == WIFI_EVENT_STA_START || id == WIFI_EVENT_STA_DISCONNECTED)) {
        xEventGroupClearBits(s_network, WIFI_CONNECTED);
        atomic_store(&s_state, RADIO_CONNECTING_WIFI);
        if (id == WIFI_EVENT_STA_DISCONNECTED) {
            const wifi_event_sta_disconnected_t *event = data;
            ESP_LOGW(TAG, "Wi-Fi disconnected (reason %u); reconnecting", event->reason);
        }
        esp_err_t error = esp_wifi_connect();
        if (error != ESP_OK) {
            ESP_LOGE(TAG, "Wi-Fi connect: %s", esp_err_to_name(error));
            atomic_store(&s_state, RADIO_ERROR);
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *event = data;
        ESP_LOGI(TAG, "Wi-Fi connected, IP " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_network, WIFI_CONNECTED);
    }
}

static void radio_task(void *arg) {
    (void)arg;
    bool clock_synced = false;
    for (;;) {
        xEventGroupWaitBits(s_network, WIFI_CONNECTED, pdFALSE, pdTRUE, portMAX_DELAY);
        if (!clock_synced) {
            atomic_store(&s_state, RADIO_SYNCING_TIME);
            esp_err_t error = esp_netif_sntp_start();
            if (error == ESP_OK) {
                error = esp_netif_sntp_sync_wait(pdMS_TO_TICKS(15000));
            }
            if (error != ESP_OK) {
                ESP_LOGW(TAG, "Waiting for network time: %s", esp_err_to_name(error));
                vTaskDelay(pdMS_TO_TICKS(1000));
                continue;
            }
            clock_synced = true;
            ESP_LOGI(TAG, "Network clock synchronized for HTTPS");
        }
        atomic_store(&s_state, RADIO_CONNECTING_STREAM);
        ESP_LOGI(TAG, "Opening Donggan 101: " RADIO_URL);
        esp_err_t error = radio_stream_play();
        ESP_LOGE(TAG, "Donggan 101 stream stopped: %s; retrying in 5s",
                 esp_err_to_name(error));
        atomic_store(&s_state, RADIO_RETRYING);
        vTaskDelay(pdMS_TO_TICKS(5000));
        if (!(xEventGroupGetBits(s_network) & WIFI_CONNECTED)) {
            atomic_store(&s_state, RADIO_CONNECTING_WIFI);
        }
    }
}

esp_err_t radio_stream_start(void) {
    ESP_RETURN_ON_FALSE(strlen(CONFIG_MINIBOX_WIFI_SSID) > 0 &&
                        strlen(CONFIG_MINIBOX_WIFI_SSID) <= 32 &&
                        strlen(CONFIG_MINIBOX_WIFI_PASSWORD) >= 8 &&
                        strlen(CONFIG_MINIBOX_WIFI_PASSWORD) <= 63,
                        ESP_ERR_INVALID_ARG, TAG, "configure Wi-Fi SSID and WPA password locally");
    ESP_RETURN_ON_ERROR(radio_stream_init(), TAG, "initialize radio audio");
    ESP_RETURN_ON_ERROR(nvs_flash_init(), TAG, "initialize NVS");
    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "initialize network");
    ESP_RETURN_ON_ERROR(esp_event_loop_create_default(), TAG, "create event loop");
    ESP_RETURN_ON_FALSE(esp_netif_create_default_wifi_sta(), ESP_ERR_NO_MEM,
                        TAG, "create Wi-Fi station");
    s_network = xEventGroupCreate();
    ESP_RETURN_ON_FALSE(s_network, ESP_ERR_NO_MEM, TAG, "create Wi-Fi event group");
    const wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&init), TAG, "initialize Wi-Fi");
    ESP_RETURN_ON_ERROR(esp_wifi_set_storage(WIFI_STORAGE_RAM), TAG, "Wi-Fi RAM config");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                   wifi_event, NULL), TAG, "Wi-Fi events");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                   wifi_event, NULL), TAG, "IP event");
    wifi_config_t config = {.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK};
    memcpy(config.sta.ssid, CONFIG_MINIBOX_WIFI_SSID, strlen(CONFIG_MINIBOX_WIFI_SSID));
    memcpy(config.sta.password, CONFIG_MINIBOX_WIFI_PASSWORD,
           strlen(CONFIG_MINIBOX_WIFI_PASSWORD));
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "Wi-Fi station mode");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &config), TAG, "Wi-Fi credentials");
    esp_sntp_config_t time_config = ESP_NETIF_SNTP_DEFAULT_CONFIG("ntp.aliyun.com");
    time_config.start = false;
    ESP_RETURN_ON_ERROR(esp_netif_sntp_init(&time_config), TAG, "initialize network clock");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "start Wi-Fi");
    BaseType_t created = xTaskCreate(radio_task, "radio", 12288, NULL, 4, NULL);
    ESP_RETURN_ON_FALSE(created == pdPASS, ESP_ERR_NO_MEM, TAG, "start radio task");
    ESP_LOGI(TAG, "Donggan 101 only; microphone disabled; BCLK=%d WS=%d DIN=%d SD=%d GAIN=%d",
             AUDIO_BCLK_PIN, AUDIO_WS_PIN, SPEAKER_DIN_PIN,
             SPEAKER_SD_MODE_PIN, SPEAKER_GAIN_PIN);
    return ESP_OK;
}
