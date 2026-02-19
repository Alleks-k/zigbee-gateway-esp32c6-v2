#include "device_service_internal.h"

#include <stdlib.h>
#include <string.h>

#include "device_service_rules.h"

static const char *s_default_device_name_prefix = "Пристрій";

static gateway_status_t device_service_lock_ensure(device_service_handle_t handle)
{
    if (!handle) {
        return GATEWAY_STATUS_INVALID_ARG;
    }
    if (!handle->lock_port || !handle->lock_port->create) {
        return GATEWAY_STATUS_INVALID_STATE;
    }
    if (handle->lock_handle) {
        return GATEWAY_STATUS_OK;
    }
    return handle->lock_port->create(handle->lock_port->ctx, &handle->lock_handle);
}

static void device_service_lock_destroy(device_service_handle_t handle)
{
    if (!handle || !handle->lock_handle || !handle->lock_port || !handle->lock_port->destroy) {
        return;
    }
    handle->lock_port->destroy(handle->lock_port->ctx, handle->lock_handle);
    handle->lock_handle = NULL;
}

static void device_service_lock_acquire(device_service_handle_t handle)
{
    if (!handle || !handle->lock_handle || !handle->lock_port || !handle->lock_port->enter) {
        return;
    }
    handle->lock_port->enter(handle->lock_port->ctx, handle->lock_handle);
}

static void device_service_lock_release(device_service_handle_t handle)
{
    if (!handle || !handle->lock_handle || !handle->lock_port || !handle->lock_port->exit) {
        return;
    }
    handle->lock_port->exit(handle->lock_port->ctx, handle->lock_handle);
}

static void device_service_notify_list_changed(device_service_handle_t handle)
{
    if (!handle || !handle->on_list_changed) {
        return;
    }
    handle->on_list_changed(handle->notifier_ctx);
}

static void device_service_notify_delete_request(device_service_handle_t handle, uint16_t short_addr, gateway_ieee_addr_t ieee_addr)
{
    if (!handle || !handle->on_delete_request) {
        return;
    }
    handle->on_delete_request(handle->notifier_ctx, short_addr, ieee_addr);
}

gateway_status_t device_service_create_with_params(const device_service_init_params_t *params,
                                                   device_service_handle_t *out_handle)
{
    if (!out_handle) {
        return GATEWAY_STATUS_INVALID_ARG;
    }

    device_service_handle_t handle = calloc(1, sizeof(*handle));
    if (!handle) {
        return GATEWAY_STATUS_NO_MEM;
    }

    if (params) {
        handle->lock_port = params->lock_port;
        handle->repo_port = params->repo_port;
        if (params->notifier) {
            handle->on_list_changed = params->notifier->on_list_changed;
            handle->on_delete_request = params->notifier->on_delete_request;
            handle->notifier_ctx = params->notifier->ctx;
        }
    }

    *out_handle = handle;
    return GATEWAY_STATUS_OK;
}

gateway_status_t device_service_create(device_service_handle_t *out_handle)
{
    return device_service_create_with_params(NULL, out_handle);
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
    if (!handle->lock_port || !handle->repo_port) {
        return GATEWAY_STATUS_INVALID_STATE;
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

gateway_status_t device_service_set_notifier(device_service_handle_t handle, const device_service_notifier_t *notifier)
{
    if (!handle) {
        return GATEWAY_STATUS_INVALID_ARG;
    }

    if (!notifier) {
        handle->on_list_changed = NULL;
        handle->on_delete_request = NULL;
        handle->notifier_ctx = NULL;
        return GATEWAY_STATUS_OK;
    }

    handle->on_list_changed = notifier->on_list_changed;
    handle->on_delete_request = notifier->on_delete_request;
    handle->notifier_ctx = notifier->ctx;
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
    device_service_notify_list_changed(handle);
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
    device_service_notify_list_changed(handle);
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
        device_service_notify_delete_request(handle, deleted_short_addr, deleted_ieee);
    }
    device_service_notify_list_changed(handle);
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
