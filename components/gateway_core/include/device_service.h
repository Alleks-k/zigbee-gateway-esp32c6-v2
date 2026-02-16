#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "gateway_config_types.h"

#ifndef MAX_DEVICES
#define MAX_DEVICES GATEWAY_MAX_DEVICES
#endif

typedef gateway_device_record_t zb_device_t;

typedef struct device_service device_service_t;
typedef device_service_t *device_service_handle_t;

esp_err_t device_service_get_default(device_service_handle_t *out_handle);
esp_err_t device_service_init(device_service_handle_t handle);
void device_service_add_with_ieee(device_service_handle_t handle, uint16_t addr, gateway_ieee_addr_t ieee);
void device_service_update_name(device_service_handle_t handle, uint16_t addr, const char *new_name);
void device_service_delete(device_service_handle_t handle, uint16_t addr);
int device_service_get_snapshot(device_service_handle_t handle, zb_device_t *out, size_t max_items);
