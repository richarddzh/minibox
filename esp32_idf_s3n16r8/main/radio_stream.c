#include "radio_stream.h"
#include "app_config.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "esp_audio_dec_default.h"
#include "esp_audio_simple_dec.h"
#include "esp_audio_simple_dec_default.h"
#include "esp_check.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"

#define RADIO_URL "https://lhttp.qtfm.cn/live/274/64k.mp3"
#define INPUT_BYTES 2048
#define OUTPUT_BYTES 8192

static const char *TAG = "radio";

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
    esp_err_t error = i2s_channel_write(channel, stereo, bytes, &written,
                                         pdMS_TO_TICKS(2000));
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
        esp_http_client_fetch_headers(client);
        int status = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "Donggan 101 HTTP %d", status);
        result = status == 200 || status == 206 ? decode_stream(client) : ESP_ERR_INVALID_RESPONSE;
    }
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return result;
}
