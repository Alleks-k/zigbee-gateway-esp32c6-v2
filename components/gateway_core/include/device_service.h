#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "gateway_config_types.h"

#ifndef MAX_DEVICES
#define MAX_DEVICES GATEWAY_MAX_DEVICES
#endif

typedef gateway_device_record_t zb_device_t;

esp_err_t device_service_init(void);
void device_service_add_with_ieee(uint16_t addr, gateway_ieee_addr_t ieee);
void device_service_update_name(uint16_t addr, const char *new_name);
void device_service_delete(uint16_t addr);
int device_service_get_snapshot(zb_device_t *out, size_t max_items);
void device_service_lock(void);
void device_service_unlock(void);
