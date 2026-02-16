#pragma once

#include "device_service.h"

#include "esp_err.h"
#include "gateway_status.h"

struct device_service {
    void *devices_mutex;
    zb_device_t devices[MAX_DEVICES];
    int device_count;
    void *dev_announce_handler;
};

esp_err_t device_service_storage_save_locked(device_service_handle_t handle);
esp_err_t device_service_storage_load_locked(device_service_handle_t handle);

esp_err_t device_service_events_register_announce_handler(device_service_handle_t handle);
void device_service_events_unregister_announce_handler(device_service_handle_t handle);
void device_service_events_post_list_changed(void);
void device_service_events_post_delete_request(uint16_t short_addr, gateway_ieee_addr_t ieee_addr);

gateway_status_t device_service_lock_ensure(device_service_handle_t handle);
void device_service_lock_destroy(device_service_handle_t handle);
void device_service_lock_acquire(device_service_handle_t handle);
void device_service_lock_release(device_service_handle_t handle);
