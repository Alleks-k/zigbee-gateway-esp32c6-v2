#pragma once

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"
#include "device_manager.h"

typedef struct {
    uint32_t pan_id;
    uint32_t channel;
    uint32_t short_addr;
} zgw_network_status_t;

esp_err_t zigbee_service_get_network_status(zgw_network_status_t *out);
esp_err_t zigbee_service_permit_join(uint16_t seconds);
esp_err_t zigbee_service_send_on_off(uint16_t short_addr, uint8_t endpoint, uint8_t on_off);
int zigbee_service_get_devices_snapshot(zb_device_t *out, size_t max_items);
esp_err_t zigbee_service_delete_device(uint16_t short_addr);
esp_err_t zigbee_service_rename_device(uint16_t short_addr, const char *name);

