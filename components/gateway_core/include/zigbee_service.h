#pragma once

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"
#include "gateway_config_types.h"

#ifndef MAX_DEVICES
#define MAX_DEVICES GATEWAY_MAX_DEVICES
#endif

typedef gateway_device_record_t zb_device_t;

typedef struct {
    uint32_t pan_id;
    uint32_t channel;
    uint32_t short_addr;
} zigbee_network_status_t;

typedef enum {
    ZIGBEE_LQI_SOURCE_UNKNOWN = 0,
    ZIGBEE_LQI_SOURCE_NEIGHBOR_TABLE,
    ZIGBEE_LQI_SOURCE_MGMT_LQI,
} zigbee_lqi_source_t;

typedef struct {
    uint16_t short_addr;
    int lqi;
    int rssi;
    uint8_t relationship;
    uint8_t depth;
    uint64_t updated_ms;
    zigbee_lqi_source_t source;
} zigbee_neighbor_lqi_t;

typedef struct {
    esp_err_t (*send_on_off)(uint16_t short_addr, uint8_t endpoint, uint8_t on_off);
    esp_err_t (*delete_device)(uint16_t short_addr);
    esp_err_t (*rename_device)(uint16_t short_addr, const char *name);
} zigbee_service_runtime_ops_t;

void zigbee_service_set_runtime_ops(const zigbee_service_runtime_ops_t *ops);

esp_err_t zigbee_service_get_network_status(zigbee_network_status_t *out);
esp_err_t zigbee_service_permit_join(uint16_t seconds);
esp_err_t zigbee_service_send_on_off(uint16_t short_addr, uint8_t endpoint, uint8_t on_off);
int zigbee_service_get_devices_snapshot(zb_device_t *out, size_t max_items);
int zigbee_service_get_neighbor_lqi_snapshot(zigbee_neighbor_lqi_t *out, size_t max_items);
esp_err_t zigbee_service_refresh_neighbor_lqi_snapshot(zigbee_neighbor_lqi_t *out, size_t max_items, int *out_count);
esp_err_t zigbee_service_refresh_neighbor_lqi_from_table(void);
esp_err_t zigbee_service_get_cached_lqi_snapshot(zigbee_neighbor_lqi_t *out, size_t max_items, int *out_count,
                                                 zigbee_lqi_source_t *out_source, uint64_t *out_updated_ms);
esp_err_t zigbee_service_delete_device(uint16_t short_addr);
esp_err_t zigbee_service_rename_device(uint16_t short_addr, const char *name);
