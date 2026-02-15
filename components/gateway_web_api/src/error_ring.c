#include "error_ring.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include <string.h>

static gateway_error_entry_t s_ring[GATEWAY_ERROR_RING_CAPACITY];
static size_t s_ring_head = 0;
static size_t s_ring_count = 0;
static portMUX_TYPE s_ring_lock = portMUX_INITIALIZER_UNLOCKED;
static gateway_error_ring_now_ms_provider_t s_now_ms_provider = NULL;
static uint64_t s_fallback_now_ms = 0;

static uint64_t now_ms(void)
{
    if (s_now_ms_provider) {
        return s_now_ms_provider();
    }
    return ++s_fallback_now_ms;
}

void gateway_error_ring_set_now_ms_provider(gateway_error_ring_now_ms_provider_t provider)
{
    portENTER_CRITICAL(&s_ring_lock);
    s_now_ms_provider = provider;
    portEXIT_CRITICAL(&s_ring_lock);
}

void gateway_error_ring_add(const char *source, int32_t code, const char *message)
{
    gateway_error_entry_t entry = {0};
    entry.ts_ms = now_ms();
    entry.code = code;
    if (source) {
        strlcpy(entry.source, source, sizeof(entry.source));
    } else {
        strlcpy(entry.source, "sys", sizeof(entry.source));
    }
    if (message) {
        strlcpy(entry.message, message, sizeof(entry.message));
    } else {
        strlcpy(entry.message, "unknown", sizeof(entry.message));
    }

    portENTER_CRITICAL(&s_ring_lock);
    s_ring[s_ring_head] = entry;
    s_ring_head = (s_ring_head + 1) % GATEWAY_ERROR_RING_CAPACITY;
    if (s_ring_count < GATEWAY_ERROR_RING_CAPACITY) {
        s_ring_count++;
    }
    portEXIT_CRITICAL(&s_ring_lock);
}

size_t gateway_error_ring_snapshot(gateway_error_entry_t *out, size_t max_items)
{
    if (!out || max_items == 0) {
        return 0;
    }

    size_t copied = 0;
    portENTER_CRITICAL(&s_ring_lock);
    size_t count = s_ring_count;
    if (count > max_items) {
        count = max_items;
    }
    for (size_t i = 0; i < count; i++) {
        size_t idx = (s_ring_head + GATEWAY_ERROR_RING_CAPACITY - 1 - i) % GATEWAY_ERROR_RING_CAPACITY;
        out[copied++] = s_ring[idx];
    }
    portEXIT_CRITICAL(&s_ring_lock);
    return copied;
}
