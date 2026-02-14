#pragma once

#include <stddef.h>
#include <stdint.h>

#define GATEWAY_ERROR_RING_CAPACITY 10

typedef struct {
    uint64_t ts_ms;
    int32_t code;
    char source[8];
    char message[96];
} gateway_error_entry_t;

void gateway_error_ring_add(const char *source, int32_t code, const char *message);
size_t gateway_error_ring_snapshot(gateway_error_entry_t *out, size_t max_items);

