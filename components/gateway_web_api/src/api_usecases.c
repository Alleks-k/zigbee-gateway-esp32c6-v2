#include "api_usecases.h"

#include <stdlib.h>
#include <string.h>

struct api_usecases {
    const api_service_ops_t *service_ops;
    zigbee_service_handle_t zigbee_service;
    gateway_wifi_system_handle_t wifi_system;
    gateway_jobs_handle_t jobs;
    api_ws_client_count_provider_t ws_client_count_provider;
    api_ws_metrics_provider_t ws_metrics_provider;
    api_ws_provider_ctx_t *ws_provider_ctx;
};

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

static esp_err_t require_handle(api_usecases_handle_t handle)
{
    return handle ? ESP_OK : ESP_ERR_INVALID_ARG;
}

static esp_err_t require_wifi_system(api_usecases_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    return handle->wifi_system ? ESP_OK : ESP_ERR_INVALID_STATE;
}

static esp_err_t require_zigbee(api_usecases_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    return handle->zigbee_service ? ESP_OK : ESP_ERR_INVALID_STATE;
}

static esp_err_t require_jobs(api_usecases_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    return handle->jobs ? ESP_OK : ESP_ERR_INVALID_STATE;
}

esp_err_t api_usecase_control(api_usecases_handle_t handle, const api_control_request_t *in)
{
    esp_err_t ret = require_handle(handle);
    if (ret != ESP_OK || !in) {
        return ESP_ERR_INVALID_ARG;
    }

    if (handle->service_ops) {
        if (!handle->service_ops->send_on_off) {
            return ESP_ERR_INVALID_ARG;
        }
        return handle->service_ops->send_on_off(in->addr, in->ep, in->cmd);
    }

    ret = require_zigbee(handle);
    if (ret != ESP_OK) {
        return ret;
    }

    return gateway_device_zigbee_send_on_off(handle->zigbee_service, in->addr, in->ep, in->cmd);
}

esp_err_t api_usecase_wifi_save(api_usecases_handle_t handle, const api_wifi_save_request_t *in)
{
    if (!handle || !in) {
        return ESP_ERR_INVALID_ARG;
    }

    if (handle->service_ops) {
        if (!handle->service_ops->wifi_save_credentials || !handle->service_ops->schedule_reboot) {
            return ESP_ERR_INVALID_ARG;
        }
        esp_err_t err = handle->service_ops->wifi_save_credentials(in->ssid, in->password);
        if (err != ESP_OK) {
            return err;
        }
        return handle->service_ops->schedule_reboot(1000);
    }

    esp_err_t err = require_wifi_system(handle);
    if (err != ESP_OK) {
        return err;
    }
    err = gateway_wifi_system_save_credentials(handle->wifi_system, in->ssid, in->password);
    if (err != ESP_OK) {
        return err;
    }
    return gateway_wifi_system_schedule_reboot(handle->wifi_system, 1000);
}

esp_err_t api_usecase_factory_reset(api_usecases_handle_t handle)
{
    esp_err_t ret = require_handle(handle);
    if (ret != ESP_OK) {
        return ret;
    }

    if (handle->service_ops) {
        if (!handle->service_ops->factory_reset_and_reboot) {
            return ESP_ERR_INVALID_ARG;
        }
        return handle->service_ops->factory_reset_and_reboot(1000);
    }

    ret = require_wifi_system(handle);
    if (ret != ESP_OK) {
        return ret;
    }
    return gateway_wifi_system_factory_reset_and_reboot(handle->wifi_system, 1000);
}

esp_err_t api_usecase_get_network_status(api_usecases_handle_t handle, zigbee_network_status_t *out_status)
{
    esp_err_t ret = require_handle(handle);
    if (ret != ESP_OK || !out_status) {
        return ESP_ERR_INVALID_ARG;
    }
    ret = require_zigbee(handle);
    if (ret != ESP_OK) {
        return ret;
    }

    return gateway_device_zigbee_get_network_status(handle->zigbee_service, out_status);
}

int api_usecase_get_devices_snapshot(api_usecases_handle_t handle, zb_device_t *out_devices, int max_devices)
{
    if (!handle) {
        return 0;
    }
    if (require_zigbee(handle) != ESP_OK) {
        return 0;
    }
    return gateway_device_zigbee_get_devices_snapshot(handle->zigbee_service, out_devices, max_devices);
}

int api_usecase_get_neighbor_lqi_snapshot(api_usecases_handle_t handle, zigbee_neighbor_lqi_t *out_neighbors, int max_neighbors)
{
    if (!handle) {
        return 0;
    }
    if (require_zigbee(handle) != ESP_OK) {
        return 0;
    }
    return gateway_device_zigbee_get_neighbor_lqi_snapshot(handle->zigbee_service, out_neighbors, max_neighbors);
}

esp_err_t api_usecase_get_cached_lqi_snapshot(api_usecases_handle_t handle, zigbee_neighbor_lqi_t *out_neighbors,
                                              int max_neighbors, int *out_count, zigbee_lqi_source_t *out_source,
                                              uint64_t *out_updated_ms)
{
    esp_err_t ret = require_handle(handle);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = require_zigbee(handle);
    if (ret != ESP_OK) {
        return ret;
    }

    return gateway_device_zigbee_get_cached_lqi_snapshot(handle->zigbee_service, out_neighbors, max_neighbors, out_count,
                                                         out_source, out_updated_ms);
}

esp_err_t api_usecase_permit_join(api_usecases_handle_t handle, uint8_t duration_seconds)
{
    esp_err_t ret = require_handle(handle);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = require_zigbee(handle);
    if (ret != ESP_OK) {
        return ret;
    }

    return gateway_device_zigbee_permit_join(handle->zigbee_service, duration_seconds);
}

esp_err_t api_usecase_delete_device(api_usecases_handle_t handle, uint16_t short_addr)
{
    esp_err_t ret = require_handle(handle);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = require_zigbee(handle);
    if (ret != ESP_OK) {
        return ret;
    }

    return gateway_device_zigbee_delete_device(handle->zigbee_service, short_addr);
}

esp_err_t api_usecase_rename_device(api_usecases_handle_t handle, uint16_t short_addr, const char *name)
{
    esp_err_t ret = require_handle(handle);
    if (ret != ESP_OK || !name) {
        return ESP_ERR_INVALID_ARG;
    }
    ret = require_zigbee(handle);
    if (ret != ESP_OK) {
        return ret;
    }

    return gateway_device_zigbee_rename_device(handle->zigbee_service, short_addr, name);
}

esp_err_t api_usecase_wifi_scan(api_usecases_handle_t handle, wifi_ap_info_t **out_list, size_t *out_count)
{
    esp_err_t ret = require_wifi_system(handle);
    if (ret != ESP_OK) {
        return ret;
    }
    return gateway_wifi_system_scan(handle->wifi_system, out_list, out_count);
}

void api_usecase_wifi_scan_free(api_usecases_handle_t handle, wifi_ap_info_t *list)
{
    if (!handle || !handle->wifi_system) {
        return;
    }
    gateway_wifi_system_scan_free(handle->wifi_system, list);
}

esp_err_t api_usecase_schedule_reboot(api_usecases_handle_t handle, uint32_t delay_ms)
{
    esp_err_t ret = require_wifi_system(handle);
    if (ret != ESP_OK) {
        return ret;
    }
    return gateway_wifi_system_schedule_reboot(handle->wifi_system, delay_ms);
}

esp_err_t api_usecase_get_factory_reset_report(api_usecases_handle_t handle, api_factory_reset_report_t *out_report)
{
    esp_err_t ret = require_wifi_system(handle);
    if (ret != ESP_OK || !out_report) {
        return ESP_ERR_INVALID_ARG;
    }

    gateway_core_factory_reset_report_t report = {0};
    ret = gateway_wifi_system_get_factory_reset_report(handle->wifi_system, &report);
    if (ret != ESP_OK) {
        return ret;
    }
    out_report->wifi_err = report.wifi_err;
    out_report->devices_err = report.devices_err;
    out_report->zigbee_storage_err = report.zigbee_storage_err;
    out_report->zigbee_fct_err = report.zigbee_fct_err;
    return ESP_OK;
}

static api_system_wifi_link_quality_t to_api_wifi_link_quality(gateway_core_wifi_link_quality_t quality)
{
    switch (quality) {
    case GATEWAY_CORE_WIFI_LINK_GOOD:
        return API_WIFI_LINK_GOOD;
    case GATEWAY_CORE_WIFI_LINK_WARN:
        return API_WIFI_LINK_WARN;
    case GATEWAY_CORE_WIFI_LINK_BAD:
        return API_WIFI_LINK_BAD;
    case GATEWAY_CORE_WIFI_LINK_UNKNOWN:
    default:
        return API_WIFI_LINK_UNKNOWN;
    }
}

esp_err_t api_usecase_collect_telemetry(api_usecases_handle_t handle, api_system_telemetry_t *out)
{
    esp_err_t ret = require_wifi_system(handle);
    if (ret != ESP_OK || !out) {
        return ESP_ERR_INVALID_ARG;
    }

    gateway_core_telemetry_t telemetry = {0};
    ret = gateway_wifi_system_collect_telemetry(handle->wifi_system, &telemetry);
    if (ret != ESP_OK) {
        return ret;
    }

    memset(out, 0, sizeof(*out));
    out->uptime_ms = telemetry.uptime_ms;
    out->heap_free = telemetry.heap_free;
    out->heap_min = telemetry.heap_min;
    out->heap_largest_block = telemetry.heap_largest_block;
    out->main_stack_hwm_bytes = telemetry.main_stack_hwm_bytes;
    out->httpd_stack_hwm_bytes = telemetry.httpd_stack_hwm_bytes;
    out->has_temperature_c = telemetry.has_temperature_c;
    out->temperature_c = telemetry.temperature_c;
    out->has_wifi_rssi = telemetry.has_wifi_rssi;
    out->wifi_rssi = telemetry.wifi_rssi;
    out->has_wifi_ip = telemetry.has_wifi_ip;
    memcpy(out->wifi_ip, telemetry.wifi_ip, sizeof(out->wifi_ip));
    out->wifi_link_quality = to_api_wifi_link_quality(telemetry.wifi_link_quality);
    return ESP_OK;
}

esp_err_t api_usecase_collect_health_snapshot(api_usecases_handle_t handle, api_health_snapshot_t *out)
{
    esp_err_t ret = require_handle(handle);
    if (ret != ESP_OK || !out) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(out, 0, sizeof(*out));

    gateway_network_state_t gw_state = {0};
    ret = gateway_wifi_system_get_network_state(handle->wifi_system, &gw_state);
    if (ret != ESP_OK) {
        return ret;
    }
    out->zigbee_started = gw_state.zigbee_started;
    out->zigbee_factory_new = gw_state.factory_new;
    out->zigbee_pan_id = gw_state.pan_id;
    out->zigbee_channel = gw_state.channel;
    out->zigbee_short_addr = gw_state.short_addr;

    gateway_wifi_state_t wifi_state = {0};
    ret = gateway_wifi_system_get_wifi_state(handle->wifi_system, &wifi_state);
    if (ret != ESP_OK) {
        return ret;
    }
    out->wifi_sta_connected = wifi_state.sta_connected;
    out->wifi_fallback_ap_active = wifi_state.fallback_ap_active;
    out->wifi_loaded_from_nvs = wifi_state.loaded_from_nvs;
    strlcpy(out->wifi_active_ssid, wifi_state.active_ssid, sizeof(out->wifi_active_ssid));

    int32_t schema_version = 0;
    ret = gateway_wifi_system_get_schema_version(handle->wifi_system, &schema_version);
    out->nvs_ok = (ret == ESP_OK);
    out->nvs_schema_version = (ret == ESP_OK) ? schema_version : -1;

    out->ws_clients = handle->ws_client_count_provider ? handle->ws_client_count_provider(handle->ws_provider_ctx) : 0;

    ret = api_usecase_collect_telemetry(handle, &out->telemetry);
    if (ret != ESP_OK) {
        return ret;
    }

    gateway_core_job_metrics_t job_metrics = {0};
    if (handle->jobs) {
        ret = gateway_jobs_get_metrics(handle->jobs, &job_metrics);
        if (ret == ESP_OK) {
            out->jobs_metrics.submitted_total = job_metrics.submitted_total;
            out->jobs_metrics.dedup_reused_total = job_metrics.dedup_reused_total;
            out->jobs_metrics.completed_total = job_metrics.completed_total;
            out->jobs_metrics.failed_total = job_metrics.failed_total;
            out->jobs_metrics.queue_depth_current = job_metrics.queue_depth_current;
            out->jobs_metrics.queue_depth_peak = job_metrics.queue_depth_peak;
            out->jobs_metrics.latency_p95_ms = job_metrics.latency_p95_ms;
        }
    }

    if (handle->ws_metrics_provider) {
        (void)handle->ws_metrics_provider(handle->ws_provider_ctx, &out->ws_metrics);
    }

    return ESP_OK;
}

esp_err_t api_usecase_jobs_submit(api_usecases_handle_t handle, gateway_core_job_type_t type, uint32_t reboot_delay_ms,
                                  uint32_t *out_job_id)
{
    esp_err_t ret = require_jobs(handle);
    if (ret != ESP_OK) {
        return ret;
    }
    return gateway_jobs_submit(handle->jobs, type, reboot_delay_ms, out_job_id);
}

esp_err_t api_usecase_jobs_get(api_usecases_handle_t handle, uint32_t job_id, gateway_core_job_info_t *out_info)
{
    esp_err_t ret = require_jobs(handle);
    if (ret != ESP_OK) {
        return ret;
    }
    return gateway_jobs_get(handle->jobs, job_id, out_info);
}
