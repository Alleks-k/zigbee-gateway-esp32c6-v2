#pragma once

#include "api_usecases.h"

struct api_usecases {
    const api_service_ops_t *service_ops;
    zigbee_service_handle_t zigbee_service;
    gateway_wifi_system_handle_t wifi_system;
    gateway_jobs_handle_t jobs;
    api_ws_client_count_provider_t ws_client_count_provider;
    api_ws_metrics_provider_t ws_metrics_provider;
    api_ws_provider_ctx_t *ws_provider_ctx;
};

esp_err_t api_usecases_require_handle(api_usecases_handle_t handle);
esp_err_t api_usecases_require_wifi_system(api_usecases_handle_t handle);
esp_err_t api_usecases_require_zigbee(api_usecases_handle_t handle);
esp_err_t api_usecases_require_jobs(api_usecases_handle_t handle);
