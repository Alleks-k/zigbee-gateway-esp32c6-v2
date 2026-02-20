#include "net_platform_services.h"
#include "wifi_service.h"
#include "system_service.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "lwip/ip4_addr.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#if __has_include("driver/temperature_sensor.h")
#include "driver/temperature_sensor.h"
#define GATEWAY_HAS_TEMP_SENSOR 1
#else
#define GATEWAY_HAS_TEMP_SENSOR 0
#endif

#if GATEWAY_HAS_TEMP_SENSOR
static temperature_sensor_handle_t s_temp_sensor_handle = NULL;
static bool s_temp_sensor_init_attempted = false;
static bool s_temp_sensor_available = false;
#endif

static esp_err_t net_wifi_scan_impl(wifi_ap_info_t **out_list, size_t *out_count)
{
    wifi_mode_t mode;
    esp_err_t ret = esp_wifi_get_mode(&mode);
    if (ret != ESP_OK) {
        return ret;
    }

    if (mode == WIFI_MODE_AP) {
        ret = esp_wifi_set_mode(WIFI_MODE_APSTA);
        if (ret != ESP_OK) {
            return ret;
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    wifi_scan_config_t scan_config = {
        .ssid = 0,
        .bssid = 0,
        .channel = 0,
        .show_hidden = true
    };

    ret = ESP_FAIL;
    for (int attempt = 0; attempt < 3; attempt++) {
        ret = esp_wifi_scan_start(&scan_config, true);
        if (ret == ESP_OK) {
            break;
        }
        if (ret == ESP_ERR_WIFI_STATE) {
            vTaskDelay(pdMS_TO_TICKS(250));
            continue;
        }
        break;
    }
    if (ret != ESP_OK) {
        return ret;
    }

    uint16_t ap_count = 0;
    ret = esp_wifi_scan_get_ap_num(&ap_count);
    if (ret != ESP_OK || ap_count == 0) {
        return ret;
    }

    wifi_ap_record_t *ap_records = (wifi_ap_record_t *)malloc(sizeof(wifi_ap_record_t) * ap_count);
    if (!ap_records) {
        return ESP_ERR_NO_MEM;
    }

    ret = esp_wifi_scan_get_ap_records(&ap_count, ap_records);
    if (ret != ESP_OK) {
        free(ap_records);
        return ret;
    }

    wifi_ap_info_t *list = (wifi_ap_info_t *)calloc(ap_count, sizeof(wifi_ap_info_t));
    if (!list) {
        free(ap_records);
        return ESP_ERR_NO_MEM;
    }

    for (uint16_t i = 0; i < ap_count; i++) {
        strlcpy(list[i].ssid, (const char *)ap_records[i].ssid, sizeof(list[i].ssid));
        list[i].rssi = ap_records[i].rssi;
        list[i].auth = (uint8_t)ap_records[i].authmode;
    }

    free(ap_records);
    *out_list = list;
    *out_count = ap_count;
    return ESP_OK;
}

static void net_wifi_scan_free_impl(wifi_ap_info_t *list)
{
    free(list);
}

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

static esp_err_t net_collect_telemetry_impl(system_telemetry_t *out)
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

void net_platform_services_init(wifi_service_handle_t wifi_service, system_service_handle_t system_service)
{
    if (!wifi_service || !system_service) {
        return;
    }
    wifi_service_register_scan_impl(wifi_service, net_wifi_scan_impl, net_wifi_scan_free_impl);
    system_service_register_telemetry_impl(system_service, net_collect_telemetry_impl);
}
