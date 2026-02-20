#include "gateway_wifi_system_facade.h"

#include <stdlib.h>
#include <string.h>

#include "config_service.h"
#include "gateway_status_esp.h"
#include "state_store.h"
#include "system_service.h"
#include "wifi_service.h"

struct gateway_wifi_system {
    gateway_state_handle_t gateway_state;
};

esp_err_t gateway_wifi_system_create(const gateway_wifi_system_init_params_t *params,
                                     gateway_wifi_system_handle_t *out_handle)
{
    if (!params || !params->gateway_state_handle || !out_handle) {
        return ESP_ERR_INVALID_ARG;
    }

    gateway_wifi_system_t *handle = (gateway_wifi_system_t *)calloc(1, sizeof(*handle));
    if (!handle) {
        return ESP_ERR_NO_MEM;
    }

    handle->gateway_state = params->gateway_state_handle;
    *out_handle = handle;
    return ESP_OK;
}

void gateway_wifi_system_destroy(gateway_wifi_system_handle_t handle)
{
    free(handle);
}

static esp_err_t require_gateway_state_handle(gateway_wifi_system_handle_t handle)
{
    return (handle && handle->gateway_state) ? ESP_OK : ESP_ERR_INVALID_STATE;
}

static gateway_core_wifi_link_quality_t to_core_wifi_link_quality(system_wifi_link_quality_t quality)
{
    switch (quality) {
    case SYSTEM_WIFI_LINK_GOOD:
        return GATEWAY_CORE_WIFI_LINK_GOOD;
    case SYSTEM_WIFI_LINK_WARN:
        return GATEWAY_CORE_WIFI_LINK_WARN;
    case SYSTEM_WIFI_LINK_BAD:
        return GATEWAY_CORE_WIFI_LINK_BAD;
    case SYSTEM_WIFI_LINK_UNKNOWN:
    default:
        return GATEWAY_CORE_WIFI_LINK_UNKNOWN;
    }
}

esp_err_t gateway_wifi_system_save_credentials(gateway_wifi_system_handle_t handle, const char *ssid, const char *password)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    return wifi_service_save_credentials(ssid, password);
}

esp_err_t gateway_wifi_system_schedule_reboot(gateway_wifi_system_handle_t handle, uint32_t delay_ms)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    return system_service_schedule_reboot(delay_ms);
}

esp_err_t gateway_wifi_system_factory_reset_and_reboot(gateway_wifi_system_handle_t handle, uint32_t reboot_delay_ms)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    return system_service_factory_reset_and_reboot(reboot_delay_ms);
}

esp_err_t gateway_wifi_system_scan(gateway_wifi_system_handle_t handle, wifi_ap_info_t **out_list, size_t *out_count)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    return wifi_service_scan(out_list, out_count);
}

void gateway_wifi_system_scan_free(gateway_wifi_system_handle_t handle, wifi_ap_info_t *list)
{
    (void)handle;
    wifi_service_scan_free(list);
}

esp_err_t gateway_wifi_system_get_factory_reset_report(gateway_wifi_system_handle_t handle,
                                                       gateway_core_factory_reset_report_t *out_report)
{
    if (!handle || !out_report) {
        return ESP_ERR_INVALID_ARG;
    }

    system_factory_reset_report_t report = {0};
    esp_err_t err = system_service_get_last_factory_reset_report(&report);
    if (err != ESP_OK) {
        return err;
    }

    out_report->wifi_err = gateway_status_to_esp_err(report.wifi_err);
    out_report->devices_err = gateway_status_to_esp_err(report.devices_err);
    out_report->zigbee_storage_err = gateway_status_to_esp_err(report.zigbee_storage_err);
    out_report->zigbee_fct_err = gateway_status_to_esp_err(report.zigbee_fct_err);
    return ESP_OK;
}

esp_err_t gateway_wifi_system_collect_telemetry(gateway_wifi_system_handle_t handle, gateway_core_telemetry_t *out)
{
    if (!handle || !out) {
        return ESP_ERR_INVALID_ARG;
    }

    system_telemetry_t telemetry = {0};
    esp_err_t err = system_service_collect_telemetry(&telemetry);
    if (err != ESP_OK) {
        return err;
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
    out->wifi_link_quality = to_core_wifi_link_quality(telemetry.wifi_link_quality);
    return ESP_OK;
}

esp_err_t gateway_wifi_system_get_network_state(gateway_wifi_system_handle_t handle, gateway_network_state_t *out_state)
{
    esp_err_t ret = require_gateway_state_handle(handle);
    if (ret != ESP_OK) {
        return ret;
    }
    return gateway_status_to_esp_err(gateway_state_get_network(handle->gateway_state, out_state));
}

esp_err_t gateway_wifi_system_get_wifi_state(gateway_wifi_system_handle_t handle, gateway_wifi_state_t *out_state)
{
    esp_err_t ret = require_gateway_state_handle(handle);
    if (ret != ESP_OK) {
        return ret;
    }
    return gateway_status_to_esp_err(gateway_state_get_wifi(handle->gateway_state, out_state));
}

esp_err_t gateway_wifi_system_get_schema_version(gateway_wifi_system_handle_t handle, int32_t *out_version)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    return gateway_status_to_esp_err(config_service_get_schema_version(out_version));
}
