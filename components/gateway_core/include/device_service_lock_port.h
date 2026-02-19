#pragma once

#include "gateway_status.h"

typedef struct {
    gateway_status_t (*create)(void *ctx, void **out_lock);
    void (*destroy)(void *ctx, void *lock);
    void (*enter)(void *ctx, void *lock);
    void (*exit)(void *ctx, void *lock);
    void *ctx;
} device_service_lock_port_t;
