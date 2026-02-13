#include "api_handlers.h"
#include "api_contracts.h"
#include "api_usecases.h"
#include "http_error.h"
#include "gateway_state.h"
#include "settings_manager.h"
#include "wifi_init.h"
#include "ws_manager.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
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
    int count = api_usecase_get_devices_snapshot(snapshot, MAX_DEVICES);
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
    if (api_usecase_get_network_status(&status) != ESP_OK) {
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
    if (!json_str) {
        return http_error_send_esp(req, ESP_ERR_NO_MEM, "Failed to build status payload");
    }

    size_t payload_len = strlen(json_str);
    size_t wrapped_len = payload_len + 32;
    char *wrapped = (char *)malloc(wrapped_len);
    if (!wrapped) {
        free((void *)json_str);
        return http_error_send_esp(req, ESP_ERR_NO_MEM, "Failed to wrap status payload");
    }
    int written = snprintf(wrapped, wrapped_len, "{\"status\":\"ok\",\"data\":%s}", json_str);
    free((void *)json_str);
    if (written < 0 || (size_t)written >= wrapped_len) {
        free(wrapped);
        return http_error_send_esp(req, ESP_ERR_NO_MEM, "Failed to wrap status payload");
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, wrapped);
    free(wrapped);
    return ESP_OK;
}

esp_err_t api_health_handler(httpd_req_t *req)
{
    gateway_network_state_t gw_state = {0};
    esp_err_t gw_ret = gateway_state_get_network(&gw_state);
    if (gw_ret != ESP_OK) {
        return http_error_send_esp(req, gw_ret, "Gateway state unavailable");
    }

    int32_t schema_version = 0;
    esp_err_t schema_ret = settings_manager_get_schema_version(&schema_version);

    const bool fallback_ap = wifi_is_fallback_ap_active();
    const bool sta_connected = wifi_sta_is_connected();
    const bool loaded_from_nvs = wifi_loaded_credentials_from_nvs();
    const char *active_ssid = wifi_get_active_ssid();
    if (!active_ssid) {
        active_ssid = "";
    }

    cJSON *root = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();
    cJSON *wifi = cJSON_CreateObject();
    cJSON *zigbee = cJSON_CreateObject();
    cJSON *web = cJSON_CreateObject();
    cJSON *nvs = cJSON_CreateObject();
    cJSON *heap = cJSON_CreateObject();
    if (!root || !data || !wifi || !zigbee || !web || !nvs || !heap) {
        cJSON_Delete(root);
        cJSON_Delete(data);
        cJSON_Delete(wifi);
        cJSON_Delete(zigbee);
        cJSON_Delete(web);
        cJSON_Delete(nvs);
        cJSON_Delete(heap);
        return http_error_send_esp(req, ESP_ERR_NO_MEM, "Failed to allocate health response");
    }

    cJSON_AddStringToObject(root, "status", "ok");
    cJSON_AddItemToObject(root, "data", data);

    cJSON_AddBoolToObject(wifi, "sta_connected", sta_connected);
    cJSON_AddBoolToObject(wifi, "fallback_ap_active", fallback_ap);
    cJSON_AddBoolToObject(wifi, "loaded_from_nvs", loaded_from_nvs);
    cJSON_AddStringToObject(wifi, "active_ssid", active_ssid);
    cJSON_AddItemToObject(data, "wifi", wifi);

    cJSON_AddBoolToObject(zigbee, "started", gw_state.zigbee_started);
    cJSON_AddBoolToObject(zigbee, "factory_new", gw_state.factory_new);
    cJSON_AddNumberToObject(zigbee, "pan_id", gw_state.pan_id);
    cJSON_AddNumberToObject(zigbee, "channel", gw_state.channel);
    cJSON_AddNumberToObject(zigbee, "short_addr", gw_state.short_addr);
    cJSON_AddItemToObject(data, "zigbee", zigbee);

    cJSON_AddNumberToObject(web, "ws_clients", ws_manager_get_client_count());
    cJSON_AddBoolToObject(web, "ready", true);
    cJSON_AddItemToObject(data, "web", web);

    cJSON_AddBoolToObject(nvs, "ok", schema_ret == ESP_OK);
    cJSON_AddNumberToObject(nvs, "schema_version", schema_ret == ESP_OK ? schema_version : -1);
    cJSON_AddItemToObject(data, "nvs", nvs);

    cJSON_AddNumberToObject(heap, "free", esp_get_free_heap_size());
    cJSON_AddNumberToObject(heap, "minimum_free", esp_get_minimum_free_heap_size());
    cJSON_AddItemToObject(data, "heap", heap);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!json_str) {
        return http_error_send_esp(req, ESP_ERR_NO_MEM, "Failed to build health JSON");
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);
    free(json_str);
    return ESP_OK;
}

    /* API: Відкрити мережу (Permit Join) */
esp_err_t api_permit_join_handler(httpd_req_t *req)
{
    esp_err_t ret = api_usecase_permit_join(60);
    if (ret != ESP_OK) {
        return http_error_send_esp(req, ret, "Failed to open network");
    }
    ESP_LOGI(TAG, "Network opened for 60 seconds via Web API");
    return http_success_send(req, "Network opened for 60 seconds");
}

/* API: Керування пристроєм */
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

/* API: Видалення пристрою */
esp_err_t api_delete_device_handler(httpd_req_t *req) {
    api_delete_request_t in = {0};
    if (api_parse_delete_request(req, &in) != ESP_OK) {
        return http_error_send_esp(req, ESP_ERR_INVALID_ARG, "Invalid JSON");
    }

    if (api_usecase_delete_device(in.short_addr) != ESP_OK) {
        return http_error_send_esp(req, ESP_FAIL, "Delete failed");
    }
    return http_success_send(req, "Device deleted");
}

/* API: Перейменування пристрою */
esp_err_t api_rename_device_handler(httpd_req_t *req) {
    api_rename_request_t in = {0};
    if (api_parse_rename_request(req, &in) != ESP_OK) {
        return http_error_send_esp(req, ESP_ERR_INVALID_ARG, "Invalid JSON");
    }

    if (api_usecase_rename_device(in.short_addr, in.name) != ESP_OK) {
        return http_error_send_esp(req, ESP_FAIL, "Rename failed");
    }
    return http_success_send(req, "Device renamed");
}

/* API: Сканування Wi-Fi мереж */
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
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_sendstr(req, "{\"status\":\"ok\",\"data\":[]}");
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

    size_t payload_len = strlen(json_str);
    size_t wrapped_len = payload_len + 32;
    char *wrapped = (char *)malloc(wrapped_len);
    if (!wrapped) {
        free(json_str);
        return http_error_send_esp(req, ESP_ERR_NO_MEM, "Failed to wrap scan JSON");
    }
    int written = snprintf(wrapped, wrapped_len, "{\"status\":\"ok\",\"data\":%s}", json_str);
    free(json_str);
    if (written < 0 || (size_t)written >= wrapped_len) {
        free(wrapped);
        return http_error_send_esp(req, ESP_ERR_NO_MEM, "Failed to wrap scan JSON");
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, wrapped);
    free(wrapped);

    return ESP_OK;
}

    /* API: Перезавантаження системи */
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

    system_factory_reset_report_t report = {0};
    err = api_usecase_get_factory_reset_report(&report);
    if (err != ESP_OK) {
        return http_error_send_esp(req, err, "Factory reset status unavailable");
    }

    char resp[384];
    int written = snprintf(resp, sizeof(resp),
        "{\"status\":\"ok\",\"data\":{\"message\":\"Factory reset done. Rebooting...\","
        "\"details\":{\"wifi\":\"%s\",\"devices\":\"%s\",\"zigbee_storage\":\"%s\",\"zigbee_fct\":\"%s\"}}}",
        esp_err_to_name(report.wifi_err),
        esp_err_to_name(report.devices_err),
        esp_err_to_name(report.zigbee_storage_err),
        esp_err_to_name(report.zigbee_fct_err));
    if (written < 0 || (size_t)written >= sizeof(resp)) {
        return http_error_send_esp(req, ESP_ERR_NO_MEM, "Factory reset response too large");
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, resp);
    return ESP_OK;
}

/* Збереження налаштувань Wi-Fi */
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
