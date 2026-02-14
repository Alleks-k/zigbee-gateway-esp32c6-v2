#include "api_usecases.h"
#include "zigbee_service.h"
#include "wifi_service.h"
#include "system_service.h"
#include "gateway_state.h"
#include "settings_manager.h"
#include "job_queue.h"
#include <string.h>

static esp_err_t real_send_on_off(uint16_t short_addr, uint8_t endpoint, uint8_t on_off)
{
    return zigbee_service_send_on_off(short_addr, endpoint, on_off);
}

static esp_err_t real_wifi_save_credentials(const char *ssid, const char *password)
{
    return wifi_service_save_credentials(ssid, password);
}

static esp_err_t real_schedule_reboot(uint32_t delay_ms)
{
    return system_service_schedule_reboot(delay_ms);
}

static esp_err_t real_factory_reset_and_reboot(uint32_t reboot_delay_ms)
{
    return system_service_factory_reset_and_reboot(reboot_delay_ms);
}

static const api_service_ops_t s_real_ops = {
    .send_on_off = real_send_on_off,
    .wifi_save_credentials = real_wifi_save_credentials,
    .schedule_reboot = real_schedule_reboot,
    .factory_reset_and_reboot = real_factory_reset_and_reboot,
};

static const api_service_ops_t *s_ops = &s_real_ops;
static api_ws_client_count_provider_t s_ws_client_count_provider = NULL;
static api_ws_metrics_provider_t s_ws_metrics_provider = NULL;

void api_usecases_set_service_ops(const api_service_ops_t *ops)
{
    s_ops = ops ? ops : &s_real_ops;
}

void api_usecases_set_ws_client_count_provider(api_ws_client_count_provider_t provider)
{
    s_ws_client_count_provider = provider;
}

void api_usecases_set_ws_metrics_provider(api_ws_metrics_provider_t provider)
{
    s_ws_metrics_provider = provider;
}

esp_err_t api_usecase_control(const api_control_request_t *in)
{
    if (!in || !s_ops || !s_ops->send_on_off) {
        return ESP_ERR_INVALID_ARG;
    }
    return s_ops->send_on_off(in->addr, in->ep, in->cmd);
}

esp_err_t api_usecase_wifi_save(const api_wifi_save_request_t *in)
{
    if (!in || !s_ops || !s_ops->wifi_save_credentials || !s_ops->schedule_reboot) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = s_ops->wifi_save_credentials(in->ssid, in->password);
    if (err != ESP_OK) {
        return err;
    }
    return s_ops->schedule_reboot(1000);
}

esp_err_t api_usecase_factory_reset(void)
{
    if (!s_ops || !s_ops->factory_reset_and_reboot) {
        return ESP_ERR_INVALID_ARG;
    }
    return s_ops->factory_reset_and_reboot(1000);
}

esp_err_t api_usecase_get_network_status(zigbee_network_status_t *out_status)
{
    if (!out_status) {
        return ESP_ERR_INVALID_ARG;
    }
    return zigbee_service_get_network_status(out_status);
}

int api_usecase_get_devices_snapshot(zb_device_t *out_devices, int max_devices)
{
    return zigbee_service_get_devices_snapshot(out_devices, max_devices);
}

int api_usecase_get_neighbor_lqi_snapshot(zigbee_neighbor_lqi_t *out_neighbors, int max_neighbors)
{
    if (!out_neighbors || max_neighbors <= 0) {
        return 0;
    }
    return zigbee_service_get_neighbor_lqi_snapshot(out_neighbors, (size_t)max_neighbors);
}

esp_err_t api_usecase_get_cached_lqi_snapshot(zigbee_neighbor_lqi_t *out_neighbors, int max_neighbors, int *out_count,
                                              zigbee_lqi_source_t *out_source, uint64_t *out_updated_ms)
{
    if (!out_neighbors || max_neighbors <= 0 || !out_count) {
        return ESP_ERR_INVALID_ARG;
    }
    return zigbee_service_get_cached_lqi_snapshot(out_neighbors, (size_t)max_neighbors, out_count, out_source, out_updated_ms);
}

esp_err_t api_usecase_permit_join(uint8_t duration_seconds)
{
    return zigbee_service_permit_join(duration_seconds);
}

esp_err_t api_usecase_delete_device(uint16_t short_addr)
{
    return zigbee_service_delete_device(short_addr);
}

esp_err_t api_usecase_rename_device(uint16_t short_addr, const char *name)
{
    if (!name) {
        return ESP_ERR_INVALID_ARG;
    }
    return zigbee_service_rename_device(short_addr, name);
}

esp_err_t api_usecase_wifi_scan(wifi_ap_info_t **out_list, size_t *out_count)
{
    return wifi_service_scan(out_list, out_count);
}

void api_usecase_wifi_scan_free(wifi_ap_info_t *list)
{
    wifi_service_scan_free(list);
}

esp_err_t api_usecase_schedule_reboot(uint32_t delay_ms)
{
    return system_service_schedule_reboot(delay_ms);
}

esp_err_t api_usecase_get_factory_reset_report(api_factory_reset_report_t *out_report)
{
    if (!out_report) {
        return ESP_ERR_INVALID_ARG;
    }
    system_factory_reset_report_t report = {0};
    esp_err_t err = system_service_get_last_factory_reset_report(&report);
    if (err != ESP_OK) {
        return err;
    }
    out_report->wifi_err = report.wifi_err;
    out_report->devices_err = report.devices_err;
    out_report->zigbee_storage_err = report.zigbee_storage_err;
    out_report->zigbee_fct_err = report.zigbee_fct_err;
    return ESP_OK;
}

static api_system_wifi_link_quality_t to_api_wifi_link_quality(system_wifi_link_quality_t quality)
{
    switch (quality) {
    case SYSTEM_WIFI_LINK_GOOD:
        return API_WIFI_LINK_GOOD;
    case SYSTEM_WIFI_LINK_WARN:
        return API_WIFI_LINK_WARN;
    case SYSTEM_WIFI_LINK_BAD:
        return API_WIFI_LINK_BAD;
    case SYSTEM_WIFI_LINK_UNKNOWN:
    default:
        return API_WIFI_LINK_UNKNOWN;
    }
}

esp_err_t api_usecase_collect_telemetry(api_system_telemetry_t *out)
{
    if (!out) {
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
    strlcpy(out->wifi_ip, telemetry.wifi_ip, sizeof(out->wifi_ip));
    out->wifi_link_quality = to_api_wifi_link_quality(telemetry.wifi_link_quality);
    return ESP_OK;
}

esp_err_t api_usecase_collect_health_snapshot(api_health_snapshot_t *out)
{
    if (!out) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));

    gateway_network_state_t gw_state = {0};
    esp_err_t err = gateway_state_get_network(&gw_state);
    if (err != ESP_OK) {
        return err;
    }
    out->zigbee_started = gw_state.zigbee_started;
    out->zigbee_factory_new = gw_state.factory_new;
    out->zigbee_pan_id = gw_state.pan_id;
    out->zigbee_channel = gw_state.channel;
    out->zigbee_short_addr = gw_state.short_addr;

    gateway_wifi_state_t wifi_state = {0};
    err = gateway_state_get_wifi(&wifi_state);
    if (err != ESP_OK) {
        return err;
    }
    out->wifi_sta_connected = wifi_state.sta_connected;
    out->wifi_fallback_ap_active = wifi_state.fallback_ap_active;
    out->wifi_loaded_from_nvs = wifi_state.loaded_from_nvs;
    strlcpy(out->wifi_active_ssid, wifi_state.active_ssid, sizeof(out->wifi_active_ssid));

    int32_t schema_version = 0;
    err = settings_manager_get_schema_version(&schema_version);
    out->nvs_ok = (err == ESP_OK);
    out->nvs_schema_version = (err == ESP_OK) ? schema_version : -1;

    out->ws_clients = s_ws_client_count_provider ? s_ws_client_count_provider() : 0;

    err = api_usecase_collect_telemetry(&out->telemetry);
    if (err != ESP_OK) {
        return err;
    }

    zgw_job_metrics_t job_metrics = {0};
    err = job_queue_get_metrics(&job_metrics);
    if (err == ESP_OK) {
        out->jobs_metrics.submitted_total = job_metrics.submitted_total;
        out->jobs_metrics.dedup_reused_total = job_metrics.dedup_reused_total;
        out->jobs_metrics.completed_total = job_metrics.completed_total;
        out->jobs_metrics.failed_total = job_metrics.failed_total;
        out->jobs_metrics.queue_depth_current = job_metrics.queue_depth_current;
        out->jobs_metrics.queue_depth_peak = job_metrics.queue_depth_peak;
        out->jobs_metrics.latency_p95_ms = job_metrics.latency_p95_ms;
    }
    if (s_ws_metrics_provider) {
        (void)s_ws_metrics_provider(&out->ws_metrics);
    }

    return ESP_OK;
}
