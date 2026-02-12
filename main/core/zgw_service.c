#include "zgw_service.h"
#include "esp_zigbee_gateway.h"
#include "settings_manager.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdlib.h>

static const char *TAG = "ZGW_SERVICE";

static void reboot_task(void *arg)
{
    uint32_t delay_ms = (uint32_t)(uintptr_t)arg;
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
    esp_restart();
}

esp_err_t zgw_service_get_network_status(zgw_network_status_t *out)
{
    if (!out) {
        return ESP_ERR_INVALID_ARG;
    }
    out->pan_id = esp_zb_get_pan_id();
    out->channel = esp_zb_get_current_channel();
    out->short_addr = esp_zb_get_short_address();
    return ESP_OK;
}

esp_err_t zgw_service_permit_join(uint16_t seconds)
{
    esp_zb_bdb_open_network(seconds);
    return ESP_OK;
}

esp_err_t zgw_service_send_on_off(uint16_t short_addr, uint8_t endpoint, uint8_t on_off)
{
    send_on_off_command(short_addr, endpoint, on_off);
    return ESP_OK;
}

int zgw_service_get_devices_snapshot(zb_device_t *out, size_t max_items)
{
    return device_manager_get_snapshot(out, max_items);
}

esp_err_t zgw_service_delete_device(uint16_t short_addr)
{
    delete_device(short_addr);
    return ESP_OK;
}

esp_err_t zgw_service_rename_device(uint16_t short_addr, const char *name)
{
    if (!name) {
        return ESP_ERR_INVALID_ARG;
    }
    update_device_name(short_addr, name);
    return ESP_OK;
}

esp_err_t zgw_service_wifi_scan(zgw_wifi_ap_info_t **out_list, size_t *out_count)
{
    if (!out_list || !out_count) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_list = NULL;
    *out_count = 0;

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

    zgw_wifi_ap_info_t *list = (zgw_wifi_ap_info_t *)calloc(ap_count, sizeof(zgw_wifi_ap_info_t));
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

void zgw_service_wifi_scan_free(zgw_wifi_ap_info_t *list)
{
    free(list);
}

esp_err_t zgw_service_wifi_save_credentials(const char *ssid, const char *password)
{
    return settings_manager_save_wifi_credentials(ssid, password);
}

void zgw_service_reboot(void)
{
    ESP_LOGI(TAG, "Reboot requested by service");
    esp_restart();
}

esp_err_t zgw_service_schedule_reboot(uint32_t delay_ms)
{
    BaseType_t ok = xTaskCreate(reboot_task, "zgw_reboot", 2048, (void *)(uintptr_t)delay_ms, 5, NULL);
    if (ok != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t zgw_service_factory_reset_and_reboot(uint32_t reboot_delay_ms)
{
    esp_err_t err = settings_manager_factory_reset();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Factory reset failed: %s", esp_err_to_name(err));
        return err;
    }
    return zgw_service_schedule_reboot(reboot_delay_ms);
}
