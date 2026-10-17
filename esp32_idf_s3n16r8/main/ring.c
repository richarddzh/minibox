#include "ring.h"
#include <string.h>

int ring_init(ring_t *r, uint8_t *storage, size_t capacity) {
    if (!r || !storage || capacity == 0 || (capacity & (capacity - 1)) != 0) {
        return -1;
    }
    r->buf = storage;
    r->cap = capacity;
    r->mask = capacity - 1;
    r->head = 0;
    r->tail = 0;
    return 0;
}

void ring_reset(ring_t *r) {
    r->head = 0;
    r->tail = 0;
    __sync_synchronize();
}

size_t ring_capacity(const ring_t *r) { return r->cap; }

size_t ring_available(const ring_t *r) {
    uint32_t h = r->head;
    uint32_t t = r->tail;
    return (size_t)(h - t);
}

size_t ring_free_space(const ring_t *r) {
    return r->cap - ring_available(r);
}

size_t ring_write_acquire(ring_t *r, uint8_t **dst) {
    if (!r || !dst) return 0;
    uint32_t head = r->head;
    uint32_t tail = r->tail;
    size_t free_bytes = r->cap - (size_t)(head - tail);
    if (free_bytes == 0) return 0;
    size_t pos = head & r->mask;
    size_t first = r->cap - pos;
    if (first > free_bytes) first = free_bytes;
    *dst = r->buf + pos;
    return first;
}

void ring_write_commit(ring_t *r, size_t n) {
    if (!r || n == 0) return;
    uint32_t head = r->head;
    uint32_t tail = r->tail;
    size_t free_bytes = r->cap - (size_t)(head - tail);
    if (n > free_bytes) n = free_bytes;
    size_t pos = head & r->mask;
    size_t first = r->cap - pos;
    if (n > first) n = first;
    __sync_synchronize();
    r->head = head + (uint32_t)n;
}

size_t ring_write(ring_t *r, const uint8_t *src, size_t n) {
    uint32_t head = r->head;
    uint32_t tail = r->tail;
    size_t free_bytes = r->cap - (size_t)(head - tail);
    if (n > free_bytes) n = free_bytes;
    if (n == 0) return 0;
    size_t pos = head & r->mask;
    size_t first = r->cap - pos;
    if (first > n) first = n;
    memcpy(r->buf + pos, src, first);
    if (n > first) memcpy(r->buf, src + first, n - first);
    __sync_synchronize();
    r->head = head + (uint32_t)n;
    return n;
}

size_t ring_read(ring_t *r, uint8_t *dst, size_t n) {
    uint32_t head = r->head;
    uint32_t tail = r->tail;
    __sync_synchronize();
    size_t avail = (size_t)(head - tail);
    if (n > avail) n = avail;
    if (n == 0) return 0;
    size_t pos = tail & r->mask;
    size_t first = r->cap - pos;
    if (first > n) first = n;
    memcpy(dst, r->buf + pos, first);
    if (n > first) memcpy(dst + first, r->buf, n - first);
    __sync_synchronize();
    r->tail = tail + (uint32_t)n;
    return n;
}
