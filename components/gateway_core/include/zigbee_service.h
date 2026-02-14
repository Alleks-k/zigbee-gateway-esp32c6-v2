#pragma once

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"
#include "device_manager.h"

typedef struct {
    uint32_t pan_id;
    uint32_t channel;
    uint32_t short_addr;
} zigbee_network_status_t;

typedef struct {
    uint16_t short_addr;
    int lqi;
    int rssi;
    uint8_t relationship;
    uint8_t depth;
} zigbee_neighbor_lqi_t;

esp_err_t zigbee_service_get_network_status(zigbee_network_status_t *out);
esp_err_t zigbee_service_permit_join(uint16_t seconds);
esp_err_t zigbee_service_send_on_off(uint16_t short_addr, uint8_t endpoint, uint8_t on_off);
int zigbee_service_get_devices_snapshot(zb_device_t *out, size_t max_items);
int zigbee_service_get_neighbor_lqi_snapshot(zigbee_neighbor_lqi_t *out, size_t max_items);
esp_err_t zigbee_service_refresh_neighbor_lqi_snapshot(zigbee_neighbor_lqi_t *out, size_t max_items, int *out_count);
esp_err_t zigbee_service_delete_device(uint16_t short_addr);
esp_err_t zigbee_service_rename_device(uint16_t short_addr, const char *name);
