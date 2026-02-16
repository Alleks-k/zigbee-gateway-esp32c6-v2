#include "gateway_state_lock.h"

typedef struct {
    int initialized;
} gateway_state_lock_noop_t;

static gateway_status_t gateway_state_lock_create_noop(gateway_state_lock_t *out_lock)
{
    static gateway_state_lock_noop_t s_noop_lock = {
        .initialized = 1,
    };

    if (!out_lock) {
        return GATEWAY_STATUS_INVALID_ARG;
    }

    *out_lock = (gateway_state_lock_t)&s_noop_lock;
    return GATEWAY_STATUS_OK;
}

static void gateway_state_lock_destroy_noop(gateway_state_lock_t lock)
{
    (void)lock;
}

static void gateway_state_lock_enter_noop(gateway_state_lock_t lock)
{
    (void)lock;
}

static void gateway_state_lock_exit_noop(gateway_state_lock_t lock)
{
    (void)lock;
}

static const gateway_state_lock_ops_t s_gateway_state_lock_noop_ops = {
    .create = gateway_state_lock_create_noop,
    .destroy = gateway_state_lock_destroy_noop,
    .enter = gateway_state_lock_enter_noop,
    .exit = gateway_state_lock_exit_noop,
};

const gateway_state_lock_ops_t *gateway_state_lock_backend_noop_ops(void)
{
    return &s_gateway_state_lock_noop_ops;
}
