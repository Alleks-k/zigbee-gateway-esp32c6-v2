#include "device_service_internal.h"

#include <stdlib.h>
#include <string.h>

#include "device_service_rules.h"

static const char *s_default_device_name_prefix = "Пристрій";

gateway_status_t device_service_create(device_service_handle_t *out_handle)
{
    if (!out_handle) {
        return GATEWAY_STATUS_INVALID_ARG;
    }

    device_service_handle_t handle = calloc(1, sizeof(*handle));
    if (!handle) {
        return GATEWAY_STATUS_NO_MEM;
    }

    *out_handle = handle;
    return GATEWAY_STATUS_OK;
}

void device_service_destroy(device_service_handle_t handle)
{
    if (!handle) {
        return;
    }

    device_service_lock_destroy(handle);
    handle->device_count = 0;
    memset(handle->devices, 0, sizeof(handle->devices));
    free(handle);
}

gateway_status_t device_service_init(device_service_handle_t handle)
{
    if (!handle) {
        return GATEWAY_STATUS_INVALID_ARG;
    }

    gateway_status_t lock_ret = device_service_lock_ensure(handle);
    if (lock_ret != GATEWAY_STATUS_OK) {
        return lock_ret;
    }
    device_service_lock_acquire(handle);
    (void)device_service_storage_load_locked(handle);
    device_service_lock_release(handle);

    return GATEWAY_STATUS_OK;
}

void device_service_add_with_ieee(device_service_handle_t handle, uint16_t addr, gateway_ieee_addr_t ieee)
{
    if (!handle) {
        return;
    }

    device_service_lock_acquire(handle);

    device_service_rules_result_t upsert_result = device_service_rules_upsert(
        handle->devices, &handle->device_count, MAX_DEVICES, addr, ieee, s_default_device_name_prefix);
    if (upsert_result == DEVICE_SERVICE_RULES_RESULT_UPDATED) {
        device_service_lock_release(handle);
        return;
    }

    if (upsert_result == DEVICE_SERVICE_RULES_RESULT_ADDED) {
        (void)device_service_storage_save_locked(handle);
    }

    device_service_lock_release(handle);
    device_service_events_post_list_changed();
}

void device_service_update_name(device_service_handle_t handle, uint16_t addr, const char *new_name)
{
    if (!handle || !new_name) {
        return;
    }
    bool renamed = false;

    device_service_lock_acquire(handle);

    renamed = device_service_rules_rename(handle->devices, handle->device_count, addr, new_name);
    if (renamed) {
        (void)device_service_storage_save_locked(handle);
    }

    device_service_lock_release(handle);
    device_service_events_post_list_changed();
}

void device_service_delete(device_service_handle_t handle, uint16_t addr)
{
    if (!handle) {
        return;
    }

    bool deleted = false;
    uint16_t deleted_short_addr = 0;
    gateway_ieee_addr_t deleted_ieee = {0};

    device_service_lock_acquire(handle);

    zb_device_t deleted_device = {0};
    deleted = device_service_rules_delete_by_short_addr(handle->devices, &handle->device_count, addr, &deleted_device);
    if (deleted) {
        deleted_short_addr = deleted_device.short_addr;
        memcpy(deleted_ieee, deleted_device.ieee_addr, sizeof(deleted_ieee));
        (void)device_service_storage_save_locked(handle);
    }

    device_service_lock_release(handle);
    if (deleted) {
        device_service_events_post_delete_request(deleted_short_addr, deleted_ieee);
    }
    device_service_events_post_list_changed();
}

int device_service_get_snapshot(device_service_handle_t handle, zb_device_t *out, size_t max_items)
{
    if (!handle || !out || max_items == 0) {
        return 0;
    }

    device_service_lock_acquire(handle);

    int count = handle->device_count;
    if ((size_t)count > max_items) {
        count = (int)max_items;
    }
    if (count > 0) {
        memcpy(out, handle->devices, sizeof(zb_device_t) * (size_t)count);
    }

    device_service_lock_release(handle);
    return count;
}
