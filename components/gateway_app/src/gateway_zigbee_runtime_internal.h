#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "device_service.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_zigbee_core.h"
#include "gateway_zigbee_runtime.h"
#include "state_store.h"
#include "zigbee_service.h"

struct gateway_zigbee_runtime {
    esp_event_handler_instance_t delete_req_handler;
    int64_t last_live_lqi_refresh_us;
    device_service_handle_t device_service;
    gateway_state_handle_t gateway_state;
    zigbee_service_handle_t zigbee_service;
};

gateway_zigbee_runtime_handle_t gateway_zigbee_runtime_get_active(void);
void gateway_zigbee_runtime_set_active(gateway_zigbee_runtime_handle_t handle);

void refresh_lqi_from_live_event(gateway_zigbee_runtime_handle_t handle, const char *reason);
void gateway_state_publish(gateway_zigbee_runtime_handle_t handle, bool zigbee_started, bool factory_new);
void device_delete_request_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
esp_err_t gateway_zigbee_runtime_action_handler(esp_zb_core_action_callback_id_t callback_id, const void *message);
const zigbee_service_runtime_ops_t *gateway_zigbee_runtime_get_ops(void);
