#pragma once

#include "device_service.h"

#include "esp_err.h"
#include "gateway_status.h"

struct device_service {
    void *devices_mutex;
    zb_device_t devices[MAX_DEVICES];
    int device_count;
    device_service_on_list_changed_fn on_list_changed;
    device_service_on_delete_request_fn on_delete_request;
    void *notifier_ctx;
};

esp_err_t device_service_storage_save_locked(device_service_handle_t handle);
esp_err_t device_service_storage_load_locked(device_service_handle_t handle);

gateway_status_t device_service_lock_ensure(device_service_handle_t handle);
void device_service_lock_destroy(device_service_handle_t handle);
void device_service_lock_acquire(device_service_handle_t handle);
void device_service_lock_release(device_service_handle_t handle);
