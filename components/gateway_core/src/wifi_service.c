#include "wifi_service.h"
#include "settings_manager.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdlib.h>
#include <string.h>

esp_err_t wifi_service_scan(wifi_ap_info_t **out_list, size_t *out_count)
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

void wifi_service_scan_free(wifi_ap_info_t *list)
{
    free(list);
}

esp_err_t wifi_service_save_credentials(const char *ssid, const char *password)
{
    return settings_manager_save_wifi_credentials(ssid, password);
}
