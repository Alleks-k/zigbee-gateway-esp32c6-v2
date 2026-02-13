#include "api_handlers.h"
#include "esp_log.h"
#include "zigbee_service.h"
#include "wifi_service.h"
#include "system_service.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>

static const char *TAG = "API_HANDLERS";

static bool append_literal(char **cursor, size_t *remaining, const char *text)
{
    int written = snprintf(*cursor, *remaining, "%s", text);
    if (written < 0 || (size_t)written >= *remaining) {
        return false;
    }
    *cursor += written;
    *remaining -= (size_t)written;
    return true;
}

static bool append_u32(char **cursor, size_t *remaining, uint32_t value)
{
    int written = snprintf(*cursor, *remaining, "%" PRIu32, value);
    if (written < 0 || (size_t)written >= *remaining) {
        return false;
    }
    *cursor += written;
    *remaining -= (size_t)written;
    return true;
}

static bool append_json_escaped(char **cursor, size_t *remaining, const char *src)
{
    if (!src) {
        return append_literal(cursor, remaining, "");
    }

    while (*src) {
        unsigned char ch = (unsigned char)*src++;
        if (ch == '"' || ch == '\\') {
            if (*remaining < 3) {
                return false;
            }
            *(*cursor)++ = '\\';
            *(*cursor)++ = (char)ch;
            *remaining -= 2;
            continue;
        }
        if (ch < 0x20) {
            if (*remaining < 7) {
                return false;
            }
            int written = snprintf(*cursor, *remaining, "\\u%04x", (unsigned)ch);
            if (written != 6) {
                return false;
            }
            *cursor += written;
            *remaining -= (size_t)written;
            continue;
        }
        if (*remaining < 2) {
            return false;
        }
        *(*cursor)++ = (char)ch;
        (*remaining)--;
    }
    **cursor = '\0';
    return true;
}

static esp_err_t append_devices_array(char **cursor, size_t *remaining)
{
    zb_device_t snapshot[MAX_DEVICES];
    int count = zigbee_service_get_devices_snapshot(snapshot, MAX_DEVICES);
    for (int i = 0; i < count; i++) {
        if (i > 0 && !append_literal(cursor, remaining, ",")) {
            return ESP_ERR_NO_MEM;
        }
        if (!append_literal(cursor, remaining, "{\"name\":\"") ||
            !append_json_escaped(cursor, remaining, snapshot[i].name) ||
            !append_literal(cursor, remaining, "\",\"short_addr\":") ||
            !append_u32(cursor, remaining, snapshot[i].short_addr) ||
            !append_literal(cursor, remaining, "}"))
        {
            return ESP_ERR_NO_MEM;
        }
    }
    return ESP_OK;
}

esp_err_t build_status_json_compact(char *out, size_t out_size, size_t *out_len)
{
    if (!out || out_size < 2) {
        return ESP_ERR_INVALID_ARG;
    }

    char *cursor = out;
    size_t remaining = out_size;

    zigbee_network_status_t status = {0};
    if (zigbee_service_get_network_status(&status) != ESP_OK) {
        return ESP_FAIL;
    }

    if (!append_literal(&cursor, &remaining, "{\"pan_id\":") ||
        !append_u32(&cursor, &remaining, status.pan_id) ||
        !append_literal(&cursor, &remaining, ",\"channel\":") ||
        !append_u32(&cursor, &remaining, status.channel) ||
        !append_literal(&cursor, &remaining, ",\"short_addr\":") ||
        !append_u32(&cursor, &remaining, status.short_addr) ||
        !append_literal(&cursor, &remaining, ",\"devices\":["))
    {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t dev_ret = append_devices_array(&cursor, &remaining);
    if (dev_ret != ESP_OK) {
        return dev_ret;
    }

    if (!append_literal(&cursor, &remaining, "]}")) {
        return ESP_ERR_NO_MEM;
    }

    if (out_len) {
        *out_len = (size_t)(cursor - out);
    }
    return ESP_OK;
}

esp_err_t build_devices_json_compact(char *out, size_t out_size, size_t *out_len)
{
    if (!out || out_size < 2) {
        return ESP_ERR_INVALID_ARG;
    }

    char *cursor = out;
    size_t remaining = out_size;

    if (!append_literal(&cursor, &remaining, "{\"devices\":[")) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t dev_ret = append_devices_array(&cursor, &remaining);
    if (dev_ret != ESP_OK) {
        return dev_ret;
    }

    if (!append_literal(&cursor, &remaining, "]}")) {
        return ESP_ERR_NO_MEM;
    }

    if (out_len) {
        *out_len = (size_t)(cursor - out);
    }
    return ESP_OK;
}

/* Helper: Створення JSON зі статусом */
char* create_status_json(void)
{
    size_t cap = 1024;
    for (int i = 0; i < 4; i++) {
        char *buf = (char *)malloc(cap);
        if (!buf) {
            return NULL;
        }
        size_t out_len = 0;
        esp_err_t ret = build_status_json_compact(buf, cap, &out_len);
        if (ret == ESP_OK) {
            (void)out_len;
            return buf;
        }
        free(buf);
        if (ret != ESP_ERR_NO_MEM) {
            return NULL;
        }
        cap *= 2;
    }
    return NULL;
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
    esp_err_t ret = zigbee_service_permit_join(60);
    if (ret != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to open network");
        return ESP_FAIL;
    }
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
        if (zigbee_service_send_on_off(addr, endpoint, cmd) != ESP_OK) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send command");
            cJSON_Delete(root);
            return ESP_FAIL;
        }
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
            if (zigbee_service_delete_device((uint16_t)addr_item->valueint) != ESP_OK) {
                cJSON_Delete(root);
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Delete failed");
                return ESP_FAIL;
            }
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
            if (zigbee_service_rename_device((uint16_t)addr_item->valueint, name_item->valuestring) != ESP_OK) {
                cJSON_Delete(root);
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Rename failed");
                return ESP_FAIL;
            }
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
    wifi_ap_info_t *list = NULL;
    size_t count = 0;
    esp_err_t err = wifi_service_scan(&list, &count);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "WiFi scan failed in service: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Scan failed");
        return ESP_FAIL;
    }

    if (count == 0) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "[]");
        return ESP_OK;
    }

    cJSON *root = cJSON_CreateArray();
    for (size_t i = 0; i < count; i++) {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "ssid", list[i].ssid);
        cJSON_AddNumberToObject(item, "rssi", list[i].rssi);
        cJSON_AddNumberToObject(item, "auth", list[i].auth);
        cJSON_AddItemToArray(root, item);
    }

    wifi_service_scan_free(list);
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
    if (system_service_schedule_reboot(1000) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to schedule reboot");
        return ESP_FAIL;
    }
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\", \"message\":\"Rebooting...\"}");
    return ESP_OK;
}

esp_err_t api_factory_reset_handler(httpd_req_t *req)
{
    esp_err_t err = system_service_factory_reset_and_reboot(1000);
    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Factory reset failed");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\", \"message\":\"Factory reset done. Rebooting...\"}");
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
        esp_err_t err = wifi_service_save_credentials(ssid->valuestring, pass->valuestring);
        if (err == ESP_OK) {
            if (system_service_schedule_reboot(1000) != ESP_OK) {
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to schedule reboot");
                cJSON_Delete(root);
                return ESP_FAIL;
            }
            const char* resp = "{\"status\":\"ok\", \"message\":\"Saved. Restarting...\"}";
            httpd_resp_set_type(req, "application/json");
            httpd_resp_sendstr(req, resp);
        } else if (err == ESP_ERR_INVALID_ARG) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid SSID or password");
        } else {
            ESP_LOGE(TAG, "Failed to save Wi-Fi credentials: %s", esp_err_to_name(err));
            httpd_resp_send_500(req);
        }
    } else {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing ssid or password");
    }
    cJSON_Delete(root);
    return ESP_OK;
}
