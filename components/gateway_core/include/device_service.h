#pragma once

#include <stddef.h>
#include <stdint.h>

#include "device_service_lock_port.h"
#include "device_service_repo_port.h"
#include "gateway_runtime_types.h"
#include "gateway_status.h"

typedef struct device_service device_service_t;
typedef device_service_t *device_service_handle_t;

typedef void (*device_service_on_list_changed_fn)(void *ctx);
typedef void (*device_service_on_delete_request_fn)(void *ctx, uint16_t short_addr, const gateway_ieee_addr_t ieee_addr);

typedef struct {
    device_service_on_list_changed_fn on_list_changed;
    device_service_on_delete_request_fn on_delete_request;
    void *ctx;
} device_service_notifier_t;

typedef struct {
    const device_service_lock_port_t *lock_port;
    const device_service_repo_port_t *repo_port;
    const device_service_notifier_t *notifier;
} device_service_init_params_t;

gateway_status_t device_service_create_with_params(const device_service_init_params_t *params,
                                                   device_service_handle_t *out_handle);
gateway_status_t device_service_create(device_service_handle_t *out_handle);
void device_service_destroy(device_service_handle_t handle);
gateway_status_t device_service_init(device_service_handle_t handle);
gateway_status_t device_service_set_notifier(device_service_handle_t handle, const device_service_notifier_t *notifier);
void device_service_add_with_ieee(device_service_handle_t handle, uint16_t addr, gateway_ieee_addr_t ieee);
void device_service_update_name(device_service_handle_t handle, uint16_t addr, const char *new_name);
void device_service_delete(device_service_handle_t handle, uint16_t addr);
int device_service_get_snapshot(device_service_handle_t handle, zb_device_t *out, size_t max_items);
