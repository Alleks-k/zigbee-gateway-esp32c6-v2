#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "gateway_runtime_context.h"
#include "state_store.h"
#include "wifi_service.h"

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

esp_err_t gateway_wifi_system_init(const gateway_runtime_context_t *ctx);

esp_err_t gateway_wifi_system_save_credentials(const char *ssid, const char *password);
esp_err_t gateway_wifi_system_schedule_reboot(uint32_t delay_ms);
esp_err_t gateway_wifi_system_factory_reset_and_reboot(uint32_t reboot_delay_ms);

esp_err_t gateway_wifi_system_scan(wifi_ap_info_t **out_list, size_t *out_count);
void gateway_wifi_system_scan_free(wifi_ap_info_t *list);

esp_err_t gateway_wifi_system_get_factory_reset_report(gateway_core_factory_reset_report_t *out_report);
esp_err_t gateway_wifi_system_collect_telemetry(gateway_core_telemetry_t *out);
esp_err_t gateway_wifi_system_get_network_state(gateway_network_state_t *out_state);
esp_err_t gateway_wifi_system_get_wifi_state(gateway_wifi_state_t *out_state);
esp_err_t gateway_wifi_system_get_schema_version(int32_t *out_version);
