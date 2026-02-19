#pragma once

#include "device_service.h"

#include "gateway_status.h"

struct device_service {
    void *lock_handle;
    const device_service_lock_port_t *lock_port;
    const device_service_repo_port_t *repo_port;
    zb_device_t devices[MAX_DEVICES];
    int device_count;
    device_service_on_list_changed_fn on_list_changed;
    device_service_on_delete_request_fn on_delete_request;
    void *notifier_ctx;
};

gateway_status_t device_service_storage_save_locked(device_service_handle_t handle);
gateway_status_t device_service_storage_load_locked(device_service_handle_t handle);
