#include "audio_test.h"
#include "app_config.h"

#include <limits.h>
#include <stdatomic.h>
#include <stdlib.h>
#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#define AUDIO_FRAMES (AUDIO_SAMPLE_RATE * AUDIO_MAX_SECONDS)
#define AUDIO_BLOCK_FRAMES 256

typedef struct {
    joystick_button_t button;
    int direction_x;
} audio_input_t;

static const char *TAG = "audio";
static int16_t *s_recording;
static QueueHandle_t s_buttons;
static atomic_int s_phase = AUDIO_IDLE;

static void set_phase(audio_phase_t phase) {
    atomic_store(&s_phase, phase);
}

audio_phase_t audio_test_phase(void) {
    return atomic_load(&s_phase);
}

static esp_err_t open_channel(bool receive, i2s_chan_handle_t *channel) {
    i2s_chan_config_t channel_config =
        I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    esp_err_t error = receive ? i2s_new_channel(&channel_config, NULL, channel) :
                                i2s_new_channel(&channel_config, channel, NULL);
    ESP_RETURN_ON_ERROR(error, TAG, "create I2S channel");

    i2s_std_config_t config = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(AUDIO_SAMPLE_RATE),
        .slot_cfg = receive ?
            (i2s_std_slot_config_t)I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
                I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO) :
            (i2s_std_slot_config_t)I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
                I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = AUDIO_BCLK_PIN,
            .ws = AUDIO_WS_PIN,
            .dout = receive ? I2S_GPIO_UNUSED : SPEAKER_DIN_PIN,
            .din = receive ? MIC_SD_PIN : I2S_GPIO_UNUSED,
        },
    };
    if (receive) {
        config.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;
    }
    error = i2s_channel_init_std_mode(*channel, &config);
    if (error == ESP_OK) error = i2s_channel_enable(*channel);
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "I2S initialization: %s", esp_err_to_name(error));
        esp_err_t cleanup = i2s_del_channel(*channel);
        if (cleanup != ESP_OK) {
            ESP_LOGE(TAG, "I2S cleanup: %s", esp_err_to_name(cleanup));
        }
        *channel = NULL;
    }
    return error;
}

static esp_err_t close_channel(i2s_chan_handle_t *channel) {
    if (!*channel) return ESP_OK;
    esp_err_t error = i2s_channel_disable(*channel);
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "I2S disable: %s", esp_err_to_name(error));
    }
    esp_err_t deleted = i2s_del_channel(*channel);
    if (deleted != ESP_OK) {
        ESP_LOGE(TAG, "I2S delete: %s", esp_err_to_name(deleted));
        return deleted;
    }
    *channel = NULL;
    return error;
}

static esp_err_t speaker_idle(void) {
    ESP_RETURN_ON_ERROR(gpio_set_level(SPEAKER_SD_MODE_PIN, 0), TAG, "speaker shutdown");
    const gpio_config_t pin = {
        .pin_bit_mask = 1ULL << SPEAKER_DIN_PIN,
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&pin), TAG, "speaker DIN");
    return gpio_set_level(SPEAKER_DIN_PIN, 0);
}

static esp_err_t speaker_pins_init(void) {
    ESP_RETURN_ON_ERROR(gpio_set_level(SPEAKER_SD_MODE_PIN, 0), TAG, "shutdown level");
    const gpio_config_t shutdown = {
        .pin_bit_mask = 1ULL << SPEAKER_SD_MODE_PIN,
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&shutdown), TAG, "shutdown pin");
    const gpio_config_t gain = {
        .pin_bit_mask = 1ULL << SPEAKER_GAIN_PIN,
#ifdef CONFIG_MINIBOX_AMP_GAIN_12DB
        .mode = GPIO_MODE_OUTPUT,
#else
        .mode = GPIO_MODE_INPUT,
#endif
    };
#ifdef CONFIG_MINIBOX_AMP_GAIN_12DB
    ESP_RETURN_ON_ERROR(gpio_set_level(SPEAKER_GAIN_PIN, 0), TAG, "gain level");
#endif
    ESP_RETURN_ON_ERROR(gpio_config(&gain), TAG, "gain pin");
    return speaker_idle();
}

static void log_recording_levels(size_t frames) {
    int16_t minimum = INT16_MAX;
    int16_t maximum = INT16_MIN;
    uint32_t nonzero = 0;
    uint64_t total_magnitude = 0;
    for (size_t i = 0; i < frames; ++i) {
        int32_t sample = s_recording[i];
        if (sample < minimum) minimum = sample;
        if (sample > maximum) maximum = sample;
        if (sample != 0) ++nonzero;
        total_magnitude += sample < 0 ? -sample : sample;
    }
    ESP_LOGI(TAG, "Mic: %u samples, min=%d max=%d nonzero=%u avg_abs=%u",
             (unsigned)frames, frames ? minimum : 0, frames ? maximum : 0,
             (unsigned)nonzero,
             frames ? (unsigned)(total_magnitude / frames) : 0);
    if (frames && nonzero == 0) {
        ESP_LOGW(TAG, "Microphone data is silent; check VDD, SD, and L/R=GND");
    }
}

static esp_err_t play_recording(size_t frames, bool tone) {
    i2s_chan_handle_t channel = NULL;
    esp_err_t error = open_channel(false, &channel);
    if (error != ESP_OK) {
        esp_err_t idle = speaker_idle();
        return idle != ESP_OK ? idle : error;
    }
    error = gpio_set_level(SPEAKER_SD_MODE_PIN, 1);
    if (error == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    int16_t stereo[AUDIO_BLOCK_FRAMES * 2];
    for (size_t offset = 0; offset < frames && error == ESP_OK;) {
        size_t count = frames - offset;
        if (count > AUDIO_BLOCK_FRAMES) count = AUDIO_BLOCK_FRAMES;
        for (size_t i = 0; i < count; ++i) {
            int16_t sample;
            if (tone) {
                int phase = (offset + i) % 32;
                sample = ((phase < 16 ? phase : 32 - phase) - 8) * 256;
            } else {
                sample = s_recording[offset + i];
            }
            stereo[2 * i] = sample;
            stereo[2 * i + 1] = sample;
        }
        size_t written = 0;
        error = i2s_channel_write(channel, stereo, count * 2 * sizeof(int16_t),
                                  &written, 1000);
        if (error == ESP_OK && written != count * 2 * sizeof(int16_t)) {
            error = ESP_ERR_TIMEOUT;
        }
        offset += count;
    }
    if (error == ESP_OK) {
        /* The last write may still be in DMA; allow its queued samples to finish. */
        vTaskDelay(pdMS_TO_TICKS(150));
    }
    esp_err_t idle = gpio_set_level(SPEAKER_SD_MODE_PIN, 0);
    esp_err_t closed = close_channel(&channel);
    esp_err_t data_idle = speaker_idle();
    if (error != ESP_OK) ESP_LOGE(TAG, "Speaker write: %s", esp_err_to_name(error));
    if (idle != ESP_OK) return idle;
    if (closed != ESP_OK) return closed;
    if (data_idle != ESP_OK) return data_idle;
    return error;
}

static void audio_task(void *arg) {
    (void)arg;
    audio_test_logic_t logic = {0};
    joystick_button_t button = {0};
    int direction_x = 0;
    bool tone_mode = false;
    i2s_chan_handle_t mic = NULL;
    size_t frames = 0;
    esp_err_t error = ESP_OK;
    for (;;) {
        audio_input_t latest;
        TickType_t wait = logic.phase == AUDIO_RECORDING ? 0 : pdMS_TO_TICKS(20);
        if (xQueueReceive(s_buttons, &latest, wait) == pdTRUE) {
            button = latest.button;
            direction_x = latest.direction_x;
        }
        int64_t now_ms = esp_timer_get_time() / 1000;
        bool full = frames == AUDIO_FRAMES;
        audio_action_t action = audio_test_logic_step(&logic, &button, now_ms, full);
        if (action == AUDIO_START_RECORD) {
            frames = 0;
            tone_mode = direction_x > 0;
            if (tone_mode) {
                logic.phase = AUDIO_TONE_WAIT_RELEASE;
                ESP_LOGI(TAG, "Speaker test: release K to play 500 Hz tone");
            } else {
                error = open_channel(true, &mic);
                if (error == ESP_OK) {
                    ESP_LOGI(TAG, "Recording (max %d s, %d Hz)", AUDIO_MAX_SECONDS,
                             AUDIO_SAMPLE_RATE);
                }
            }
        } else if (action == AUDIO_STOP_RECORD || action == AUDIO_STOP_AND_PLAY) {
            error = close_channel(&mic);
            if (error == ESP_OK) log_recording_levels(frames);
        }
        if (error == ESP_OK &&
            (action == AUDIO_STOP_AND_PLAY || action == AUDIO_START_PLAY)) {
            set_phase(AUDIO_PLAYING);
            if (tone_mode) {
                ESP_LOGI(TAG, "Playing 500 Hz test tone");
            } else {
                ESP_LOGI(TAG, "Playing %u samples", (unsigned)frames);
            }
            error = play_recording(tone_mode ? AUDIO_SAMPLE_RATE / 2 : frames,
                                   tone_mode);
            logic.phase = AUDIO_WAIT_RESET;
        }
        if (error == ESP_OK && logic.phase == AUDIO_RECORDING && mic) {
            int32_t raw[AUDIO_BLOCK_FRAMES];
            size_t count = AUDIO_FRAMES - frames;
            if (count > AUDIO_BLOCK_FRAMES) count = AUDIO_BLOCK_FRAMES;
            size_t bytes = 0;
            error = i2s_channel_read(mic, raw, count * sizeof(int32_t),
                                     &bytes, 100);
            if (error == ESP_OK && (bytes == 0 || bytes % sizeof(int32_t) != 0)) {
                error = ESP_ERR_INVALID_SIZE;
            }
            if (error == ESP_OK) {
                for (size_t i = 0; i < bytes / sizeof(int32_t); ++i) {
                    s_recording[frames++] = raw[i] >> 16;
                }
            }
        }
        if (error != ESP_OK) {
            ESP_LOGE(TAG, "Audio test stopped: %s", esp_err_to_name(error));
            esp_err_t closed = close_channel(&mic);
            if (closed != ESP_OK) {
                ESP_LOGE(TAG, "Microphone cleanup: %s", esp_err_to_name(closed));
            }
            set_phase(AUDIO_ERROR);
            vTaskDelete(NULL);
        } else {
            set_phase(logic.phase);
        }
    }
}

esp_err_t audio_test_start(void) {
    s_recording = heap_caps_malloc(AUDIO_FRAMES * sizeof(int16_t),
                                   MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    ESP_RETURN_ON_FALSE(s_recording, ESP_ERR_NO_MEM, TAG, "96000-byte PSRAM audio buffer");
    s_buttons = xQueueCreate(1, sizeof(audio_input_t));
    if (!s_buttons) {
        free(s_recording);
        return ESP_ERR_NO_MEM;
    }
    esp_err_t error = speaker_pins_init();
    if (error != ESP_OK) {
        vQueueDelete(s_buttons);
        free(s_recording);
        return error;
    }
    BaseType_t created = xTaskCreate(audio_task, "audio", 6144, NULL, 5, NULL);
    if (created != pdPASS) {
        vQueueDelete(s_buttons);
        free(s_recording);
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "INMP441 SD=%d, MAX98357 DIN=%d SD_MODE=%d GAIN=%d; BCLK=%d WS=%d",
             MIC_SD_PIN, SPEAKER_DIN_PIN, SPEAKER_SD_MODE_PIN, SPEAKER_GAIN_PIN,
             AUDIO_BCLK_PIN, AUDIO_WS_PIN);
    return ESP_OK;
}

void audio_test_submit(const joystick_state_t *state) {
    audio_input_t input = {
        .button = state->button,
        .direction_x = state->direction_x,
    };
    xQueueOverwrite(s_buttons, &input);
}
