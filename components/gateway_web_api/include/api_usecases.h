#pragma once

#include "esp_err.h"
#include "api_contracts.h"
#include "gateway_device_zigbee_facade.h"
#include "gateway_jobs_facade.h"
#include "gateway_wifi_system_facade.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    esp_err_t (*send_on_off)(uint16_t short_addr, uint8_t endpoint, uint8_t on_off);
    esp_err_t (*wifi_save_credentials)(const char *ssid, const char *password);
    esp_err_t (*schedule_reboot)(uint32_t delay_ms);
    esp_err_t (*factory_reset_and_reboot)(uint32_t reboot_delay_ms);
} api_service_ops_t;

typedef struct {
    esp_err_t wifi_err;
    esp_err_t devices_err;
    esp_err_t zigbee_storage_err;
    esp_err_t zigbee_fct_err;
} api_factory_reset_report_t;

typedef enum {
    API_WIFI_LINK_UNKNOWN = 0,
    API_WIFI_LINK_GOOD,
    API_WIFI_LINK_WARN,
    API_WIFI_LINK_BAD,
} api_system_wifi_link_quality_t;

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
    api_system_wifi_link_quality_t wifi_link_quality;
} api_system_telemetry_t;

typedef struct {
    uint32_t submitted_total;
    uint32_t dedup_reused_total;
    uint32_t completed_total;
    uint32_t failed_total;
    uint32_t queue_depth_current;
    uint32_t queue_depth_peak;
    uint32_t latency_p95_ms;
} api_job_runtime_metrics_t;

typedef struct {
    uint32_t dropped_frames_total;
    uint32_t reconnect_count;
    uint32_t connections_total;
    uint32_t broadcast_lock_skips_total;
} api_ws_runtime_metrics_t;

typedef struct {
    bool zigbee_started;
    bool zigbee_factory_new;
    uint32_t zigbee_pan_id;
    uint32_t zigbee_channel;
    uint32_t zigbee_short_addr;

    bool wifi_sta_connected;
    bool wifi_fallback_ap_active;
    bool wifi_loaded_from_nvs;
    char wifi_active_ssid[33];

    bool nvs_ok;
    int32_t nvs_schema_version;
    uint32_t ws_clients;

    api_system_telemetry_t telemetry;
    api_job_runtime_metrics_t jobs_metrics;
    api_ws_runtime_metrics_t ws_metrics;
} api_health_snapshot_t;

typedef struct api_usecases api_usecases_t;
typedef api_usecases_t *api_usecases_handle_t;

typedef uint32_t (*api_ws_client_count_provider_t)(void *ctx);
typedef bool (*api_ws_metrics_provider_t)(void *ctx, api_ws_runtime_metrics_t *out_metrics);

typedef struct {
    const api_service_ops_t *service_ops;
    gateway_wifi_system_handle_t wifi_system;
    gateway_jobs_handle_t jobs;
    api_ws_client_count_provider_t ws_client_count_provider;
    api_ws_metrics_provider_t ws_metrics_provider;
    void *ws_provider_ctx;
} api_usecases_init_params_t;

esp_err_t api_usecases_create(const api_usecases_init_params_t *params, api_usecases_handle_t *out_handle);
void api_usecases_destroy(api_usecases_handle_t handle);

void api_usecases_set_service_ops_with_handle(api_usecases_handle_t handle, const api_service_ops_t *ops);
void api_usecases_set_runtime_handles(api_usecases_handle_t handle, gateway_wifi_system_handle_t wifi_system,
                                      gateway_jobs_handle_t jobs);
void api_usecases_set_ws_providers(api_usecases_handle_t handle, api_ws_client_count_provider_t count_provider,
                                   api_ws_metrics_provider_t metrics_provider, void *provider_ctx);

esp_err_t api_usecase_control(api_usecases_handle_t handle, const api_control_request_t *in);
esp_err_t api_usecase_wifi_save(api_usecases_handle_t handle, const api_wifi_save_request_t *in);
esp_err_t api_usecase_factory_reset(api_usecases_handle_t handle);
esp_err_t api_usecase_get_network_status(api_usecases_handle_t handle, zigbee_network_status_t *out_status);
int api_usecase_get_devices_snapshot(api_usecases_handle_t handle, zb_device_t *out_devices, int max_devices);
int api_usecase_get_neighbor_lqi_snapshot(api_usecases_handle_t handle, zigbee_neighbor_lqi_t *out_neighbors, int max_neighbors);
esp_err_t api_usecase_get_cached_lqi_snapshot(api_usecases_handle_t handle, zigbee_neighbor_lqi_t *out_neighbors,
                                              int max_neighbors, int *out_count, zigbee_lqi_source_t *out_source,
                                              uint64_t *out_updated_ms);
esp_err_t api_usecase_permit_join(api_usecases_handle_t handle, uint8_t duration_seconds);
esp_err_t api_usecase_delete_device(api_usecases_handle_t handle, uint16_t short_addr);
esp_err_t api_usecase_rename_device(api_usecases_handle_t handle, uint16_t short_addr, const char *name);
esp_err_t api_usecase_wifi_scan(api_usecases_handle_t handle, wifi_ap_info_t **out_list, size_t *out_count);
void api_usecase_wifi_scan_free(api_usecases_handle_t handle, wifi_ap_info_t *list);
esp_err_t api_usecase_schedule_reboot(api_usecases_handle_t handle, uint32_t delay_ms);
esp_err_t api_usecase_get_factory_reset_report(api_usecases_handle_t handle, api_factory_reset_report_t *out_report);
esp_err_t api_usecase_collect_telemetry(api_usecases_handle_t handle, api_system_telemetry_t *out);
esp_err_t api_usecase_collect_health_snapshot(api_usecases_handle_t handle, api_health_snapshot_t *out);
esp_err_t api_usecase_jobs_submit(api_usecases_handle_t handle, gateway_core_job_type_t type, uint32_t reboot_delay_ms,
                                  uint32_t *out_job_id);
esp_err_t api_usecase_jobs_get(api_usecases_handle_t handle, uint32_t job_id, gateway_core_job_info_t *out_info);
