#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "state_store.h"
#include "wifi_service.h"
#include "zigbee_service.h"

typedef enum {
    GATEWAY_CORE_WIFI_LINK_UNKNOWN = 0,
    GATEWAY_CORE_WIFI_LINK_GOOD,
    GATEWAY_CORE_WIFI_LINK_WARN,
    GATEWAY_CORE_WIFI_LINK_BAD,
} gateway_core_wifi_link_quality_t;

typedef struct {
    esp_err_t wifi_err;
    esp_err_t devices_err;
    esp_err_t zigbee_storage_err;
    esp_err_t zigbee_fct_err;
} gateway_core_factory_reset_report_t;

typedef struct {
    uint64_t uptime_ms;
    uint32_t heap_free;
    uint32_t heap_min;
    uint32_t heap_largest_block;
    int32_t main_stack_hwm_bytes;
    int32_t httpd_stack_hwm_bytes;
    bool has_temperature_c;
    float temperature_c;
    bool has_wifi_rssi;
    int32_t wifi_rssi;
    bool has_wifi_ip;
    char wifi_ip[20];
    gateway_core_wifi_link_quality_t wifi_link_quality;
} gateway_core_telemetry_t;

typedef struct {
    uint32_t submitted_total;
    uint32_t dedup_reused_total;
    uint32_t completed_total;
    uint32_t failed_total;
    uint32_t queue_depth_current;
    uint32_t queue_depth_peak;
    uint32_t latency_p95_ms;
} gateway_core_job_metrics_t;

typedef enum {
    GATEWAY_CORE_JOB_TYPE_WIFI_SCAN = 0,
    GATEWAY_CORE_JOB_TYPE_FACTORY_RESET,
    GATEWAY_CORE_JOB_TYPE_REBOOT,
    GATEWAY_CORE_JOB_TYPE_UPDATE,
    GATEWAY_CORE_JOB_TYPE_LQI_REFRESH,
} gateway_core_job_type_t;

typedef enum {
    GATEWAY_CORE_JOB_STATE_QUEUED = 0,
    GATEWAY_CORE_JOB_STATE_RUNNING,
    GATEWAY_CORE_JOB_STATE_SUCCEEDED,
    GATEWAY_CORE_JOB_STATE_FAILED,
} gateway_core_job_state_t;

typedef struct {
    uint32_t id;
    gateway_core_job_type_t type;
    gateway_core_job_state_t state;
    esp_err_t err;
    uint64_t created_ms;
    uint64_t updated_ms;
    bool has_result;
    char result_json[2048];
} gateway_core_job_info_t;

esp_err_t gateway_core_facade_send_on_off(uint16_t short_addr, uint8_t endpoint, uint8_t on_off);
esp_err_t gateway_core_facade_wifi_save_credentials(const char *ssid, const char *password);
esp_err_t gateway_core_facade_schedule_reboot(uint32_t delay_ms);
esp_err_t gateway_core_facade_factory_reset_and_reboot(uint32_t reboot_delay_ms);

esp_err_t gateway_core_facade_get_network_status(zigbee_network_status_t *out_status);
int gateway_core_facade_get_devices_snapshot(zb_device_t *out_devices, int max_devices);
int gateway_core_facade_get_neighbor_lqi_snapshot(zigbee_neighbor_lqi_t *out_neighbors, int max_neighbors);
esp_err_t gateway_core_facade_get_cached_lqi_snapshot(zigbee_neighbor_lqi_t *out_neighbors, int max_neighbors, int *out_count,
                                                      zigbee_lqi_source_t *out_source, uint64_t *out_updated_ms);
esp_err_t gateway_core_facade_permit_join(uint8_t duration_seconds);
esp_err_t gateway_core_facade_delete_device(uint16_t short_addr);
esp_err_t gateway_core_facade_rename_device(uint16_t short_addr, const char *name);

esp_err_t gateway_core_facade_wifi_scan(wifi_ap_info_t **out_list, size_t *out_count);
void gateway_core_facade_wifi_scan_free(wifi_ap_info_t *list);

esp_err_t gateway_core_facade_get_factory_reset_report(gateway_core_factory_reset_report_t *out_report);
esp_err_t gateway_core_facade_collect_telemetry(gateway_core_telemetry_t *out);
esp_err_t gateway_core_facade_get_job_metrics(gateway_core_job_metrics_t *out_metrics);
esp_err_t gateway_core_facade_job_submit(gateway_core_job_type_t type, uint32_t reboot_delay_ms, uint32_t *out_job_id);
esp_err_t gateway_core_facade_job_get(uint32_t job_id, gateway_core_job_info_t *out_info);
const char *gateway_core_facade_job_type_to_string(gateway_core_job_type_t type);
const char *gateway_core_facade_job_state_to_string(gateway_core_job_state_t state);

esp_err_t gateway_core_facade_get_network_state(gateway_network_state_t *out_state);
esp_err_t gateway_core_facade_get_wifi_state(gateway_wifi_state_t *out_state);
esp_err_t gateway_core_facade_get_schema_version(int32_t *out_version);
