#include "job_queue_json.h"

#include "config_service.h"
#include "rcp_tool.h"
#include "system_service.h"
#include "wifi_service.h"
#include "zigbee_service.h"

#include "cJSON.h"
#include "gateway_status_esp.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

esp_err_t job_queue_json_build_scan_result_with_services(struct wifi_service *wifi_service_handle, char *out, size_t out_size)
{
    if (!wifi_service_handle) {
        return ESP_ERR_INVALID_STATE;
    }
    wifi_ap_info_t *list = NULL;
    size_t count = 0;
    esp_err_t err = wifi_service_scan(wifi_service_handle, &list, &count);
    if (err != ESP_OK) {
        return err;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON *arr = cJSON_CreateArray();
    if (!root || !arr) {
        cJSON_Delete(root);
        cJSON_Delete(arr);
        wifi_service_scan_free(wifi_service_handle, list);
        return ESP_ERR_NO_MEM;
    }
    cJSON_AddNumberToObject(root, "count", (double)count);
    cJSON_AddItemToObject(root, "networks", arr);

    for (size_t i = 0; i < count; i++) {
        cJSON *item = cJSON_CreateObject();
        if (!item) {
            cJSON_Delete(root);
            wifi_service_scan_free(wifi_service_handle, list);
            return ESP_ERR_NO_MEM;
        }
        cJSON_AddStringToObject(item, "ssid", list[i].ssid);
        cJSON_AddNumberToObject(item, "rssi", list[i].rssi);
        cJSON_AddNumberToObject(item, "auth", list[i].auth);
        cJSON_AddItemToArray(arr, item);
    }

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    wifi_service_scan_free(wifi_service_handle, list);
    if (!json) {
        return ESP_ERR_NO_MEM;
    }

    int written = snprintf(out, out_size, "%s", json);
    free(json);
    if (written < 0 || (size_t)written >= out_size) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t job_queue_json_build_factory_reset_result(char *out, size_t out_size)
{
    esp_err_t err = gateway_status_to_esp_err(config_service_factory_reset());
    if (err != ESP_OK) {
        return err;
    }

    config_service_factory_reset_report_t report = {0};
    err = gateway_status_to_esp_err(config_service_get_last_factory_reset_report(&report));
    if (err != ESP_OK) {
        return err;
    }

    int written = snprintf(
        out, out_size,
        "{\"message\":\"Factory reset completed\",\"details\":{\"wifi\":\"%s\",\"devices\":\"%s\","
        "\"zigbee_storage\":\"%s\",\"zigbee_fct\":\"%s\"}}",
        esp_err_to_name(gateway_status_to_esp_err(report.wifi_err)),
        esp_err_to_name(gateway_status_to_esp_err(report.devices_err)),
        esp_err_to_name(gateway_status_to_esp_err(report.zigbee_storage_err)),
        esp_err_to_name(gateway_status_to_esp_err(report.zigbee_fct_err)));
    if (written < 0 || (size_t)written >= out_size) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t job_queue_json_build_reboot_result_with_services(struct system_service *system_service_handle,
                                                           uint32_t delay_ms,
                                                           char *out,
                                                           size_t out_size)
{
    if (!system_service_handle) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t err = system_service_schedule_reboot(system_service_handle, delay_ms);
    if (err != ESP_OK) {
        return err;
    }
    int written = snprintf(out, out_size, "{\"message\":\"Reboot scheduled\",\"delay_ms\":%" PRIu32 "}", delay_ms);
    if (written < 0 || (size_t)written >= out_size) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t job_queue_json_build_update_result(char *out, size_t out_size)
{
#if CONFIG_OPENTHREAD_SPINEL_ONLY
    esp_err_t err = check_ot_rcp_version();
    if (err != ESP_OK) {
        return err;
    }
    int written = snprintf(out, out_size, "{\"message\":\"Update check completed\"}");
    if (written < 0 || (size_t)written >= out_size) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
#else
    (void)out;
    (void)out_size;
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

static const char *lqi_quality_label_from_value(int lqi, int rssi)
{
    (void)rssi;
    if (lqi <= 0) {
        return "unknown";
    }
    if (lqi >= 180) {
        return "good";
    }
    if (lqi >= 120) {
        return "warn";
    }
    return "bad";
}

esp_err_t job_queue_json_build_lqi_refresh_result(zigbee_service_handle_t zigbee_service_handle, char *out, size_t out_size)
{
    if (!zigbee_service_handle) {
        return ESP_ERR_INVALID_STATE;
    }

    zigbee_neighbor_lqi_t neighbors[MAX_DEVICES] = {0};
    int count = 0;
    esp_err_t err = zigbee_service_refresh_neighbor_lqi_snapshot(zigbee_service_handle, neighbors, MAX_DEVICES, &count);
    if (err != ESP_OK) {
        return err;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON *arr = cJSON_CreateArray();
    if (!root || !arr) {
        cJSON_Delete(root);
        cJSON_Delete(arr);
        return ESP_ERR_NO_MEM;
    }
    cJSON_AddNumberToObject(root, "count", count);
    cJSON_AddItemToObject(root, "neighbors", arr);

    for (int i = 0; i < count; i++) {
        cJSON *item = cJSON_CreateObject();
        if (!item) {
            cJSON_Delete(root);
            return ESP_ERR_NO_MEM;
        }
        cJSON_AddNumberToObject(item, "short_addr", neighbors[i].short_addr);
        if (neighbors[i].lqi <= 0) {
            cJSON_AddNullToObject(item, "lqi");
        } else {
            cJSON_AddNumberToObject(item, "lqi", neighbors[i].lqi);
        }

        if (neighbors[i].rssi == 127 || neighbors[i].rssi <= -127) {
            cJSON_AddNullToObject(item, "rssi");
        } else {
            cJSON_AddNumberToObject(item, "rssi", neighbors[i].rssi);
        }
        cJSON_AddStringToObject(item, "quality", lqi_quality_label_from_value(neighbors[i].lqi, neighbors[i].rssi));
        cJSON_AddItemToArray(arr, item);
    }

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!json) {
        return ESP_ERR_NO_MEM;
    }
    int written = snprintf(out, out_size, "%s", json);
    free(json);
    if (written < 0 || (size_t)written >= out_size) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
