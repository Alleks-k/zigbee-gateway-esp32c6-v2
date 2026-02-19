#include "device_service_internal.h"

#include <stdbool.h>

gateway_status_t device_service_storage_save_locked(device_service_handle_t handle)
{
    if (!handle) {
        return GATEWAY_STATUS_INVALID_ARG;
    }
    if (!handle->repo_port || !handle->repo_port->save) {
        return GATEWAY_STATUS_INVALID_STATE;
    }

    return handle->repo_port->save(
        handle->repo_port->ctx, handle->devices, MAX_DEVICES, handle->device_count);
}

gateway_status_t device_service_storage_load_locked(device_service_handle_t handle)
{
    if (!handle) {
        return GATEWAY_STATUS_INVALID_ARG;
    }
    if (!handle->repo_port || !handle->repo_port->load) {
        return GATEWAY_STATUS_INVALID_STATE;
    }

    bool loaded = false;
    int count = 0;
    gateway_status_t status = handle->repo_port->load(
        handle->repo_port->ctx, handle->devices, MAX_DEVICES, &count, &loaded);
    if (status == GATEWAY_STATUS_OK && loaded) {
        handle->device_count = count;
    } else if (status == GATEWAY_STATUS_OK) {
        handle->device_count = 0;
    } else {
        handle->device_count = 0;
    }

    return status;
}
