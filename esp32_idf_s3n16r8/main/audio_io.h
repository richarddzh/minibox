#pragma once
// Highest-priority audio I/O task pinned to Core 1.
//
// Drives I2S0 (ICS-43432 mic) during RECORD, then I2S1 (MAX98357A
// speaker) during PLAY. The two phases never overlap, so a single
// shared 64 KB DRAM arena is reused as either the mic ring (producer =
// audio task, consumer = main) or the speaker ring (producer = a Core-0
// worker, consumer = audio task). Recording resets the ring before REC;
// TTS explicitly clears and assigns it to speaker use before prefill.
// The underlying storage is never freed.
//
// Phase machine:
//   IDLE  --start_recording-->  REC   (audio task fills mic ring)
//   REC   --stop_recording -->  IDLE  (mic uninstalled, ring drained by main)
//   IDLE  --start_playback-->   PLAY  (audio task drains spk ring)
//   PLAY  --stop_playback -->   IDLE  (spk drained, DMA flushed, uninstalled)

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <esp_err.h>

typedef enum {
    AUDIO_PHASE_IDLE = 0,
    AUDIO_PHASE_REC  = 1,
    AUDIO_PHASE_PLAY = 2,
} audio_phase_t;

esp_err_t audio_io_init(void);
esp_err_t audio_io_quiet_speaker_pins(void);

esp_err_t audio_io_start_recording(uint32_t ack_timeout_ms);
esp_err_t audio_io_stop_recording(uint32_t ack_timeout_ms);
esp_err_t audio_io_start_playback(uint32_t ack_timeout_ms);
esp_err_t audio_io_stop_playback(uint32_t ack_timeout_ms);
void      audio_io_abort(uint32_t timeout_ms);
esp_err_t audio_io_prepare_speaker(uint32_t sample_rate, uint8_t channels);

// Main-side I/O. Block up to timeout_ms (0 = non-blocking).
size_t audio_io_read_mic(uint8_t *dst, size_t maxlen, uint32_t timeout_ms);
size_t audio_io_write_speaker(const uint8_t *src, size_t len, uint32_t timeout_ms);
size_t audio_io_reserve_speaker(uint8_t **dst, uint32_t timeout_ms);
void   audio_io_commit_speaker(size_t n);

audio_phase_t audio_io_phase(void);
bool      audio_io_mic_active(void);
bool      audio_io_spk_active(void);
uint32_t  audio_io_mic_overflow(void);
uint32_t  audio_io_spk_underrun(void);
size_t    audio_io_mic_available(void);
size_t    audio_io_spk_free(void);
