#include "gateway_state_lock.h"

#include <stdbool.h>

const gateway_state_lock_ops_t *gateway_state_lock_backend_freertos_ops(void);
#if CONFIG_GATEWAY_STATE_ALLOW_NOOP_LOCK_BACKEND
const gateway_state_lock_ops_t *gateway_state_lock_backend_noop_ops(void);
#endif

static const gateway_state_lock_ops_t *s_lock_ops = NULL;
static bool s_backend_locked = false;

static const gateway_state_lock_ops_t *gateway_state_lock_backend_ops(gateway_state_lock_backend_t backend)
{
    switch (backend) {
    case GATEWAY_STATE_LOCK_BACKEND_FREERTOS:
        return gateway_state_lock_backend_freertos_ops();
    case GATEWAY_STATE_LOCK_BACKEND_NOOP:
#if CONFIG_GATEWAY_STATE_ALLOW_NOOP_LOCK_BACKEND
        return gateway_state_lock_backend_noop_ops();
#else
        return NULL;
#endif
    default:
        return NULL;
    }
}

gateway_status_t gateway_state_lock_select_backend(gateway_state_lock_backend_t backend)
{
    const gateway_state_lock_ops_t *ops = NULL;
    switch (backend) {
    case GATEWAY_STATE_LOCK_BACKEND_FREERTOS:
    case GATEWAY_STATE_LOCK_BACKEND_NOOP:
        ops = gateway_state_lock_backend_ops(backend);
        break;
    default:
        return GATEWAY_STATUS_INVALID_ARG;
    }
    if (!ops) {
        return GATEWAY_STATUS_NOT_SUPPORTED;
    }

    if (s_backend_locked && s_lock_ops != ops) {
        return GATEWAY_STATUS_INVALID_STATE;
    }

    s_lock_ops = ops;
    return GATEWAY_STATUS_OK;
}

const gateway_state_lock_ops_t *gateway_state_lock_get_ops(void)
{
    if (!s_lock_ops) {
        s_lock_ops = gateway_state_lock_backend_freertos_ops();
    }
    return s_lock_ops;
}

gateway_status_t gateway_state_lock_create(gateway_state_lock_t *out_lock)
{
    const gateway_state_lock_ops_t *ops = gateway_state_lock_get_ops();
    if (!ops) {
        return GATEWAY_STATUS_NOT_SUPPORTED;
    }
    if (!ops->create) {
        return GATEWAY_STATUS_FAIL;
    }

    gateway_status_t status = ops->create(out_lock);
    if (status == GATEWAY_STATUS_OK) {
        s_backend_locked = true;
    }
    return status;
}

void gateway_state_lock_destroy(gateway_state_lock_t lock)
{
    const gateway_state_lock_ops_t *ops = gateway_state_lock_get_ops();
    if (!ops || !ops->destroy) {
        return;
    }
    ops->destroy(lock);
}

void gateway_state_lock_enter(gateway_state_lock_t lock)
{
    const gateway_state_lock_ops_t *ops = gateway_state_lock_get_ops();
    if (!ops || !ops->enter) {
        return;
    }
    ops->enter(lock);
}

void gateway_state_lock_exit(gateway_state_lock_t lock)
{
    const gateway_state_lock_ops_t *ops = gateway_state_lock_get_ops();
    if (!ops || !ops->exit) {
        return;
    }
    ops->exit(lock);
}
