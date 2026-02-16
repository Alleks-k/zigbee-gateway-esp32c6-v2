#include "device_service_internal.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

gateway_status_t device_service_lock_ensure(device_service_handle_t handle)
{
    if (!handle) {
        return GATEWAY_STATUS_INVALID_ARG;
    }
    if (!handle->devices_mutex) {
        handle->devices_mutex = xSemaphoreCreateMutex();
        if (!handle->devices_mutex) {
            return GATEWAY_STATUS_NO_MEM;
        }
    }
    return GATEWAY_STATUS_OK;
}

void device_service_lock_destroy(device_service_handle_t handle)
{
    if (!handle || !handle->devices_mutex) {
        return;
    }
    vSemaphoreDelete((SemaphoreHandle_t)handle->devices_mutex);
    handle->devices_mutex = NULL;
}

void device_service_lock_acquire(device_service_handle_t handle)
{
    if (!handle || !handle->devices_mutex) {
        return;
    }
    xSemaphoreTake((SemaphoreHandle_t)handle->devices_mutex, portMAX_DELAY);
}

void device_service_lock_release(device_service_handle_t handle)
{
    if (!handle || !handle->devices_mutex) {
        return;
    }
    xSemaphoreGive((SemaphoreHandle_t)handle->devices_mutex);
}
