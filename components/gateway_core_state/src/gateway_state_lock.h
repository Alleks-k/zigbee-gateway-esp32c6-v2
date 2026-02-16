#pragma once

#include "gateway_status.h"

typedef void *gateway_state_lock_t;

gateway_status_t gateway_state_lock_create(gateway_state_lock_t *out_lock);
void gateway_state_lock_destroy(gateway_state_lock_t lock);
void gateway_state_lock_enter(gateway_state_lock_t lock);
void gateway_state_lock_exit(gateway_state_lock_t lock);
