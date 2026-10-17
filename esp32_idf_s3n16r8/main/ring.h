#pragma once
// Single-producer / single-consumer byte ring backed by caller-supplied
// storage. Capacity must be a power of two so masking replaces modulo.
//
// Synchronisation: head/tail are 32-bit volatile counters. Only the
// producer mutates head_, only the consumer mutates tail_. Word-aligned
// 32-bit reads/writes are atomic on Xtensa LX6, and __sync_synchronize()
// is used as a full memory barrier so the producer's data write is
// visible before the consumer sees the updated head.
//
// reset() must only be called when neither side is running (typically
// during phase transitions in audio_io).

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t  *buf;
    size_t    cap;
    size_t    mask;
    volatile uint32_t head;
    volatile uint32_t tail;
} ring_t;

// Initialise with externally-owned storage. capacity must be power-of-two
// and >0. Returns 0 on success, -1 on bad arguments.
int  ring_init(ring_t *r, uint8_t *storage, size_t capacity);

void ring_reset(ring_t *r);
size_t ring_capacity(const ring_t *r);
size_t ring_available(const ring_t *r);
size_t ring_free_space(const ring_t *r);

// Producer side. Returns bytes actually written (<=n).
size_t ring_write(ring_t *r, const uint8_t *src, size_t n);
// Producer-side zero-copy path: acquire a contiguous writable span, then
// publish the bytes actually filled with ring_write_commit().
size_t ring_write_acquire(ring_t *r, uint8_t **dst);
void   ring_write_commit(ring_t *r, size_t n);
// Consumer side. Returns bytes actually read (<=n).
size_t ring_read(ring_t *r, uint8_t *dst, size_t n);
