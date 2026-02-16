#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "gateway_config_types.h"

#ifndef MAX_DEVICES
#define MAX_DEVICES GATEWAY_MAX_DEVICES
#endif

#ifndef GATEWAY_STATE_LQI_CACHE_CAPACITY
#define GATEWAY_STATE_LQI_CACHE_CAPACITY GATEWAY_MAX_DEVICES
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
    char ssid[33];
    int8_t rssi;
    uint8_t auth;
} wifi_ap_info_t;

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
