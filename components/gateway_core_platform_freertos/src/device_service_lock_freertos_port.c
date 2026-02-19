#include "device_service_lock_freertos_port.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static gateway_status_t device_service_lock_create_freertos(void *ctx, void **out_lock)
{
    (void)ctx;
    if (!out_lock) {
        return GATEWAY_STATUS_INVALID_ARG;
    }

    SemaphoreHandle_t lock = xSemaphoreCreateMutex();
    if (!lock) {
        return GATEWAY_STATUS_NO_MEM;
    }

    *out_lock = (void *)lock;
    return GATEWAY_STATUS_OK;
}

static void device_service_lock_destroy_freertos(void *ctx, void *lock)
{
    (void)ctx;
    if (!lock) {
        return;
    }
    vSemaphoreDelete((SemaphoreHandle_t)lock);
}

static void device_service_lock_enter_freertos(void *ctx, void *lock)
{
    (void)ctx;
    if (!lock) {
        return;
    }
    xSemaphoreTake((SemaphoreHandle_t)lock, portMAX_DELAY);
}

static void device_service_lock_exit_freertos(void *ctx, void *lock)
{
    (void)ctx;
    if (!lock) {
        return;
    }
    xSemaphoreGive((SemaphoreHandle_t)lock);
}

static const device_service_lock_port_t s_device_service_lock_port_freertos = {
    .create = device_service_lock_create_freertos,
    .destroy = device_service_lock_destroy_freertos,
    .enter = device_service_lock_enter_freertos,
    .exit = device_service_lock_exit_freertos,
    .ctx = NULL,
};

const device_service_lock_port_t *device_service_lock_port_freertos(void)
{
    return &s_device_service_lock_port_freertos;
}
