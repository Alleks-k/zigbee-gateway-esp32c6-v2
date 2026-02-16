#pragma once

#include <stddef.h>
#include <stdint.h>

#include "gateway_runtime_types.h"
#include "gateway_status.h"

typedef struct device_service device_service_t;
typedef device_service_t *device_service_handle_t;

gateway_status_t device_service_create(device_service_handle_t *out_handle);
void device_service_destroy(device_service_handle_t handle);
gateway_status_t device_service_init(device_service_handle_t handle);
void device_service_add_with_ieee(device_service_handle_t handle, uint16_t addr, gateway_ieee_addr_t ieee);
void device_service_update_name(device_service_handle_t handle, uint16_t addr, const char *new_name);
void device_service_delete(device_service_handle_t handle, uint16_t addr);
int device_service_get_snapshot(device_service_handle_t handle, zb_device_t *out, size_t max_items);
