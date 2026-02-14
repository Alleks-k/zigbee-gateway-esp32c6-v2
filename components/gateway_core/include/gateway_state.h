#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "device_manager.h"

typedef struct {
    bool zigbee_started;
    bool factory_new;
    uint16_t pan_id;
    uint8_t channel;
    uint16_t short_addr;
} gateway_network_state_t;

typedef struct {
    bool sta_connected;
    bool fallback_ap_active;
    bool loaded_from_nvs;
    char active_ssid[33];
} gateway_wifi_state_t;

typedef enum {
    GATEWAY_LQI_SOURCE_UNKNOWN = 0,
    GATEWAY_LQI_SOURCE_NEIGHBOR_TABLE = 1,
    GATEWAY_LQI_SOURCE_MGMT_LQI = 2,
} gateway_lqi_source_t;

typedef struct {
    uint16_t short_addr;
    int lqi;
    int rssi;
    uint64_t updated_ms;
    gateway_lqi_source_t source;
} gateway_device_lqi_state_t;

esp_err_t gateway_state_init(void);
esp_err_t gateway_state_set_network(const gateway_network_state_t *state);
esp_err_t gateway_state_get_network(gateway_network_state_t *out_state);
esp_err_t gateway_state_set_wifi(const gateway_wifi_state_t *state);
esp_err_t gateway_state_get_wifi(gateway_wifi_state_t *out_state);
esp_err_t gateway_state_set_devices(const zb_device_t *devices, int count);
int gateway_state_get_devices_snapshot(zb_device_t *out, size_t max_items);
esp_err_t gateway_state_update_device_lqi(uint16_t short_addr, int lqi, int rssi, gateway_lqi_source_t source, uint64_t updated_ms);
int gateway_state_get_device_lqi_snapshot(gateway_device_lqi_state_t *out, size_t max_items);
