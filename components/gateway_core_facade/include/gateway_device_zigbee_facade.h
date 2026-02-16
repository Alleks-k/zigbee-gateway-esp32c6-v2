#pragma once

#include <stddef.h>
#include <stdint.h>

#include "device_service.h"
#include "esp_err.h"
#include "zigbee_service.h"

esp_err_t gateway_core_facade_send_on_off(uint16_t short_addr, uint8_t endpoint, uint8_t on_off);
esp_err_t gateway_core_facade_get_network_status(zigbee_network_status_t *out_status);
int gateway_core_facade_get_devices_snapshot(zb_device_t *out_devices, int max_devices);
int gateway_core_facade_get_neighbor_lqi_snapshot(zigbee_neighbor_lqi_t *out_neighbors, int max_neighbors);
esp_err_t gateway_core_facade_get_cached_lqi_snapshot(zigbee_neighbor_lqi_t *out_neighbors, int max_neighbors, int *out_count,
                                                      zigbee_lqi_source_t *out_source, uint64_t *out_updated_ms);
esp_err_t gateway_core_facade_permit_join(uint8_t duration_seconds);
esp_err_t gateway_core_facade_delete_device(uint16_t short_addr);
esp_err_t gateway_core_facade_rename_device(uint16_t short_addr, const char *name);
