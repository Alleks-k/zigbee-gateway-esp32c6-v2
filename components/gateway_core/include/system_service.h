#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

typedef struct {
    esp_err_t wifi_err;
    esp_err_t devices_err;
    esp_err_t zigbee_storage_err;
    esp_err_t zigbee_fct_err;
} system_factory_reset_report_t;

typedef enum {
    SYSTEM_WIFI_LINK_UNKNOWN = 0,
    SYSTEM_WIFI_LINK_GOOD,
    SYSTEM_WIFI_LINK_WARN,
    SYSTEM_WIFI_LINK_BAD,
} system_wifi_link_quality_t;

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
    system_wifi_link_quality_t wifi_link_quality;
} system_telemetry_t;

typedef esp_err_t (*system_service_telemetry_impl_t)(system_telemetry_t *out);

void system_service_register_telemetry_impl(system_service_telemetry_impl_t impl);

void system_service_reboot(void);
esp_err_t system_service_schedule_reboot(uint32_t delay_ms);
esp_err_t system_service_factory_reset_and_reboot(uint32_t reboot_delay_ms);
esp_err_t system_service_get_last_factory_reset_report(system_factory_reset_report_t *out_report);
esp_err_t system_service_collect_telemetry(system_telemetry_t *out);

#if CONFIG_GATEWAY_SELF_TEST_APP
bool system_service_is_reboot_scheduled_for_test(void);
uint32_t system_service_get_reboot_schedule_count_for_test(void);
void system_service_reset_reboot_singleflight_for_test(void);
#endif
