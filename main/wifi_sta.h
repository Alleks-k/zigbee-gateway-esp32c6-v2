#pragma once

#include "esp_err.h"
#include "wifi_context.h"

esp_err_t wifi_sta_connect_and_wait(wifi_runtime_ctx_t *ctx);
