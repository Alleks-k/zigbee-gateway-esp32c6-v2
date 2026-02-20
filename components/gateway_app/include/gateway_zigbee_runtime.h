#pragma once

#include "esp_err.h"
#include "gateway_runtime_context.h"

typedef struct gateway_zigbee_runtime gateway_zigbee_runtime_t;
typedef gateway_zigbee_runtime_t *gateway_zigbee_runtime_handle_t;

struct zigbee_service;
typedef struct zigbee_service *zigbee_service_handle_t;

esp_err_t gateway_zigbee_runtime_create(const gateway_runtime_context_t *ctx, gateway_zigbee_runtime_handle_t *out_handle);
void gateway_zigbee_runtime_destroy(gateway_zigbee_runtime_handle_t handle);
esp_err_t gateway_zigbee_runtime_start(gateway_zigbee_runtime_handle_t handle);
zigbee_service_handle_t gateway_zigbee_runtime_get_service_handle(gateway_zigbee_runtime_handle_t handle);
