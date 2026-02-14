#include "system_service.h"
#include "settings_manager.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "lwip/ip4_addr.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if __has_include("driver/temperature_sensor.h")
#include "driver/temperature_sensor.h"
#define GATEWAY_HAS_TEMP_SENSOR 1
#else
#define GATEWAY_HAS_TEMP_SENSOR 0
#endif

static const char *TAG = "SYSTEM_SERVICE";

#if GATEWAY_HAS_TEMP_SENSOR
static temperature_sensor_handle_t s_temp_sensor_handle = NULL;
static bool s_temp_sensor_init_attempted = false;
static bool s_temp_sensor_available = false;
#endif

static bool read_wifi_rssi(int32_t *out_rssi)
{
    if (!out_rssi) {
        return false;
    }
    wifi_ap_record_t ap_info = {0};
    if (esp_wifi_sta_get_ap_info(&ap_info) != ESP_OK) {
        return false;
    }
    *out_rssi = (int32_t)ap_info.rssi;
    return true;
}

static bool read_wifi_ip(char *out, size_t out_size)
{
    if (!out || out_size < 8) {
        return false;
    }
    esp_netif_t *sta = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (!sta) {
        return false;
    }
    esp_netif_ip_info_t ip_info = {0};
    if (esp_netif_get_ip_info(sta, &ip_info) != ESP_OK || ip_info.ip.addr == 0) {
        return false;
    }
    int written = snprintf(out, out_size, IPSTR, IP2STR(&ip_info.ip));
    return (written > 0 && (size_t)written < out_size);
}

static int32_t get_stack_hwm_bytes(const char *task_name)
{
    if (!task_name) {
        return -1;
    }
    TaskHandle_t h = xTaskGetHandle(task_name);
    if (!h) {
        return -1;
    }
    UBaseType_t words = uxTaskGetStackHighWaterMark(h);
    return (int32_t)(words * sizeof(StackType_t));
}

static bool read_temperature_c(float *out_temp_c)
{
    if (!out_temp_c) {
        return false;
    }
#if GATEWAY_HAS_TEMP_SENSOR
    if (!s_temp_sensor_init_attempted) {
        s_temp_sensor_init_attempted = true;
        const struct {
            int min_c;
            int max_c;
        } ranges[] = {
            {10, 50},
            {20, 80},
            {0, 60},
        };
        for (size_t i = 0; i < (sizeof(ranges) / sizeof(ranges[0])); i++) {
            temperature_sensor_handle_t handle = NULL;
            temperature_sensor_config_t cfg = TEMPERATURE_SENSOR_CONFIG_DEFAULT(ranges[i].min_c, ranges[i].max_c);
            if (temperature_sensor_install(&cfg, &handle) == ESP_OK) {
                if (temperature_sensor_enable(handle) == ESP_OK) {
                    s_temp_sensor_handle = handle;
                    s_temp_sensor_available = true;
                    break;
                }
                (void)temperature_sensor_uninstall(handle);
            }
        }
        if (!s_temp_sensor_available) {
            s_temp_sensor_handle = NULL;
        }
    }
    if (!s_temp_sensor_available || s_temp_sensor_handle == NULL) {
        return false;
    }
    return (temperature_sensor_get_celsius(s_temp_sensor_handle, out_temp_c) == ESP_OK);
#else
    return false;
#endif
}

static system_wifi_link_quality_t wifi_link_quality_from_rssi(int32_t rssi, bool has_rssi)
{
    if (!has_rssi) {
        return SYSTEM_WIFI_LINK_UNKNOWN;
    }
    if (rssi >= -65) {
        return SYSTEM_WIFI_LINK_GOOD;
    }
    if (rssi >= -75) {
        return SYSTEM_WIFI_LINK_WARN;
    }
    return SYSTEM_WIFI_LINK_BAD;
}

static void reboot_task(void *arg)
{
    uint32_t delay_ms = (uint32_t)(uintptr_t)arg;
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
    esp_restart();
}

void system_service_reboot(void)
{
    ESP_LOGI(TAG, "Reboot requested by system service");
    esp_restart();
}

esp_err_t system_service_schedule_reboot(uint32_t delay_ms)
{
    BaseType_t ok = xTaskCreate(reboot_task, "zgw_reboot", 2048, (void *)(uintptr_t)delay_ms, 5, NULL);
    if (ok != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t system_service_factory_reset_and_reboot(uint32_t reboot_delay_ms)
{
    esp_err_t err = settings_manager_factory_reset();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Factory reset failed: %s", esp_err_to_name(err));
        return err;
    }
    return system_service_schedule_reboot(reboot_delay_ms);
}

esp_err_t system_service_get_last_factory_reset_report(system_factory_reset_report_t *out_report)
{
    if (!out_report) {
        return ESP_ERR_INVALID_ARG;
    }

    settings_manager_factory_reset_report_t report = {0};
    esp_err_t err = settings_manager_get_last_factory_reset_report(&report);
    if (err != ESP_OK) {
        return err;
    }

    out_report->wifi_err = report.wifi_err;
    out_report->devices_err = report.devices_err;
    out_report->zigbee_storage_err = report.zigbee_storage_err;
    out_report->zigbee_fct_err = report.zigbee_fct_err;
    return ESP_OK;
}

esp_err_t system_service_collect_telemetry(system_telemetry_t *out)
{
    if (!out) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(out, 0, sizeof(*out));
    out->uptime_ms = (uint64_t)(esp_timer_get_time() / 1000);
    out->heap_free = esp_get_free_heap_size();
    out->heap_min = esp_get_minimum_free_heap_size();
    out->heap_largest_block = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    out->main_stack_hwm_bytes = get_stack_hwm_bytes("main");
    out->httpd_stack_hwm_bytes = get_stack_hwm_bytes("httpd");
    out->has_wifi_rssi = read_wifi_rssi(&out->wifi_rssi);
    out->has_wifi_ip = read_wifi_ip(out->wifi_ip, sizeof(out->wifi_ip));
    out->has_temperature_c = read_temperature_c(&out->temperature_c);
    out->wifi_link_quality = wifi_link_quality_from_rssi(out->wifi_rssi, out->has_wifi_rssi);
    return ESP_OK;
}
