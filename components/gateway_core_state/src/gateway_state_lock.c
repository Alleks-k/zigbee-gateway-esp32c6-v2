#include "gateway_state_lock.h"

#include <stdbool.h>

const gateway_state_lock_ops_t *gateway_state_lock_backend_freertos_ops(void);
#if CONFIG_GATEWAY_STATE_ALLOW_NOOP_LOCK_BACKEND
const gateway_state_lock_ops_t *gateway_state_lock_backend_noop_ops(void);
#endif

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

void gateway_state_lock_ctx_init(gateway_state_lock_ctx_t *ctx)
{
    if (!ctx) {
        return;
    }
    ctx->ops = NULL;
    ctx->backend_locked = false;
}

gateway_status_t gateway_state_lock_ctx_select_backend(gateway_state_lock_ctx_t *ctx, gateway_state_lock_backend_t backend)
{
    if (!ctx) {
        return GATEWAY_STATUS_INVALID_ARG;
    }

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

    if (ctx->backend_locked && ctx->ops != ops) {
        return GATEWAY_STATUS_INVALID_STATE;
    }

    ctx->ops = ops;
    return GATEWAY_STATUS_OK;
}

const gateway_state_lock_ops_t *gateway_state_lock_ctx_get_ops(gateway_state_lock_ctx_t *ctx)
{
    if (!ctx) {
        return NULL;
    }
    if (!ctx->ops) {
        ctx->ops = gateway_state_lock_backend_freertos_ops();
    }
    return ctx->ops;
}

gateway_status_t gateway_state_lock_ctx_create(gateway_state_lock_ctx_t *ctx, gateway_state_lock_t *out_lock)
{
    const gateway_state_lock_ops_t *ops = gateway_state_lock_ctx_get_ops(ctx);
    if (!ops) {
        return GATEWAY_STATUS_NOT_SUPPORTED;
    }
    if (!ops->create) {
        return GATEWAY_STATUS_FAIL;
    }

    gateway_status_t status = ops->create(out_lock);
    if (status == GATEWAY_STATUS_OK) {
        ctx->backend_locked = true;
    }
    return status;
}

void gateway_state_lock_ctx_destroy(gateway_state_lock_ctx_t *ctx, gateway_state_lock_t lock)
{
    const gateway_state_lock_ops_t *ops = gateway_state_lock_ctx_get_ops(ctx);
    if (!ops || !ops->destroy) {
        return;
    }
    ops->destroy(lock);
}

void gateway_state_lock_ctx_enter(gateway_state_lock_ctx_t *ctx, gateway_state_lock_t lock)
{
    const gateway_state_lock_ops_t *ops = gateway_state_lock_ctx_get_ops(ctx);
    if (!ops || !ops->enter) {
        return;
    }
    ops->enter(lock);
}

void gateway_state_lock_ctx_exit(gateway_state_lock_ctx_t *ctx, gateway_state_lock_t lock)
{
    const gateway_state_lock_ops_t *ops = gateway_state_lock_ctx_get_ops(ctx);
    if (!ops || !ops->exit) {
        return;
    }
    ops->exit(lock);
}
