#include "mem_log.h"
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <esp_log.h>

static const char *TAG = "mem";

void mem_log(const char *tag) {
    size_t heap     = esp_get_free_heap_size();
    size_t min_heap = esp_get_minimum_free_heap_size();
    size_t internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    size_t dma_free = heap_caps_get_free_size(MALLOC_CAP_DMA);
    size_t dma_max  = heap_caps_get_largest_free_block(MALLOC_CAP_DMA);
    ESP_LOGI(TAG, "%s heap=%u min=%u int=%u dma=%u dma_max=%u",
             tag ? tag : "?",
             (unsigned)heap, (unsigned)min_heap, (unsigned)internal,
             (unsigned)dma_free, (unsigned)dma_max);
}
