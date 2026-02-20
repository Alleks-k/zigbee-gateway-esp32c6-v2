#pragma once

#include "gateway_status.h"
#include "state_store.h"
#include <stdbool.h>

typedef void *gateway_state_lock_t;

typedef struct {
    gateway_status_t (*create)(gateway_state_lock_t *out_lock);
    void (*destroy)(gateway_state_lock_t lock);
    void (*enter)(gateway_state_lock_t lock);
    void (*exit)(gateway_state_lock_t lock);
} gateway_state_lock_ops_t;

typedef struct {
    const gateway_state_lock_ops_t *ops;
    bool backend_locked;
} gateway_state_lock_ctx_t;

void gateway_state_lock_ctx_init(gateway_state_lock_ctx_t *ctx);
gateway_status_t gateway_state_lock_ctx_select_backend(gateway_state_lock_ctx_t *ctx, gateway_state_lock_backend_t backend);
const gateway_state_lock_ops_t *gateway_state_lock_ctx_get_ops(gateway_state_lock_ctx_t *ctx);

gateway_status_t gateway_state_lock_ctx_create(gateway_state_lock_ctx_t *ctx, gateway_state_lock_t *out_lock);
void gateway_state_lock_ctx_destroy(gateway_state_lock_ctx_t *ctx, gateway_state_lock_t lock);
void gateway_state_lock_ctx_enter(gateway_state_lock_ctx_t *ctx, gateway_state_lock_t lock);
void gateway_state_lock_ctx_exit(gateway_state_lock_ctx_t *ctx, gateway_state_lock_t lock);
