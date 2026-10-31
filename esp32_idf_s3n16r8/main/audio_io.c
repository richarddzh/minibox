#include "audio_io.h"
#include "app_config.h"
#include "ring.h"
#include "mem_log.h"

#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <driver/gpio.h>
#include <driver/i2s_std.h>

static const char *TAG = "audio_io";

#define MIC_PORT    I2S_NUM_0
#define SPK_PORT    I2S_NUM_0
#define POLL_MS     2
// Pin audio to core 1: ESP-IDF Wi-Fi/lwIP and the default app_main task
// both live on core 0, so isolating I2S DMA work on core 1 actually
// removes contention rather than (as in the Arduino version) creating it.
#define AUDIO_TASK_CORE 1
// Give audio the highest FreeRTOS task priority so Core 1 work cannot
// preempt mic/speaker servicing. Interrupts can still run above tasks.
#define AUDIO_TASK_PRIO (configMAX_PRIORITIES - 1)

enum {
    REQ_NONE       = 0,
    REQ_START_REC  = 1,
    REQ_START_PLAY = 2,
};

enum {
    RING_ROLE_NONE = 0,
    RING_ROLE_MIC  = 1,
    RING_ROLE_SPK  = 2,
};

// --- Shared arena (mic ring during REC, speaker ring during PLAY) -------
/* Software ring is copied into I2S DMA buffers; it need not occupy DMA RAM. */
static uint8_t *s_arena;
static ring_t   s_ring;

// --- Volatile request/state flags ---------------------------------------
static volatile uint8_t  s_phase     = AUDIO_PHASE_IDLE;
static volatile uint8_t  s_ring_role = RING_ROLE_NONE;
static volatile uint8_t  s_req_start = REQ_NONE;
static volatile uint8_t  s_req_stop  = 0;
static volatile uint8_t  s_req_abort = 0;
static volatile esp_err_t s_play_start_result = ESP_OK;
static volatile uint32_t s_mic_overflow_chunks = 0;
static volatile uint32_t s_spk_underrun = 0;
static volatile uint32_t s_spk_sample_rate = APP_VOICE_SAMPLE_RATE;
static volatile uint8_t  s_spk_channels = 1;
static bool s_spk_realtime;

#define REALTIME_DMA_DESCRIPTORS 3
#define REALTIME_DMA_FRAMES 256

// --- Reusable scratch buffers (kept off the task stack) -----------------
// 与下面 install_mic / install_speaker 的 dma_frame_num 对齐：每次
// i2s_channel_read 直接吃掉一整个 DMA 描述符，少做 syscall。
static int32_t s_i2s_read_buf[512];     // 2 KB mic DMA staging (512 frame * 4 B)
static int16_t s_decimate_buf[512];     // 16 kHz INMP441 -> signed 16-bit PCM
static uint8_t s_i2s_write_buf[2048] __attribute__((aligned(4)));

// --- I2S handles --------------------------------------------------------
static i2s_chan_handle_t s_mic_chan;
static i2s_chan_handle_t s_spk_chan;
static bool              s_mic_installed;
static bool              s_spk_installed;

static TaskHandle_t      s_task;

static inline int64_t now_ms(void) { return esp_timer_get_time() / 1000; }

esp_err_t audio_io_quiet_speaker_pins(void) {
    esp_err_t err = gpio_set_level(SPEAKER_SD_MODE_PIN, 0);
    if (err != ESP_OK) return err;
    const gpio_config_t shutdown = {
        .pin_bit_mask = 1ULL << SPEAKER_SD_MODE_PIN,
        .mode = GPIO_MODE_OUTPUT,
    };
    err = gpio_config(&shutdown);
    if (err != ESP_OK) return err;
    const gpio_config_t gain = {
        .pin_bit_mask = 1ULL << SPEAKER_GAIN_PIN,
#ifdef CONFIG_MINIBOX_AMP_GAIN_12DB
        .mode = GPIO_MODE_OUTPUT,
#else
        .mode = GPIO_MODE_INPUT,
#endif
    };
#ifdef CONFIG_MINIBOX_AMP_GAIN_12DB
    err = gpio_set_level(SPEAKER_GAIN_PIN, 0);
    if (err != ESP_OK) return err;
#endif
    err = gpio_config(&gain);
    if (err != ESP_OK) return err;
    const uint64_t pin_mask = (1ULL << APP_SPK_BCLK_PIN) |
                              (1ULL << APP_SPK_LRCK_PIN) |
                              (1ULL << APP_SPK_DATA_PIN);
    gpio_set_level(APP_SPK_BCLK_PIN, 0);
    gpio_set_level(APP_SPK_LRCK_PIN, 0);
    gpio_set_level(APP_SPK_DATA_PIN, 0);
    const gpio_config_t cfg = {
        .pin_bit_mask = pin_mask,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    return gpio_config(&cfg);
}

static esp_err_t install_mic(void) {
    if (s_mic_installed) return ESP_OK;
    if (s_spk_installed) return ESP_ERR_INVALID_STATE;
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(MIC_PORT, I2S_ROLE_MASTER);
    // Six 512-frame descriptors retain 192 ms at 16 kHz.
    chan_cfg.dma_desc_num = 6;
    chan_cfg.dma_frame_num = 512;
    chan_cfg.auto_clear = false;
    esp_err_t err = i2s_new_channel(&chan_cfg, NULL, &s_mic_chan);
    if (err != ESP_OK) { ESP_LOGE(TAG, "mic new_channel: %s", esp_err_to_name(err)); return err; }

    i2s_std_config_t std = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(APP_MIC_CAPTURE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT,
                                                       I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = APP_MIC_BCLK_PIN,
            .ws   = APP_MIC_LRCK_PIN,
            .dout = I2S_GPIO_UNUSED,
            .din  = APP_MIC_DATA_PIN,
        },
    };
    std.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;
    err = i2s_channel_init_std_mode(s_mic_chan, &std);
    if (err != ESP_OK) { i2s_del_channel(s_mic_chan); s_mic_chan = NULL;
        ESP_LOGE(TAG, "mic init_std: %s", esp_err_to_name(err)); return err; }
    err = i2s_channel_enable(s_mic_chan);
    if (err != ESP_OK) { i2s_del_channel(s_mic_chan); s_mic_chan = NULL; return err; }
    s_mic_installed = true;
    mem_log("mic install");
    return ESP_OK;
}

static void uninstall_mic(void) {
    if (!s_mic_installed) return;
    i2s_channel_disable(s_mic_chan);
    i2s_del_channel(s_mic_chan);
    s_mic_chan = NULL;
    s_mic_installed = false;
    ESP_ERROR_CHECK(audio_io_quiet_speaker_pins());
}

static esp_err_t install_speaker(void) {
    if (s_spk_installed) return ESP_OK;
    if (s_mic_installed) return ESP_ERR_INVALID_STATE;
    uint32_t sample_rate = s_spk_sample_rate;
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(SPK_PORT, I2S_ROLE_MASTER);
    // Eight 512-frame descriptors retain about 93 ms at 44.1 kHz stereo.
    chan_cfg.dma_desc_num = s_spk_realtime ? REALTIME_DMA_DESCRIPTORS : 8;
    chan_cfg.dma_frame_num = s_spk_realtime ? REALTIME_DMA_FRAMES : 512;
    chan_cfg.auto_clear = true;
    esp_err_t err = i2s_new_channel(&chan_cfg, &s_spk_chan, NULL);
    if (err != ESP_OK) { ESP_LOGE(TAG, "spk new_channel: %s", esp_err_to_name(err)); return err; }

    i2s_std_config_t std = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(sample_rate),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                                                       I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = APP_SPK_BCLK_PIN,
            .ws   = APP_SPK_LRCK_PIN,
            .dout = APP_SPK_DATA_PIN,
            .din  = I2S_GPIO_UNUSED,
        },
    };
    err = i2s_channel_init_std_mode(s_spk_chan, &std);
    if (err != ESP_OK) { i2s_del_channel(s_spk_chan); s_spk_chan = NULL;
        audio_io_quiet_speaker_pins();
        ESP_LOGE(TAG, "spk init_std: %s", esp_err_to_name(err)); return err; }
    err = i2s_channel_enable(s_spk_chan);
    if (err != ESP_OK) { i2s_del_channel(s_spk_chan); s_spk_chan = NULL;
        audio_io_quiet_speaker_pins(); return err; }
    memset(s_i2s_write_buf, 0, sizeof(s_i2s_write_buf));
    size_t silence_written = 0;
    err = i2s_channel_write(s_spk_chan, s_i2s_write_buf,
                            sizeof(s_i2s_write_buf), &silence_written,
                            portMAX_DELAY);
    if (err != ESP_OK || silence_written != sizeof(s_i2s_write_buf)) {
        i2s_channel_disable(s_spk_chan);
        i2s_del_channel(s_spk_chan);
        s_spk_chan = NULL;
        audio_io_quiet_speaker_pins();
        return err != ESP_OK ? err : ESP_FAIL;
    }
    s_spk_installed = true;
    err = gpio_set_level(SPEAKER_SD_MODE_PIN, 1);
    if (err != ESP_OK) {
        i2s_channel_disable(s_spk_chan);
        i2s_del_channel(s_spk_chan);
        s_spk_chan = NULL;
        s_spk_installed = false;
        ESP_ERROR_CHECK(audio_io_quiet_speaker_pins());
        return err;
    }
    mem_log("spk install");
    return ESP_OK;
}

static void uninstall_speaker(void) {
    if (!s_spk_installed) return;
    ESP_ERROR_CHECK(gpio_set_level(SPEAKER_SD_MODE_PIN, 0));
    i2s_channel_disable(s_spk_chan);
    i2s_del_channel(s_spk_chan);
    s_spk_chan = NULL;
    s_spk_installed = false;
    ESP_ERROR_CHECK(audio_io_quiet_speaker_pins());
}

// --- Recording phase: reuse arena as mic ring; producer = audio task ----
static void do_recording(void) {
    if (install_mic() != ESP_OK) {
        s_req_stop = 0;
        s_req_abort = 0;
        __sync_synchronize();
        s_phase = AUDIO_PHASE_IDLE;
        return;
    }
    ring_reset(&s_ring);
    s_ring_role = RING_ROLE_MIC;
    s_mic_overflow_chunks = 0;
    __sync_synchronize();
    s_phase = AUDIO_PHASE_REC;
    ESP_LOGI(TAG, "REC start");

    while (!s_req_stop && !s_req_abort) {
        size_t bytes_read = 0;
        esp_err_t e = i2s_channel_read(s_mic_chan, s_i2s_read_buf,
                                       sizeof(s_i2s_read_buf),
                                       &bytes_read, pdMS_TO_TICKS(20));
        if (e == ESP_ERR_TIMEOUT) continue;
        if (e != ESP_OK || bytes_read == 0) {
            ESP_LOGE(TAG, "Microphone read: %s", esp_err_to_name(e));
            break;
        }

        size_t in_samples  = bytes_read / sizeof(int32_t);
        size_t out_samples = in_samples;

        for (size_t i = 0; i < out_samples; ++i) {
            int32_t mixed = (s_i2s_read_buf[i] >> 16) * APP_MIC_DIGITAL_GAIN;
            if (mixed >  INT16_MAX) mixed =  INT16_MAX;
            if (mixed <  INT16_MIN) mixed =  INT16_MIN;
            s_decimate_buf[i] = (int16_t)mixed;
        }

        size_t out_bytes = out_samples * sizeof(int16_t);
        if (ring_free_space(&s_ring) < out_bytes) {
            s_mic_overflow_chunks++;
            continue;                  // drop whole chunk; preserve alignment
        }
        ring_write(&s_ring, (const uint8_t *)s_decimate_buf, out_bytes);
    }

    uninstall_mic();
    s_req_stop = 0;
    s_req_abort = 0;
    __sync_synchronize();
    s_phase = AUDIO_PHASE_IDLE;
    ESP_LOGI(TAG, "REC stop overflow_chunks=%u", (unsigned)s_mic_overflow_chunks);
    mem_log("REC stop");
}

static esp_err_t write_speaker_chunk(size_t bytes, size_t *total) {
    size_t output_bytes = bytes;
    if (s_spk_channels == 1) {
        int16_t *pcm = (int16_t *)s_i2s_write_buf;
        size_t samples = bytes / sizeof(int16_t);
        for (size_t i = samples; i > 0; --i) {
            pcm[2 * (i - 1)] = pcm[i - 1];
            pcm[2 * (i - 1) + 1] = pcm[i - 1];
        }
        output_bytes *= 2;
    }
    size_t written = 0;
    esp_err_t err = i2s_channel_write(s_spk_chan, s_i2s_write_buf,
                                      output_bytes, &written, 1000);
    if (err == ESP_OK && written != output_bytes) err = ESP_ERR_TIMEOUT;
    if (err != ESP_OK) ESP_LOGE(TAG, "Speaker write: %s", esp_err_to_name(err));
    *total += written;
    return err;
}

static size_t read_speaker_chunk(size_t capacity) {
    size_t frame_bytes = s_spk_channels * sizeof(int16_t);
    size_t available = ring_available(&s_ring);
    /* Preserve the remainder between NES frames until a DMA block is full.
     * A partial block can be reclaimed by I2S before the next frame arrives. */
    if (s_spk_realtime && !s_req_stop && available < capacity) return 0;
    if (available > capacity) available = capacity;
    return ring_read(&s_ring, s_i2s_write_buf, available - available % frame_bytes);
}

// --- Playback phase: reuse arena as speaker ring; consumer = audio task -
static void do_playback(void) {
    esp_err_t start_result = install_speaker();
    if (start_result != ESP_OK) {
        s_req_stop = 0;
        s_req_abort = 0;
        __sync_synchronize();
        s_phase = AUDIO_PHASE_IDLE;
        __sync_synchronize();
        s_play_start_result = start_result;
        return;
    }
    s_spk_underrun = 0;
    __sync_synchronize();
    s_phase = AUDIO_PHASE_PLAY;
    __sync_synchronize();
    s_play_start_result = ESP_OK;
    ESP_LOGI(TAG, "PLAY start rate=%u channels=%u",
             (unsigned)s_spk_sample_rate, (unsigned)s_spk_channels);

    int64_t last_data_ms = now_ms();
    size_t  total_bytes = 0;
    size_t chunk_size = sizeof(s_i2s_write_buf) / (s_spk_channels == 1 ? 2 : 1);
    if (s_spk_realtime) chunk_size = REALTIME_DMA_FRAMES * s_spk_channels * sizeof(int16_t);
    uint32_t starvation_ms = s_spk_realtime
        ? (REALTIME_DMA_DESCRIPTORS * REALTIME_DMA_FRAMES * 1000U +
           s_spk_sample_rate - 1) / s_spk_sample_rate : 5;

    while (!s_req_abort) {
        size_t got = read_speaker_chunk(chunk_size);
        if (got > 0) {
            if (write_speaker_chunk(got, &total_bytes) != ESP_OK) break;
            last_data_ms = now_ms();
            continue;
        }
        if (s_req_stop) {
            // Drain any leftover that arrived between read and stop check.
            got = read_speaker_chunk(chunk_size);
            if (got > 0) {
                if (write_speaker_chunk(got, &total_bytes) != ESP_OK) break;
                continue;
            }
            if (ring_available(&s_ring)) {
                ESP_LOGE(TAG, "Truncated PCM frame at end of playback");
            }
            break;
        }
        if (now_ms() - last_data_ms > starvation_ms) {
            s_spk_underrun++;
            last_data_ms = now_ms();
        }
        vTaskDelay(pdMS_TO_TICKS(POLL_MS));
    }

    // End with digital silence before releasing the I2S pins.
    memset(s_i2s_write_buf, 0, sizeof(s_i2s_write_buf));
    size_t silence_written = 0;
    i2s_channel_write(s_spk_chan, s_i2s_write_buf,
                      sizeof(s_i2s_write_buf), &silence_written,
                      portMAX_DELAY);
    // i2s_channel_write returns once data is queued; wait for DMA drain.
    vTaskDelay(pdMS_TO_TICKS(350));
    uninstall_speaker();
    s_req_stop = 0;
    s_req_abort = 0;
    __sync_synchronize();
    s_phase = AUDIO_PHASE_IDLE;
    ESP_LOGI(TAG, "PLAY stop bytes=%u underrun=%u",
             (unsigned)total_bytes, (unsigned)s_spk_underrun);
    mem_log("PLAY stop");
}

static void task_run(void *arg) {
    (void)arg;
    ESP_LOGI(TAG, "audio task running on core %d", xPortGetCoreID());
    while (1) {
        while (s_req_start == REQ_NONE) {
            vTaskDelay(pdMS_TO_TICKS(POLL_MS));
        }
        uint8_t r = s_req_start;
        s_req_start = REQ_NONE;
        __sync_synchronize();
        // A timeout/abort can race with this task consuming the start
        // request. Never erase that cancellation after taking the request.
        if (s_req_stop || s_req_abort) {
            s_req_stop = 0;
            s_req_abort = 0;
            __sync_synchronize();
            s_phase = AUDIO_PHASE_IDLE;
            if (r == REQ_START_PLAY) {
                __sync_synchronize();
                s_play_start_result = ESP_ERR_INVALID_STATE;
            }
            continue;
        }
        if      (r == REQ_START_REC)  do_recording();
        else if (r == REQ_START_PLAY) do_playback();
    }
}

// --- Public API ---------------------------------------------------------

esp_err_t audio_io_init(void) {
    if (s_task) return ESP_OK;
    esp_err_t err = audio_io_quiet_speaker_pins();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "speaker pin quiet: %s", esp_err_to_name(err));
        return err;
    }
    s_arena = heap_caps_malloc(APP_AUDIO_ARENA_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_arena) {
        ESP_LOGE(TAG, "PSRAM audio arena allocation failed");
        return ESP_ERR_NO_MEM;
    }
    if (ring_init(&s_ring, s_arena, APP_AUDIO_ARENA_BYTES) != 0) {
        heap_caps_free(s_arena);
        s_arena = NULL;
        ESP_LOGE(TAG, "ring init failed");
        return ESP_FAIL;
    }
    BaseType_t ok = xTaskCreatePinnedToCore(task_run, "audio_io",
                                            4096, NULL, AUDIO_TASK_PRIO, &s_task,
                                            AUDIO_TASK_CORE);
    if (ok != pdPASS) {
        heap_caps_free(s_arena);
        s_arena = NULL;
        ESP_LOGE(TAG, "task create");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "audio task started core=%d prio=%d arena=%u B",
             AUDIO_TASK_CORE, AUDIO_TASK_PRIO, (unsigned)APP_AUDIO_ARENA_BYTES);
    mem_log("audio_io_init");
    return ESP_OK;
}

static esp_err_t wait_phase(audio_phase_t target, uint32_t timeout_ms) {
    int64_t t0 = now_ms();
    while (s_phase != target) {
        if (now_ms() - t0 > timeout_ms) return ESP_ERR_TIMEOUT;
        vTaskDelay(pdMS_TO_TICKS(POLL_MS));
    }
    return ESP_OK;
}

esp_err_t audio_io_start_recording(uint32_t ack_timeout_ms) {
    if (!s_task) return ESP_ERR_INVALID_STATE;
    if (s_phase != AUDIO_PHASE_IDLE || s_req_start != REQ_NONE) {
        return ESP_ERR_INVALID_STATE;
    }
    s_req_stop = 0;
    s_req_abort = 0;
    __sync_synchronize();
    s_req_start = REQ_START_REC;
    esp_err_t e = wait_phase(AUDIO_PHASE_REC, ack_timeout_ms);
    if (e != ESP_OK) {
        // Either the request was never picked up or it picked up but
        // failed during install. Either way, force back to IDLE so the
        // next attempt won't be rejected.
        s_req_start = REQ_NONE;
        audio_io_abort(2000);
    }
    return e;
}

esp_err_t audio_io_stop_recording(uint32_t ack_timeout_ms) {
    if (s_phase == AUDIO_PHASE_IDLE) return ESP_OK;
    if (s_phase != AUDIO_PHASE_REC) return ESP_ERR_INVALID_STATE;
    s_req_stop = 1;
    __sync_synchronize();
    return wait_phase(AUDIO_PHASE_IDLE, ack_timeout_ms);
}

esp_err_t audio_io_start_playback(uint32_t ack_timeout_ms) {
    if (!s_task) return ESP_ERR_INVALID_STATE;
    if (s_phase != AUDIO_PHASE_IDLE || s_req_start != REQ_NONE ||
        s_ring_role != RING_ROLE_SPK) {
        return ESP_ERR_INVALID_STATE;
    }
    s_req_stop = 0;
    s_req_abort = 0;
    s_play_start_result = ESP_ERR_NOT_FINISHED;
    __sync_synchronize();
    s_req_start = REQ_START_PLAY;
    int64_t start_ms = now_ms();
    while (s_play_start_result == ESP_ERR_NOT_FINISHED &&
           now_ms() - start_ms < ack_timeout_ms) {
        vTaskDelay(pdMS_TO_TICKS(POLL_MS));
    }
    esp_err_t e = s_play_start_result;
    if (e == ESP_ERR_NOT_FINISHED) e = ESP_ERR_TIMEOUT;
    if (e != ESP_OK) {
        s_req_start = REQ_NONE;
        audio_io_abort(2000);
    }
    return e;
}

esp_err_t audio_io_stop_playback(uint32_t ack_timeout_ms) {
    if (s_phase == AUDIO_PHASE_IDLE) return ESP_OK;
    if (s_phase != AUDIO_PHASE_PLAY) return ESP_ERR_INVALID_STATE;
    s_req_stop = 1;
    __sync_synchronize();
    return wait_phase(AUDIO_PHASE_IDLE, ack_timeout_ms);
}

void audio_io_abort(uint32_t timeout_ms) {
    s_req_start = REQ_NONE;
    s_req_abort = 1;
    s_req_stop  = 1;
    __sync_synchronize();
    wait_phase(AUDIO_PHASE_IDLE, timeout_ms);
}

esp_err_t audio_io_prepare_speaker(uint32_t sample_rate, uint8_t channels) {
    if (sample_rate < 8000 || sample_rate > 96000 ||
        (channels != 1 && channels != 2)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_phase != AUDIO_PHASE_IDLE || s_req_start != REQ_NONE) {
        return ESP_ERR_INVALID_STATE;
    }
    s_spk_sample_rate = sample_rate;
    s_spk_channels = channels;
    s_spk_realtime = false;
    ring_reset(&s_ring);
    s_ring_role = RING_ROLE_SPK;
    __sync_synchronize();
    return ESP_OK;
}

esp_err_t audio_io_prepare_realtime_speaker(uint32_t sample_rate, uint8_t channels) {
    esp_err_t err = audio_io_prepare_speaker(sample_rate, channels);
    if (err == ESP_OK) s_spk_realtime = true;
    return err;
}

size_t audio_io_read_mic(uint8_t *dst, size_t maxlen, uint32_t timeout_ms) {
    if (s_ring_role != RING_ROLE_MIC) return 0;
    if (timeout_ms == 0) return ring_read(&s_ring, dst, maxlen);
    int64_t t0 = now_ms();
    for (;;) {
        size_t n = ring_read(&s_ring, dst, maxlen);
        if (n) return n;
        if ((uint32_t)(now_ms() - t0) >= timeout_ms) return 0;
        vTaskDelay(1);
    }
}

size_t audio_io_write_speaker(const uint8_t *src, size_t len, uint32_t timeout_ms) {
    if (s_ring_role != RING_ROLE_SPK ||
        (s_phase != AUDIO_PHASE_IDLE && s_phase != AUDIO_PHASE_PLAY)) {
        return 0;
    }
    size_t total = 0;
    int64_t t0 = now_ms();
    while (total < len) {
        size_t pushed = ring_write(&s_ring, src + total, len - total);
        if (pushed) { total += pushed; t0 = now_ms(); continue; }
        if (timeout_ms == 0 || (uint32_t)(now_ms() - t0) >= timeout_ms) break;
        vTaskDelay(1);
    }
    return total;
}

size_t audio_io_reserve_speaker(uint8_t **dst, uint32_t timeout_ms) {
    if (!dst) return 0;
    if (s_ring_role != RING_ROLE_SPK ||
        (s_phase != AUDIO_PHASE_IDLE && s_phase != AUDIO_PHASE_PLAY)) {
        return 0;
    }
    int64_t t0 = now_ms();
    for (;;) {
        size_t n = ring_write_acquire(&s_ring, dst);
        if (n) return n;
        if (s_ring_role != RING_ROLE_SPK ||
            (s_phase != AUDIO_PHASE_IDLE && s_phase != AUDIO_PHASE_PLAY)) {
            return 0;
        }
        if (timeout_ms == 0 || (uint32_t)(now_ms() - t0) >= timeout_ms) return 0;
        vTaskDelay(1);
    }
}

void audio_io_commit_speaker(size_t n) {
    if (n == 0) return;
    if (s_ring_role != RING_ROLE_SPK ||
        (s_phase != AUDIO_PHASE_IDLE && s_phase != AUDIO_PHASE_PLAY)) {
        return;
    }
    ring_write_commit(&s_ring, n);
}

audio_phase_t audio_io_phase(void)     { return (audio_phase_t)s_phase; }
bool     audio_io_mic_active(void)     { return s_phase == AUDIO_PHASE_REC; }
bool     audio_io_spk_active(void)     { return s_phase == AUDIO_PHASE_PLAY; }
uint32_t audio_io_mic_overflow(void)   { return s_mic_overflow_chunks; }
uint32_t audio_io_spk_underrun(void)   { return s_spk_underrun; }
size_t audio_io_mic_available(void) {
    return s_ring_role == RING_ROLE_MIC ? ring_available(&s_ring) : 0;
}
size_t audio_io_spk_free(void) {
    return s_ring_role == RING_ROLE_SPK ? ring_free_space(&s_ring) : 0;
}
size_t audio_io_spk_pending(void) {
    return s_ring_role == RING_ROLE_SPK ? ring_available(&s_ring) : 0;
}
