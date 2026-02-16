#include "gateway_state_lock.h"

#if defined(__has_include)
#if __has_include("freertos/FreeRTOS.h") && __has_include("freertos/semphr.h")
#define GATEWAY_STATE_FREERTOS_LOCK_AVAILABLE 1
#endif
#endif

#ifndef GATEWAY_STATE_FREERTOS_LOCK_AVAILABLE
#define GATEWAY_STATE_FREERTOS_LOCK_AVAILABLE 0
#endif

#if GATEWAY_STATE_FREERTOS_LOCK_AVAILABLE
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static gateway_status_t gateway_state_lock_create_freertos(gateway_state_lock_t *out_lock)
{
    if (!out_lock) {
        return GATEWAY_STATUS_INVALID_ARG;
    }

    SemaphoreHandle_t lock = xSemaphoreCreateMutex();
    if (!lock) {
        return GATEWAY_STATUS_NO_MEM;
    }

    *out_lock = (gateway_state_lock_t)lock;
    return GATEWAY_STATUS_OK;
}

static void gateway_state_lock_destroy_freertos(gateway_state_lock_t lock)
{
    if (!lock) {
        return;
    }
    vSemaphoreDelete((SemaphoreHandle_t)lock);
}

static void gateway_state_lock_enter_freertos(gateway_state_lock_t lock)
{
    if (!lock) {
        return;
    }
    xSemaphoreTake((SemaphoreHandle_t)lock, portMAX_DELAY);
}

static void gateway_state_lock_exit_freertos(gateway_state_lock_t lock)
{
    if (!lock) {
        return;
    }
    xSemaphoreGive((SemaphoreHandle_t)lock);
}

static const gateway_state_lock_ops_t s_gateway_state_lock_freertos_ops = {
    .create = gateway_state_lock_create_freertos,
    .destroy = gateway_state_lock_destroy_freertos,
    .enter = gateway_state_lock_enter_freertos,
    .exit = gateway_state_lock_exit_freertos,
};

const gateway_state_lock_ops_t *gateway_state_lock_backend_freertos_ops(void)
{
    return &s_gateway_state_lock_freertos_ops;
}
#else
const gateway_state_lock_ops_t *gateway_state_lock_backend_freertos_ops(void)
{
    return NULL;
}
#endif
