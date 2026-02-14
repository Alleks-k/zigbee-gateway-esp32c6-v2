#pragma once

#include "esp_event.h"
#include "esp_zigbee_core.h"
#include <stdint.h>

ESP_EVENT_DECLARE_BASE(GATEWAY_EVENT);

typedef enum {
    GATEWAY_EVENT_DEVICE_ANNOUNCE = 1,
    GATEWAY_EVENT_DEVICE_DELETE_REQUEST,
    GATEWAY_EVENT_DEVICE_LIST_CHANGED,
    GATEWAY_EVENT_LQI_STATE_CHANGED,
} gateway_event_id_t;

typedef struct {
    uint16_t short_addr;
    esp_zb_ieee_addr_t ieee_addr;
} gateway_device_announce_event_t;

typedef struct {
    uint16_t short_addr;
    esp_zb_ieee_addr_t ieee_addr;
} gateway_device_delete_request_event_t;
