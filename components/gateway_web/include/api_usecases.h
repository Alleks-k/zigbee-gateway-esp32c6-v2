#pragma once

#include "esp_err.h"
#include "api_contracts.h"
#include "zigbee_service.h"
#include "wifi_service.h"
#include "system_service.h"
#include <stdint.h>

typedef struct {
    esp_err_t (*send_on_off)(uint16_t short_addr, uint8_t endpoint, uint8_t on_off);
    esp_err_t (*wifi_save_credentials)(const char *ssid, const char *password);
    esp_err_t (*schedule_reboot)(uint32_t delay_ms);
    esp_err_t (*factory_reset_and_reboot)(uint32_t reboot_delay_ms);
} api_service_ops_t;

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

    system_telemetry_t telemetry;
} api_health_snapshot_t;

typedef uint32_t (*api_ws_client_count_provider_t)(void);

void api_usecases_set_service_ops(const api_service_ops_t *ops);
void api_usecases_set_ws_client_count_provider(api_ws_client_count_provider_t provider);

esp_err_t api_usecase_control(const api_control_request_t *in);
esp_err_t api_usecase_wifi_save(const api_wifi_save_request_t *in);
esp_err_t api_usecase_factory_reset(void);
esp_err_t api_usecase_get_network_status(zigbee_network_status_t *out_status);
int api_usecase_get_devices_snapshot(zb_device_t *out_devices, int max_devices);
int api_usecase_get_neighbor_lqi_snapshot(zigbee_neighbor_lqi_t *out_neighbors, int max_neighbors);
esp_err_t api_usecase_get_cached_lqi_snapshot(zigbee_neighbor_lqi_t *out_neighbors, int max_neighbors, int *out_count,
                                              zigbee_lqi_source_t *out_source, uint64_t *out_updated_ms);
esp_err_t api_usecase_permit_join(uint8_t duration_seconds);
esp_err_t api_usecase_delete_device(uint16_t short_addr);
esp_err_t api_usecase_rename_device(uint16_t short_addr, const char *name);
esp_err_t api_usecase_wifi_scan(wifi_ap_info_t **out_list, size_t *out_count);
void api_usecase_wifi_scan_free(wifi_ap_info_t *list);
esp_err_t api_usecase_schedule_reboot(uint32_t delay_ms);
esp_err_t api_usecase_get_factory_reset_report(system_factory_reset_report_t *out_report);
esp_err_t api_usecase_collect_telemetry(system_telemetry_t *out);
esp_err_t api_usecase_collect_health_snapshot(api_health_snapshot_t *out);
