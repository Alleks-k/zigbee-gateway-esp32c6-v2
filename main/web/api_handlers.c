#include "api_handlers.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_zigbee_gateway.h"
#include "device_manager.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"

static const char *TAG = "API_HANDLERS";

/* Helper: Створення JSON зі статусом */
char* create_status_json(void)
{
    cJSON *root = cJSON_CreateObject();
    
    cJSON_AddNumberToObject(root, "pan_id", esp_zb_get_pan_id());
    cJSON_AddNumberToObject(root, "channel", esp_zb_get_current_channel());
    cJSON_AddNumberToObject(root, "short_addr", esp_zb_get_short_address());

    /* Отримуємо список пристроїв з менеджера */
    cJSON *dev_list = device_manager_get_json_list();

    cJSON_AddItemToObject(root, "devices", dev_list);
    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json_str;
}

/* API: Статус у JSON */
esp_err_t api_status_handler(httpd_req_t *req)
{
    char *json_str = create_status_json();
    if (!json_str) return ESP_FAIL;

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);

    free((void*)json_str);
    return ESP_OK;
}

/* API: Відкрити мережу (Permit Join) */
esp_err_t api_permit_join_handler(httpd_req_t *req)
{
    esp_zb_bdb_open_network(60);
    ESP_LOGI(TAG, "Network opened for 60 seconds via Web API");
    const char* resp = "{\"message\":\"Network opened for 60 seconds\"}";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

/* API: Керування пристроєм */
esp_err_t api_control_handler(httpd_req_t *req)
{
    char buf[128];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) return ESP_FAIL;
    buf[len] = '\0';
    
    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    cJSON *addr_item = cJSON_GetObjectItem(root, "addr");
    cJSON *ep_item = cJSON_GetObjectItem(root, "ep");
    cJSON *cmd_item = cJSON_GetObjectItem(root, "cmd");

    if (cJSON_IsNumber(addr_item) && cJSON_IsNumber(ep_item) && cJSON_IsNumber(cmd_item)) {
        uint16_t addr = (uint16_t)addr_item->valueint;
        uint8_t endpoint = (uint8_t)ep_item->valueint;
        uint8_t cmd = (uint8_t)cmd_item->valueint;

        ESP_LOGI(TAG, "Web Control: addr=0x%04x, ep=%d, cmd=%d", addr, endpoint, cmd);
        send_on_off_command(addr, endpoint, cmd);
        const char* resp = "{\"status\":\"ok\", \"message\":\"Command sent\"}";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, resp);
    } else {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing parameters");
    }
    cJSON_Delete(root);
    return ESP_OK;
}

/* API: Видалення пристрою */
esp_err_t api_delete_device_handler(httpd_req_t *req) {
    char buf[128];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) return ESP_FAIL;
    buf[len] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (root) {
        cJSON *addr_item = cJSON_GetObjectItem(root, "short_addr");
        if (cJSON_IsNumber(addr_item)) {
            delete_device((uint16_t)addr_item->valueint);
            const char* resp = "{\"status\":\"ok\"}";
            httpd_resp_set_type(req, "application/json");
            httpd_resp_sendstr(req, resp);
        } else {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid address");
        }
        cJSON_Delete(root);
    } else {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
    }
    return ESP_OK;
}

/* API: Перейменування пристрою */
esp_err_t api_rename_device_handler(httpd_req_t *req) {
    char buf[128];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) return ESP_FAIL;
    buf[len] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (root) {
        cJSON *addr_item = cJSON_GetObjectItem(root, "short_addr");
        cJSON *name_item = cJSON_GetObjectItem(root, "name");
        if (cJSON_IsNumber(addr_item) && cJSON_IsString(name_item)) {
            update_device_name((uint16_t)addr_item->valueint, name_item->valuestring);
            const char* resp = "{\"status\":\"ok\"}";
            httpd_resp_set_type(req, "application/json");
            httpd_resp_sendstr(req, resp);
        } else {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid params");
        }
        cJSON_Delete(root);
    } else {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
    }
    return ESP_OK;
}

/* API: Сканування Wi-Fi мереж */
esp_err_t api_wifi_scan_handler(httpd_req_t *req)
{
    wifi_mode_t mode;
    esp_err_t mode_ret = esp_wifi_get_mode(&mode);
    if (mode_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get WiFi mode: %s", esp_err_to_name(mode_ret));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "WiFi mode unavailable");
        return ESP_FAIL;
    }

    /* Scan requires STA capability; in fallback AP-only mode switch to APSTA. */
    if (mode == WIFI_MODE_AP) {
        esp_err_t ret = esp_wifi_set_mode(WIFI_MODE_APSTA);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to switch AP->APSTA for scan: %s", esp_err_to_name(ret));
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Cannot enable scan mode");
            return ESP_FAIL;
        }
        ESP_LOGI(TAG, "WiFi mode switched to APSTA for network scan");
        /* Give driver time to settle after mode switch before first scan. */
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    wifi_scan_config_t scan_config = {
        .ssid = 0,
        .bssid = 0,
        .channel = 0,
        .show_hidden = true
    };

    /* Запускаємо сканування (блокуюче, чекаємо завершення) */
    esp_err_t err = ESP_FAIL;
    for (int attempt = 0; attempt < 3; attempt++) {
        err = esp_wifi_scan_start(&scan_config, true);
        if (err == ESP_OK) {
            break;
        }
        if (err == ESP_ERR_WIFI_STATE) {
            ESP_LOGW(TAG, "WiFi scan attempt %d failed: STA busy, retrying", attempt + 1);
            vTaskDelay(pdMS_TO_TICKS(250));
            continue;
        }
        break;
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "WiFi scan failed: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Scan failed");
        return ESP_FAIL;
    }

    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    if (ap_count == 0) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "[]");
        return ESP_OK;
    }

    wifi_ap_record_t *ap_info = (wifi_ap_record_t *)malloc(sizeof(wifi_ap_record_t) * ap_count);
    if (!ap_info) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
        return ESP_FAIL;
    }

    err = esp_wifi_scan_get_ap_records(&ap_count, ap_info);
    if (err != ESP_OK) {
        free(ap_info);
        ESP_LOGE(TAG, "Failed to get scan records: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Scan records failed");
        return ESP_FAIL;
    }

    cJSON *root = cJSON_CreateArray();
    for (int i = 0; i < ap_count; i++) {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "ssid", (char *)ap_info[i].ssid);
        cJSON_AddNumberToObject(item, "rssi", ap_info[i].rssi);
        cJSON_AddNumberToObject(item, "auth", ap_info[i].authmode);
        cJSON_AddItemToArray(root, item);
    }

    free(ap_info);
    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);
    free(json_str);

    return ESP_OK;
}

/* API: Перезавантаження системи */
esp_err_t api_reboot_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\", \"message\":\"Rebooting...\"}");
    vTaskDelay(pdMS_TO_TICKS(1000)); // Даємо час на відправку відповіді
    esp_restart();
    return ESP_OK;
}

/* Збереження налаштувань Wi-Fi */
esp_err_t api_wifi_save_handler(httpd_req_t *req)
{
    char buf[256];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) return ESP_FAIL;
    buf[len] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    cJSON *ssid = cJSON_GetObjectItem(root, "ssid");
    cJSON *pass = cJSON_GetObjectItem(root, "password");

    if (cJSON_IsString(ssid) && cJSON_IsString(pass)) {
        size_t ssid_len = strlen(ssid->valuestring);
        size_t pass_len = strlen(pass->valuestring);

        if (ssid_len == 0 || ssid_len > 32) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid SSID length (1-32)");
            cJSON_Delete(root);
            return ESP_OK;
        }

        if (pass_len < 8 || pass_len > 64) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid Password length (8-64)");
            cJSON_Delete(root);
            return ESP_OK;
        }

        nvs_handle_t my_handle;
        esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
        if (err == ESP_OK) {
            nvs_set_str(my_handle, "wifi_ssid", ssid->valuestring);
            nvs_set_str(my_handle, "wifi_pass", pass->valuestring);
            nvs_commit(my_handle);
            nvs_close(my_handle);
            
            const char* resp = "{\"status\":\"ok\", \"message\":\"Saved. Restarting...\"}";
            httpd_resp_set_type(req, "application/json");
            httpd_resp_sendstr(req, resp);
            
            /* Даємо час на відправку відповіді перед перезавантаженням */
            vTaskDelay(pdMS_TO_TICKS(1000));
            esp_restart();
        } else {
            httpd_resp_send_500(req);
        }
    } else {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing ssid or password");
    }
    cJSON_Delete(root);
    return ESP_OK;
}
