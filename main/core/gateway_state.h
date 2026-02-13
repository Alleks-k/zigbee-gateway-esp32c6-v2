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

esp_err_t gateway_state_init(void);
esp_err_t gateway_state_set_network(const gateway_network_state_t *state);
esp_err_t gateway_state_get_network(gateway_network_state_t *out_state);
esp_err_t gateway_state_set_devices(const zb_device_t *devices, int count);
int gateway_state_get_devices_snapshot(zb_device_t *out, size_t max_items);
