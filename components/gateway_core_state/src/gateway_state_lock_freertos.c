#include "gateway_state_lock.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

gateway_status_t gateway_state_lock_create(gateway_state_lock_t *out_lock)
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

void gateway_state_lock_destroy(gateway_state_lock_t lock)
{
    if (!lock) {
        return;
    }
    vSemaphoreDelete((SemaphoreHandle_t)lock);
}

void gateway_state_lock_enter(gateway_state_lock_t lock)
{
    if (!lock) {
        return;
    }
    xSemaphoreTake((SemaphoreHandle_t)lock, portMAX_DELAY);
}

void gateway_state_lock_exit(gateway_state_lock_t lock)
{
    if (!lock) {
        return;
    }
    xSemaphoreGive((SemaphoreHandle_t)lock);
}
