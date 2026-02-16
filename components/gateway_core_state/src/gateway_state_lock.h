#pragma once

#include "gateway_status.h"
#include "state_store.h"

typedef void *gateway_state_lock_t;

typedef struct {
    gateway_status_t (*create)(gateway_state_lock_t *out_lock);
    void (*destroy)(gateway_state_lock_t lock);
    void (*enter)(gateway_state_lock_t lock);
    void (*exit)(gateway_state_lock_t lock);
} gateway_state_lock_ops_t;

gateway_status_t gateway_state_lock_select_backend(gateway_state_lock_backend_t backend);
const gateway_state_lock_ops_t *gateway_state_lock_get_ops(void);

gateway_status_t gateway_state_lock_create(gateway_state_lock_t *out_lock);
void gateway_state_lock_destroy(gateway_state_lock_t lock);
void gateway_state_lock_enter(gateway_state_lock_t lock);
void gateway_state_lock_exit(gateway_state_lock_t lock);
