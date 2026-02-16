#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "gateway_config_types.h"

#ifndef GATEWAY_STATE_LQI_CACHE_CAPACITY
#define GATEWAY_STATE_LQI_CACHE_CAPACITY GATEWAY_MAX_DEVICES
#endif

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
} gateway_lqi_cache_entry_t;

typedef struct gateway_state_store gateway_state_store_t;
typedef gateway_state_store_t *gateway_state_handle_t;

esp_err_t gateway_state_get_default(gateway_state_handle_t *out_handle);
esp_err_t gateway_state_init(gateway_state_handle_t handle);
esp_err_t gateway_state_set_network(gateway_state_handle_t handle, const gateway_network_state_t *state);
esp_err_t gateway_state_get_network(gateway_state_handle_t handle, gateway_network_state_t *out_state);
esp_err_t gateway_state_set_wifi(gateway_state_handle_t handle, const gateway_wifi_state_t *state);
esp_err_t gateway_state_get_wifi(gateway_state_handle_t handle, gateway_wifi_state_t *out_state);
esp_err_t gateway_state_update_lqi(gateway_state_handle_t handle,
                                   uint16_t short_addr,
                                   int lqi,
                                   int rssi,
                                   gateway_lqi_source_t source,
                                   uint64_t updated_ms);
int gateway_state_get_lqi_snapshot(gateway_state_handle_t handle, gateway_lqi_cache_entry_t *out, size_t max_items);
