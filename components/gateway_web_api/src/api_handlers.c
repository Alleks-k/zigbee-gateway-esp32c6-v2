#include "api_handlers.h"
#include "api_contracts.h"
#include "api_usecases.h"
#include "http_error.h"

#include "esp_log.h"
#include "cJSON.h"

#include <stdio.h>
#include <stdlib.h>

static const char *TAG = "API_HANDLERS";

esp_err_t api_permit_join_handler(httpd_req_t *req)
{
    esp_err_t ret = api_usecase_permit_join(60);
    if (ret != ESP_OK) {
        return http_error_send_esp(req, ret, "Failed to open network");
    }
    ESP_LOGI(TAG, "Network opened for 60 seconds via Web API");
    return http_success_send(req, "Network opened for 60 seconds");
}

esp_err_t api_control_handler(httpd_req_t *req)
{
    api_control_request_t in = {0};
    if (api_parse_control_request(req, &in) != ESP_OK) {
        return http_error_send_esp(req, ESP_ERR_INVALID_ARG, "Missing parameters");
    }

    ESP_LOGI(TAG, "Web Control: addr=0x%04x, ep=%d, cmd=%d", in.addr, in.ep, in.cmd);
    esp_err_t err = api_usecase_control(&in);
    if (err != ESP_OK) {
        return http_error_send_esp(req, err, "Failed to send command");
    }
    return http_success_send(req, "Command sent");
}

esp_err_t api_delete_device_handler(httpd_req_t *req)
{
    api_delete_request_t in = {0};
    if (api_parse_delete_request(req, &in) != ESP_OK) {
        return http_error_send_esp(req, ESP_ERR_INVALID_ARG, "Invalid JSON");
    }

    if (api_usecase_delete_device(in.short_addr) != ESP_OK) {
        return http_error_send_esp(req, ESP_FAIL, "Delete failed");
    }
    return http_success_send(req, "Device deleted");
}

esp_err_t api_rename_device_handler(httpd_req_t *req)
{
    api_rename_request_t in = {0};
    if (api_parse_rename_request(req, &in) != ESP_OK) {
        return http_error_send_esp(req, ESP_ERR_INVALID_ARG, "Invalid JSON");
    }

    if (api_usecase_rename_device(in.short_addr, in.name) != ESP_OK) {
        return http_error_send_esp(req, ESP_FAIL, "Rename failed");
    }
    return http_success_send(req, "Device renamed");
}

esp_err_t api_wifi_scan_handler(httpd_req_t *req)
{
    wifi_ap_info_t *list = NULL;
    size_t count = 0;
    esp_err_t err = api_usecase_wifi_scan(&list, &count);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "WiFi scan failed in service: %s", esp_err_to_name(err));
        return http_error_send_esp(req, err, "Scan failed");
    }

    if (count == 0) {
        return http_success_send_data_json(req, "[]");
    }

    cJSON *root = cJSON_CreateArray();
    if (!root) {
        api_usecase_wifi_scan_free(list);
        return http_error_send_esp(req, ESP_ERR_NO_MEM, "Failed to allocate scan response");
    }
    for (size_t i = 0; i < count; i++) {
        cJSON *item = cJSON_CreateObject();
        if (!item) {
            api_usecase_wifi_scan_free(list);
            cJSON_Delete(root);
            return http_error_send_esp(req, ESP_ERR_NO_MEM, "Failed to allocate scan entry");
        }
        cJSON_AddStringToObject(item, "ssid", list[i].ssid);
        cJSON_AddNumberToObject(item, "rssi", list[i].rssi);
        cJSON_AddNumberToObject(item, "auth", list[i].auth);
        cJSON_AddItemToArray(root, item);
    }

    api_usecase_wifi_scan_free(list);
    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!json_str) {
        return http_error_send_esp(req, ESP_ERR_NO_MEM, "Failed to build scan JSON");
    }

    esp_err_t send_ret = http_success_send_data_json(req, json_str);
    free(json_str);
    if (send_ret == ESP_ERR_NO_MEM) {
        return http_error_send_esp(req, ESP_ERR_NO_MEM, "Failed to wrap scan JSON");
    }
    return send_ret;
}

esp_err_t api_reboot_handler(httpd_req_t *req)
{
    if (api_usecase_schedule_reboot(1000) != ESP_OK) {
        return http_error_send_esp(req, ESP_FAIL, "Failed to schedule reboot");
    }
    return http_success_send(req, "Rebooting...");
}

esp_err_t api_factory_reset_handler(httpd_req_t *req)
{
    esp_err_t err = api_usecase_factory_reset();
    if (err != ESP_OK) {
        return http_error_send_esp(req, err, "Factory reset failed");
    }

    api_factory_reset_report_t report = {0};
    err = api_usecase_get_factory_reset_report(&report);
    if (err != ESP_OK) {
        return http_error_send_esp(req, err, "Factory reset status unavailable");
    }

    char data_json[352];
    int written = snprintf(data_json, sizeof(data_json),
        "{\"message\":\"Factory reset done. Rebooting...\","
        "\"details\":{\"wifi\":\"%s\",\"devices\":\"%s\",\"zigbee_storage\":\"%s\",\"zigbee_fct\":\"%s\"}}}",
        esp_err_to_name(report.wifi_err),
        esp_err_to_name(report.devices_err),
        esp_err_to_name(report.zigbee_storage_err),
        esp_err_to_name(report.zigbee_fct_err));
    if (written < 0 || (size_t)written >= sizeof(data_json)) {
        return http_error_send_esp(req, ESP_ERR_NO_MEM, "Factory reset response too large");
    }
    esp_err_t send_ret = http_success_send_data_json(req, data_json);
    if (send_ret == ESP_ERR_NO_MEM) {
        return http_error_send_esp(req, ESP_ERR_NO_MEM, "Factory reset response too large");
    }
    return send_ret;
}

esp_err_t api_wifi_save_handler(httpd_req_t *req)
{
    api_wifi_save_request_t in = {0};
    if (api_parse_wifi_save_request(req, &in) != ESP_OK) {
        return http_error_send_esp(req, ESP_ERR_INVALID_ARG, "Invalid JSON");
    }

    esp_err_t err = api_usecase_wifi_save(&in);
    if (err == ESP_OK) {
        return http_success_send(req, "Saved. Restarting...");
    }
    if (err == ESP_ERR_INVALID_ARG) {
        return http_error_send_esp(req, err, "Invalid SSID or password");
    }

    ESP_LOGE(TAG, "Failed to save Wi-Fi credentials: %s", esp_err_to_name(err));
    return http_error_send_esp(req, err, "Failed to save Wi-Fi credentials");
}
