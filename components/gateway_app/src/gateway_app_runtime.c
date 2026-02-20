#include "gateway_app_runtime.h"

#include "device_service_lock_freertos_port.h"
#include "gateway_events.h"
#include "gateway_persistence_adapter.h"
#include "gateway_runtime_context.h"
#include "gateway_status_esp.h"
#include "gateway_zigbee_runtime.h"
#include "system_service.h"
#include "wifi_service.h"
#include "wifi_init.h"

#include <string.h>

static gateway_status_t gateway_app_runtime_repo_load(void *ctx,
                                                      gateway_device_record_t *devices,
                                                      size_t max_devices,
                                                      int *device_count,
                                                      bool *loaded)
{
    (void)ctx;
    return gateway_persistence_devices_load(devices, max_devices, device_count, loaded);
}

static gateway_status_t gateway_app_runtime_repo_save(void *ctx,
                                                      const gateway_device_record_t *devices,
                                                      size_t max_devices,
                                                      int device_count)
{
    (void)ctx;
    return gateway_persistence_devices_save(devices, max_devices, device_count);
}

static const device_service_repo_port_t s_gateway_app_runtime_repo_port = {
    .load = gateway_app_runtime_repo_load,
    .save = gateway_app_runtime_repo_save,
    .ctx = NULL,
};

static void gateway_app_runtime_on_device_list_changed(void *ctx)
{
    (void)ctx;
    (void)esp_event_post(GATEWAY_EVENT, GATEWAY_EVENT_DEVICE_LIST_CHANGED, NULL, 0, 0);
}

static void gateway_app_runtime_on_device_delete_request(void *ctx,
                                                         uint16_t short_addr,
                                                         const gateway_ieee_addr_t ieee_addr)
{
    (void)ctx;
    gateway_device_delete_request_event_t evt = {
        .short_addr = short_addr,
    };
    memcpy(evt.ieee_addr, ieee_addr, sizeof(evt.ieee_addr));
    (void)esp_event_post(GATEWAY_EVENT, GATEWAY_EVENT_DEVICE_DELETE_REQUEST, &evt, sizeof(evt), 0);
}

static const device_service_notifier_t s_gateway_app_runtime_notifier = {
    .on_list_changed = gateway_app_runtime_on_device_list_changed,
    .on_delete_request = gateway_app_runtime_on_device_delete_request,
    .ctx = NULL,
};

esp_err_t gateway_app_runtime_create(gateway_app_runtime_handles_t *out_handles)
{
    if (!out_handles) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(out_handles, 0, sizeof(*out_handles));

    device_service_init_params_t device_service_params = {
        .lock_port = device_service_lock_port_freertos(),
        .repo_port = &s_gateway_app_runtime_repo_port,
        .notifier = &s_gateway_app_runtime_notifier,
    };
    gateway_wifi_system_init_params_t wifi_system_params = {0};
    gateway_jobs_init_params_t jobs_params = {0};
    api_usecases_init_params_t api_usecases_params = {0};
    gateway_runtime_context_t runtime_ctx = {0};

    esp_err_t ret = gateway_status_to_esp_err(
        device_service_create_with_params(&device_service_params, &out_handles->device_service));
    if (ret != ESP_OK) {
        goto fail;
    }

    ret = gateway_status_to_esp_err(gateway_state_create(&out_handles->gateway_state));
    if (ret != ESP_OK) {
        goto fail;
    }

    ret = wifi_service_create(&out_handles->wifi_service);
    if (ret != ESP_OK) {
        goto fail;
    }

    ret = system_service_create(&out_handles->system_service);
    if (ret != ESP_OK) {
        goto fail;
    }

    ret = gateway_status_to_esp_err(device_service_init(out_handles->device_service));
    if (ret != ESP_OK) {
        goto fail;
    }

    ret = gateway_status_to_esp_err(gateway_state_init(out_handles->gateway_state));
    if (ret != ESP_OK) {
        goto fail;
    }

    ret = wifi_init_bind_state(&out_handles->wifi_runtime,
                               out_handles->gateway_state,
                               out_handles->wifi_service,
                               out_handles->system_service);
    if (ret != ESP_OK) {
        goto fail;
    }

    wifi_system_params.gateway_state_handle = out_handles->gateway_state;
    wifi_system_params.wifi_service_handle = out_handles->wifi_service;
    wifi_system_params.system_service_handle = out_handles->system_service;
    ret = gateway_wifi_system_create(&wifi_system_params, &out_handles->wifi_system);
    if (ret != ESP_OK) {
        goto fail;
    }

    jobs_params.wifi_service_handle = out_handles->wifi_service;
    jobs_params.system_service_handle = out_handles->system_service;
    ret = gateway_jobs_create(&jobs_params, &out_handles->jobs);
    if (ret != ESP_OK) {
        goto fail;
    }

    api_usecases_params.wifi_system = out_handles->wifi_system;
    api_usecases_params.jobs = out_handles->jobs;
    ret = api_usecases_create(&api_usecases_params, &out_handles->api_usecases);
    if (ret != ESP_OK) {
        goto fail;
    }
    api_usecases_set_service_ops_with_handle(out_handles->api_usecases, NULL);

    ret = ws_manager_create(&out_handles->ws_manager);
    if (ret != ESP_OK) {
        goto fail;
    }

    runtime_ctx.device_service = out_handles->device_service;
    runtime_ctx.gateway_state = out_handles->gateway_state;
    ret = gateway_zigbee_runtime_create(&runtime_ctx, &out_handles->zigbee_runtime);
    if (ret != ESP_OK) {
        goto fail;
    }

    out_handles->zigbee_service = gateway_zigbee_runtime_get_service_handle(out_handles->zigbee_runtime);
    if (!out_handles->zigbee_service) {
        ret = ESP_ERR_INVALID_STATE;
        goto fail;
    }

    api_usecases_set_runtime_handles(out_handles->api_usecases, out_handles->zigbee_service, out_handles->wifi_system,
                                     out_handles->jobs);

    ret = gateway_jobs_set_zigbee_service(out_handles->jobs, out_handles->zigbee_service);
    if (ret != ESP_OK) {
        goto fail;
    }

    return ESP_OK;

fail:
    gateway_app_runtime_destroy(out_handles);
    return ret;
}

void gateway_app_runtime_destroy(gateway_app_runtime_handles_t *handles)
{
    if (!handles) {
        return;
    }

    if (handles->ws_manager) {
        ws_manager_destroy(handles->ws_manager);
    }

    if (handles->api_usecases) {
        api_usecases_destroy(handles->api_usecases);
    }

    if (handles->zigbee_runtime) {
        gateway_zigbee_runtime_destroy(handles->zigbee_runtime);
        handles->zigbee_runtime = NULL;
        handles->zigbee_service = NULL;
    }

    if (handles->jobs) {
        gateway_jobs_destroy(handles->jobs);
    }

    if (handles->wifi_system) {
        gateway_wifi_system_destroy(handles->wifi_system);
    }

    if (handles->system_service) {
        system_service_destroy(handles->system_service);
    }

    if (handles->wifi_service) {
        wifi_service_destroy(handles->wifi_service);
    }

    if (handles->gateway_state) {
        gateway_state_destroy(handles->gateway_state);
    }

    if (handles->device_service) {
        device_service_destroy(handles->device_service);
    }

    memset(handles, 0, sizeof(*handles));
}
