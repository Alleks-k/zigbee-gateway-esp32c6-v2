#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "device_service.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_zigbee_core.h"
#include "state_store.h"
#include "zigbee_service.h"

extern esp_event_handler_instance_t s_delete_req_handler;
extern int64_t s_last_live_lqi_refresh_us;
extern device_service_handle_t s_device_service;
extern gateway_state_handle_t s_gateway_state;
extern zigbee_service_handle_t s_zigbee_service;

void refresh_lqi_from_live_event(const char *reason);
void gateway_state_publish(bool zigbee_started, bool factory_new);
void device_delete_request_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
esp_err_t gateway_zigbee_runtime_action_handler(esp_zb_core_action_callback_id_t callback_id, const void *message);
const zigbee_service_runtime_ops_t *gateway_zigbee_runtime_get_ops(void);
