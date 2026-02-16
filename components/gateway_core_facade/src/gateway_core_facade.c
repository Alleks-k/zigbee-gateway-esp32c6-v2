#include "gateway_core_facade.h"

#include <string.h>

#include "config_service.h"
#include "job_queue.h"
#include "system_service.h"

static gateway_state_handle_t s_gateway_state = NULL;

static esp_err_t ensure_gateway_state_handle(void)
{
    if (s_gateway_state) {
        return ESP_OK;
    }
    return gateway_state_get_default(&s_gateway_state);
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

static zgw_job_type_t to_job_type(gateway_core_job_type_t type)
{
    switch (type) {
    case GATEWAY_CORE_JOB_TYPE_FACTORY_RESET:
        return ZGW_JOB_TYPE_FACTORY_RESET;
    case GATEWAY_CORE_JOB_TYPE_REBOOT:
        return ZGW_JOB_TYPE_REBOOT;
    case GATEWAY_CORE_JOB_TYPE_UPDATE:
        return ZGW_JOB_TYPE_UPDATE;
    case GATEWAY_CORE_JOB_TYPE_LQI_REFRESH:
        return ZGW_JOB_TYPE_LQI_REFRESH;
    case GATEWAY_CORE_JOB_TYPE_WIFI_SCAN:
    default:
        return ZGW_JOB_TYPE_WIFI_SCAN;
    }
}

static gateway_core_job_type_t from_job_type(zgw_job_type_t type)
{
    switch (type) {
    case ZGW_JOB_TYPE_FACTORY_RESET:
        return GATEWAY_CORE_JOB_TYPE_FACTORY_RESET;
    case ZGW_JOB_TYPE_REBOOT:
        return GATEWAY_CORE_JOB_TYPE_REBOOT;
    case ZGW_JOB_TYPE_UPDATE:
        return GATEWAY_CORE_JOB_TYPE_UPDATE;
    case ZGW_JOB_TYPE_LQI_REFRESH:
        return GATEWAY_CORE_JOB_TYPE_LQI_REFRESH;
    case ZGW_JOB_TYPE_WIFI_SCAN:
    default:
        return GATEWAY_CORE_JOB_TYPE_WIFI_SCAN;
    }
}

static gateway_core_job_state_t from_job_state(zgw_job_state_t state)
{
    switch (state) {
    case ZGW_JOB_STATE_RUNNING:
        return GATEWAY_CORE_JOB_STATE_RUNNING;
    case ZGW_JOB_STATE_SUCCEEDED:
        return GATEWAY_CORE_JOB_STATE_SUCCEEDED;
    case ZGW_JOB_STATE_FAILED:
        return GATEWAY_CORE_JOB_STATE_FAILED;
    case ZGW_JOB_STATE_QUEUED:
    default:
        return GATEWAY_CORE_JOB_STATE_QUEUED;
    }
}

esp_err_t gateway_core_facade_send_on_off(uint16_t short_addr, uint8_t endpoint, uint8_t on_off)
{
    return zigbee_service_send_on_off(short_addr, endpoint, on_off);
}

esp_err_t gateway_core_facade_wifi_save_credentials(const char *ssid, const char *password)
{
    return wifi_service_save_credentials(ssid, password);
}

esp_err_t gateway_core_facade_schedule_reboot(uint32_t delay_ms)
{
    return system_service_schedule_reboot(delay_ms);
}

esp_err_t gateway_core_facade_factory_reset_and_reboot(uint32_t reboot_delay_ms)
{
    return system_service_factory_reset_and_reboot(reboot_delay_ms);
}

esp_err_t gateway_core_facade_get_network_status(zigbee_network_status_t *out_status)
{
    return zigbee_service_get_network_status(out_status);
}

int gateway_core_facade_get_devices_snapshot(zb_device_t *out_devices, int max_devices)
{
    if (!out_devices || max_devices <= 0) {
        return 0;
    }
    return zigbee_service_get_devices_snapshot(out_devices, (size_t)max_devices);
}

int gateway_core_facade_get_neighbor_lqi_snapshot(zigbee_neighbor_lqi_t *out_neighbors, int max_neighbors)
{
    if (!out_neighbors || max_neighbors <= 0) {
        return 0;
    }
    return zigbee_service_get_neighbor_lqi_snapshot(out_neighbors, (size_t)max_neighbors);
}

esp_err_t gateway_core_facade_get_cached_lqi_snapshot(zigbee_neighbor_lqi_t *out_neighbors, int max_neighbors, int *out_count,
                                                      zigbee_lqi_source_t *out_source, uint64_t *out_updated_ms)
{
    if (!out_neighbors || max_neighbors <= 0 || !out_count) {
        return ESP_ERR_INVALID_ARG;
    }
    return zigbee_service_get_cached_lqi_snapshot(out_neighbors, (size_t)max_neighbors, out_count, out_source, out_updated_ms);
}

esp_err_t gateway_core_facade_permit_join(uint8_t duration_seconds)
{
    return zigbee_service_permit_join(duration_seconds);
}

esp_err_t gateway_core_facade_delete_device(uint16_t short_addr)
{
    return zigbee_service_delete_device(short_addr);
}

esp_err_t gateway_core_facade_rename_device(uint16_t short_addr, const char *name)
{
    return zigbee_service_rename_device(short_addr, name);
}

esp_err_t gateway_core_facade_wifi_scan(wifi_ap_info_t **out_list, size_t *out_count)
{
    return wifi_service_scan(out_list, out_count);
}

void gateway_core_facade_wifi_scan_free(wifi_ap_info_t *list)
{
    wifi_service_scan_free(list);
}

esp_err_t gateway_core_facade_get_factory_reset_report(gateway_core_factory_reset_report_t *out_report)
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

esp_err_t gateway_core_facade_collect_telemetry(gateway_core_telemetry_t *out)
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
    memcpy(out->wifi_ip, telemetry.wifi_ip, sizeof(out->wifi_ip));
    out->wifi_link_quality = to_core_wifi_link_quality(telemetry.wifi_link_quality);
    return ESP_OK;
}

esp_err_t gateway_core_facade_get_job_metrics(gateway_core_job_metrics_t *out_metrics)
{
    if (!out_metrics) {
        return ESP_ERR_INVALID_ARG;
    }

    zgw_job_metrics_t metrics = {0};
    esp_err_t err = job_queue_get_metrics(&metrics);
    if (err != ESP_OK) {
        return err;
    }

    out_metrics->submitted_total = metrics.submitted_total;
    out_metrics->dedup_reused_total = metrics.dedup_reused_total;
    out_metrics->completed_total = metrics.completed_total;
    out_metrics->failed_total = metrics.failed_total;
    out_metrics->queue_depth_current = metrics.queue_depth_current;
    out_metrics->queue_depth_peak = metrics.queue_depth_peak;
    out_metrics->latency_p95_ms = metrics.latency_p95_ms;
    return ESP_OK;
}

esp_err_t gateway_core_facade_job_submit(gateway_core_job_type_t type, uint32_t reboot_delay_ms, uint32_t *out_job_id)
{
    return job_queue_submit(to_job_type(type), reboot_delay_ms, out_job_id);
}

esp_err_t gateway_core_facade_job_get(uint32_t job_id, gateway_core_job_info_t *out_info)
{
    if (!out_info) {
        return ESP_ERR_INVALID_ARG;
    }

    zgw_job_info_t info = {0};
    esp_err_t err = job_queue_get(job_id, &info);
    if (err != ESP_OK) {
        return err;
    }

    out_info->id = info.id;
    out_info->type = from_job_type(info.type);
    out_info->state = from_job_state(info.state);
    out_info->err = info.err;
    out_info->created_ms = info.created_ms;
    out_info->updated_ms = info.updated_ms;
    out_info->has_result = info.has_result;
    memcpy(out_info->result_json, info.result_json, sizeof(out_info->result_json));
    return ESP_OK;
}

const char *gateway_core_facade_job_type_to_string(gateway_core_job_type_t type)
{
    return job_queue_type_to_string(to_job_type(type));
}

const char *gateway_core_facade_job_state_to_string(gateway_core_job_state_t state)
{
    switch (state) {
    case GATEWAY_CORE_JOB_STATE_RUNNING:
        return job_queue_state_to_string(ZGW_JOB_STATE_RUNNING);
    case GATEWAY_CORE_JOB_STATE_SUCCEEDED:
        return job_queue_state_to_string(ZGW_JOB_STATE_SUCCEEDED);
    case GATEWAY_CORE_JOB_STATE_FAILED:
        return job_queue_state_to_string(ZGW_JOB_STATE_FAILED);
    case GATEWAY_CORE_JOB_STATE_QUEUED:
    default:
        return job_queue_state_to_string(ZGW_JOB_STATE_QUEUED);
    }
}

esp_err_t gateway_core_facade_get_network_state(gateway_network_state_t *out_state)
{
    esp_err_t ret = ensure_gateway_state_handle();
    if (ret != ESP_OK) {
        return ret;
    }
    return gateway_state_get_network(s_gateway_state, out_state);
}

esp_err_t gateway_core_facade_get_wifi_state(gateway_wifi_state_t *out_state)
{
    esp_err_t ret = ensure_gateway_state_handle();
    if (ret != ESP_OK) {
        return ret;
    }
    return gateway_state_get_wifi(s_gateway_state, out_state);
}

esp_err_t gateway_core_facade_get_schema_version(int32_t *out_version)
{
    return config_service_get_schema_version(out_version);
}
