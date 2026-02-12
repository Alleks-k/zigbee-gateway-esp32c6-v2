#pragma once

#include "esp_event.h"
#include "esp_zigbee_core.h"
#include <stdint.h>

ESP_EVENT_DECLARE_BASE(ZGW_EVENT);

typedef enum {
    ZGW_EVENT_DEVICE_ANNOUNCE = 1,
    ZGW_EVENT_DEVICE_DELETE_REQUEST,
    ZGW_EVENT_DEVICE_LIST_CHANGED,
} zgw_event_id_t;

typedef struct {
    uint16_t short_addr;
    esp_zb_ieee_addr_t ieee_addr;
} zgw_device_announce_event_t;

typedef struct {
    uint16_t short_addr;
    esp_zb_ieee_addr_t ieee_addr;
} zgw_device_delete_request_event_t;
