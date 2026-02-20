#pragma once

#include "esp_err.h"
#include "gateway_runtime_context.h"

esp_err_t gateway_zigbee_runtime_prepare(const gateway_runtime_context_t *ctx);
esp_err_t gateway_zigbee_runtime_start(void);

struct zigbee_service;
typedef struct zigbee_service *zigbee_service_handle_t;
zigbee_service_handle_t gateway_zigbee_runtime_get_service_handle(void);
