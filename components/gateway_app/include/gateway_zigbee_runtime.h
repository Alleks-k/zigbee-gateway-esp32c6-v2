#pragma once

#include "esp_err.h"
#include "gateway_runtime_context.h"

esp_err_t gateway_zigbee_runtime_prepare(const gateway_runtime_context_t *ctx);
esp_err_t gateway_zigbee_runtime_start(void);
