#include "api_usecases_internal.h"

#include <stdlib.h>

esp_err_t api_usecases_create(const api_usecases_init_params_t *params, api_usecases_handle_t *out_handle)
{
    if (!out_handle) {
        return ESP_ERR_INVALID_ARG;
    }

    api_usecases_t *handle = (api_usecases_t *)calloc(1, sizeof(*handle));
    if (!handle) {
        return ESP_ERR_NO_MEM;
    }

    if (params) {
        handle->service_ops = params->service_ops;
        handle->zigbee_service = params->zigbee_service;
        handle->wifi_system = params->wifi_system;
        handle->jobs = params->jobs;
        handle->ws_client_count_provider = params->ws_client_count_provider;
        handle->ws_metrics_provider = params->ws_metrics_provider;
        handle->ws_provider_ctx = params->ws_provider_ctx;
    }

    *out_handle = handle;
    return ESP_OK;
}

void api_usecases_destroy(api_usecases_handle_t handle)
{
    free(handle);
}

void api_usecases_set_service_ops_with_handle(api_usecases_handle_t handle, const api_service_ops_t *ops)
{
    if (!handle) {
        return;
    }
    handle->service_ops = ops;
}

void api_usecases_set_runtime_handles(api_usecases_handle_t handle, zigbee_service_handle_t zigbee_service,
                                      gateway_wifi_system_handle_t wifi_system, gateway_jobs_handle_t jobs)
{
    if (!handle) {
        return;
    }
    handle->zigbee_service = zigbee_service;
    handle->wifi_system = wifi_system;
    handle->jobs = jobs;
}

void api_usecases_set_ws_providers(api_usecases_handle_t handle, api_ws_client_count_provider_t count_provider,
                                   api_ws_metrics_provider_t metrics_provider, api_ws_provider_ctx_t *provider_ctx)
{
    if (!handle) {
        return;
    }
    handle->ws_client_count_provider = count_provider;
    handle->ws_metrics_provider = metrics_provider;
    handle->ws_provider_ctx = provider_ctx;
}

esp_err_t api_usecases_require_handle(api_usecases_handle_t handle)
{
    return handle ? ESP_OK : ESP_ERR_INVALID_ARG;
}

esp_err_t api_usecases_require_wifi_system(api_usecases_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    return handle->wifi_system ? ESP_OK : ESP_ERR_INVALID_STATE;
}

esp_err_t api_usecases_require_zigbee(api_usecases_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    return handle->zigbee_service ? ESP_OK : ESP_ERR_INVALID_STATE;
}

esp_err_t api_usecases_require_jobs(api_usecases_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    return handle->jobs ? ESP_OK : ESP_ERR_INVALID_STATE;
}
