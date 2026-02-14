#include "api_handlers.h"
#include "api_contracts.h"
#include "api_usecases.h"
#include "http_error.h"
#include "error_ring.h"
#include "lqi_json_mapper.h"
#include "job_queue.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>

static const char *TAG = "API_HANDLERS";
// Guardrails for /api*/jobs/{id}: bound result payload by job type.
#define JOB_API_RESULT_JSON_LIMIT_SCAN          768
#define JOB_API_RESULT_JSON_LIMIT_FACTORY_RESET 1536
#define JOB_API_RESULT_JSON_LIMIT_REBOOT        512
#define JOB_API_RESULT_JSON_LIMIT_UPDATE        768
#define JOB_API_RESULT_JSON_LIMIT_LQI_REFRESH   1024

static const char *wifi_link_quality_label(system_wifi_link_quality_t quality)
{
    switch (quality) {
    case SYSTEM_WIFI_LINK_GOOD:
        return "good";
    case SYSTEM_WIFI_LINK_WARN:
        return "warn";
    case SYSTEM_WIFI_LINK_BAD:
        return "bad";
    case SYSTEM_WIFI_LINK_UNKNOWN:
    default:
        return "unknown";
    }
}

static size_t job_result_json_limit_for_type(zgw_job_type_t type)
{
    switch (type) {
    case ZGW_JOB_TYPE_WIFI_SCAN:
        return JOB_API_RESULT_JSON_LIMIT_SCAN;
    case ZGW_JOB_TYPE_FACTORY_RESET:
        return JOB_API_RESULT_JSON_LIMIT_FACTORY_RESET;
    case ZGW_JOB_TYPE_REBOOT:
        return JOB_API_RESULT_JSON_LIMIT_REBOOT;
    case ZGW_JOB_TYPE_LQI_REFRESH:
        return JOB_API_RESULT_JSON_LIMIT_LQI_REFRESH;
    case ZGW_JOB_TYPE_UPDATE:
    default:
        return JOB_API_RESULT_JSON_LIMIT_UPDATE;
    }
}

static zgw_job_type_t parse_job_type(const char *type)
{
    if (!type) {
        return ZGW_JOB_TYPE_WIFI_SCAN;
    }
    if (strcmp(type, "scan") == 0) {
        return ZGW_JOB_TYPE_WIFI_SCAN;
    }
    if (strcmp(type, "factory_reset") == 0) {
        return ZGW_JOB_TYPE_FACTORY_RESET;
    }
    if (strcmp(type, "reboot") == 0) {
        return ZGW_JOB_TYPE_REBOOT;
    }
    if (strcmp(type, "update") == 0) {
        return ZGW_JOB_TYPE_UPDATE;
    }
    if (strcmp(type, "lqi_refresh") == 0) {
        return ZGW_JOB_TYPE_LQI_REFRESH;
    }
    return ZGW_JOB_TYPE_WIFI_SCAN;
}

static esp_err_t parse_job_id_from_uri(const char *uri, uint32_t *out_id)
{
    if (!uri || !out_id) {
        return ESP_ERR_INVALID_ARG;
    }
    const char *last_slash = strrchr(uri, '/');
    if (!last_slash || *(last_slash + 1) == '\0') {
        return ESP_ERR_INVALID_ARG;
    }
    char *endptr = NULL;
    unsigned long value = strtoul(last_slash + 1, &endptr, 10);
    if (endptr == NULL || *endptr != '\0' || value == 0 || value > UINT32_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_id = (uint32_t)value;
    return ESP_OK;
}

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

static bool append_u64(char **cursor, size_t *remaining, uint64_t value)
{
    int written = snprintf(*cursor, *remaining, "%" PRIu64, value);
    if (written < 0 || (size_t)written >= *remaining) {
        return false;
    }
    *cursor += written;
    *remaining -= (size_t)written;
    return true;
}

static bool append_i32(char **cursor, size_t *remaining, int32_t value)
{
    int written = snprintf(*cursor, *remaining, "%" PRId32, value);
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

static esp_err_t append_error_ring_array(char **cursor, size_t *remaining)
{
    const size_t max_emit = 5;
    gateway_error_entry_t entries[5];
    size_t count = gateway_error_ring_snapshot(entries, max_emit);
    if (count > max_emit) {
        count = max_emit;
    }

    if (!append_literal(cursor, remaining, "[")) {
        return ESP_ERR_NO_MEM;
    }
    for (size_t i = 0; i < count; i++) {
        if (i > 0 && !append_literal(cursor, remaining, ",")) {
            return ESP_ERR_NO_MEM;
        }
        if (!append_literal(cursor, remaining, "{\"ts_ms\":") ||
            !append_u64(cursor, remaining, entries[i].ts_ms) ||
            !append_literal(cursor, remaining, ",\"source\":\"") ||
            !append_json_escaped(cursor, remaining, entries[i].source) ||
            !append_literal(cursor, remaining, "\",\"code\":") ||
            !append_i32(cursor, remaining, entries[i].code) ||
            !append_literal(cursor, remaining, ",\"message\":\"") ||
            !append_json_escaped(cursor, remaining, entries[i].message) ||
            !append_literal(cursor, remaining, "\"}"))
        {
            return ESP_ERR_NO_MEM;
        }
    }
    if (!append_literal(cursor, remaining, "]")) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
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
        !append_literal(&cursor, &remaining, ",\"zigbee\":{") ||
        !append_literal(&cursor, &remaining, "\"pan_id\":") ||
        !append_u32(&cursor, &remaining, status.pan_id) ||
        !append_literal(&cursor, &remaining, ",\"channel\":") ||
        !append_u32(&cursor, &remaining, status.channel) ||
        !append_literal(&cursor, &remaining, ",\"short_addr\":") ||
        !append_u32(&cursor, &remaining, status.short_addr) ||
        !append_literal(&cursor, &remaining, "}") ||
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
    esp_err_t ret = http_success_send_data_json(req, json_str);
    free((void *)json_str);
    if (ret == ESP_ERR_NO_MEM) {
        return http_error_send_esp(req, ret, "Failed to wrap status payload");
    }
    return ret;
}

esp_err_t api_lqi_handler(httpd_req_t *req)
{
    size_t needed = 1024;
    for (int i = 0; i < 4; i++) {
        char *json = (char *)malloc(needed);
        if (!json) {
            return http_error_send_esp(req, ESP_ERR_NO_MEM, "Out of memory");
        }

        size_t out_len = 0;
        esp_err_t ret = build_lqi_json_compact(json, needed, &out_len);
        if (ret == ESP_OK) {
            esp_err_t send_ret = http_success_send_data_json(req, json);
            free(json);
            if (send_ret == ESP_ERR_NO_MEM) {
                return http_error_send_esp(req, ESP_ERR_NO_MEM, "LQI payload too large");
            }
            return send_ret;
        }

        free(json);
        if (ret != ESP_ERR_NO_MEM) {
            return http_error_send_esp(req, ret, "Failed to build LQI payload");
        }
        needed *= 2;
    }

    return http_error_send_esp(req, ESP_ERR_NO_MEM, "LQI payload too large");
}

esp_err_t build_health_json_compact(char *out, size_t out_size, size_t *out_len)
{
    if (!out || out_size < 2) {
        return ESP_ERR_INVALID_ARG;
    }

    api_health_snapshot_t hs = {0};
    esp_err_t hs_ret = api_usecase_collect_health_snapshot(&hs);
    if (hs_ret != ESP_OK) {
        return hs_ret;
    }
    const uint64_t telemetry_updated_ms = hs.telemetry.uptime_ms;

    char *cursor = out;
    size_t remaining = out_size;

    if (!append_literal(&cursor, &remaining, "{\"wifi\":{")) {
        return ESP_ERR_NO_MEM;
    }
    if (!append_literal(&cursor, &remaining, "\"sta_connected\":") ||
        !append_literal(&cursor, &remaining, hs.wifi_sta_connected ? "true" : "false") ||
        !append_literal(&cursor, &remaining, ",\"fallback_ap_active\":") ||
        !append_literal(&cursor, &remaining, hs.wifi_fallback_ap_active ? "true" : "false") ||
        !append_literal(&cursor, &remaining, ",\"loaded_from_nvs\":") ||
        !append_literal(&cursor, &remaining, hs.wifi_loaded_from_nvs ? "true" : "false") ||
        !append_literal(&cursor, &remaining, ",\"ssid\":\"") ||
        !append_json_escaped(&cursor, &remaining, hs.wifi_active_ssid) ||
        !append_literal(&cursor, &remaining, "\",\"active_ssid\":\"") ||
        !append_json_escaped(&cursor, &remaining, hs.wifi_active_ssid) ||
        !append_literal(&cursor, &remaining, "\",\"rssi\":"))
    {
        return ESP_ERR_NO_MEM;
    }
    if (hs.telemetry.has_wifi_rssi) {
        if (!append_i32(&cursor, &remaining, hs.telemetry.wifi_rssi)) {
            return ESP_ERR_NO_MEM;
        }
    } else if (!append_literal(&cursor, &remaining, "null")) {
        return ESP_ERR_NO_MEM;
    }
    if (!append_literal(&cursor, &remaining, ",\"link_quality\":\"") ||
        !append_literal(&cursor, &remaining, wifi_link_quality_label(hs.telemetry.wifi_link_quality)) ||
        !append_literal(&cursor, &remaining, "\",\"ip\":"))
    {
        return ESP_ERR_NO_MEM;
    }
    if (hs.telemetry.has_wifi_ip) {
        if (!append_literal(&cursor, &remaining, "\"") ||
            !append_json_escaped(&cursor, &remaining, hs.telemetry.wifi_ip) ||
            !append_literal(&cursor, &remaining, "\""))
        {
            return ESP_ERR_NO_MEM;
        }
    } else if (!append_literal(&cursor, &remaining, "null")) {
        return ESP_ERR_NO_MEM;
    }

    if (!append_literal(&cursor, &remaining, "},\"zigbee\":{") ||
        !append_literal(&cursor, &remaining, "\"started\":") ||
        !append_literal(&cursor, &remaining, hs.zigbee_started ? "true" : "false") ||
        !append_literal(&cursor, &remaining, ",\"factory_new\":") ||
        !append_literal(&cursor, &remaining, hs.zigbee_factory_new ? "true" : "false") ||
        !append_literal(&cursor, &remaining, ",\"pan_id\":") ||
        !append_u32(&cursor, &remaining, hs.zigbee_pan_id) ||
        !append_literal(&cursor, &remaining, ",\"channel\":") ||
        !append_u32(&cursor, &remaining, hs.zigbee_channel) ||
        !append_literal(&cursor, &remaining, ",\"short_addr\":") ||
        !append_u32(&cursor, &remaining, hs.zigbee_short_addr) ||
        !append_literal(&cursor, &remaining, "},\"web\":{") ||
        !append_literal(&cursor, &remaining, "\"ws_clients\":") ||
        !append_u32(&cursor, &remaining, hs.ws_clients) ||
        !append_literal(&cursor, &remaining, ",\"ready\":true},\"nvs\":{") ||
        !append_literal(&cursor, &remaining, "\"ok\":") ||
        !append_literal(&cursor, &remaining, hs.nvs_ok ? "true" : "false") ||
        !append_literal(&cursor, &remaining, ",\"schema_version\":") ||
        !append_i32(&cursor, &remaining, hs.nvs_schema_version) ||
        !append_literal(&cursor, &remaining, "},\"system\":{") ||
        !append_literal(&cursor, &remaining, "\"uptime_ms\":") ||
        !append_u64(&cursor, &remaining, hs.telemetry.uptime_ms) ||
        !append_literal(&cursor, &remaining, ",\"heap_free\":") ||
        !append_u32(&cursor, &remaining, hs.telemetry.heap_free) ||
        !append_literal(&cursor, &remaining, ",\"heap_min\":") ||
        !append_u32(&cursor, &remaining, hs.telemetry.heap_min) ||
        !append_literal(&cursor, &remaining, ",\"heap_largest_block\":") ||
        !append_u32(&cursor, &remaining, hs.telemetry.heap_largest_block) ||
        !append_literal(&cursor, &remaining, ",\"main_stack_hwm_bytes\":") ||
        !append_i32(&cursor, &remaining, hs.telemetry.main_stack_hwm_bytes) ||
        !append_literal(&cursor, &remaining, ",\"httpd_stack_hwm_bytes\":") ||
        !append_i32(&cursor, &remaining, hs.telemetry.httpd_stack_hwm_bytes) ||
        !append_literal(&cursor, &remaining, ",\"temperature_c\":"))
    {
        return ESP_ERR_NO_MEM;
    }
    if (hs.telemetry.has_temperature_c) {
        int written = snprintf(cursor, remaining, "%.2f", (double)hs.telemetry.temperature_c);
        if (written < 0 || (size_t)written >= remaining) {
            return ESP_ERR_NO_MEM;
        }
        cursor += written;
        remaining -= (size_t)written;
    } else if (!append_literal(&cursor, &remaining, "null")) {
        return ESP_ERR_NO_MEM;
    }

    if (!append_literal(&cursor, &remaining, "},\"telemetry\":{") ||
        !append_literal(&cursor, &remaining, "\"updated_ms\":") ||
        !append_u64(&cursor, &remaining, telemetry_updated_ms) ||
        !append_literal(&cursor, &remaining, "},\"errors\":"))
    {
        return ESP_ERR_NO_MEM;
    }
    esp_err_t ring_ret = append_error_ring_array(&cursor, &remaining);
    if (ring_ret != ESP_OK) {
        return ring_ret;
    }
    if (!append_literal(&cursor, &remaining, "}")) {
        return ESP_ERR_NO_MEM;
    }

    if (out_len) {
        *out_len = (size_t)(cursor - out);
    }
    return ESP_OK;
}

esp_err_t api_health_handler(httpd_req_t *req)
{
    size_t needed = 768;
    for (int i = 0; i < 4; i++) {
        char *health_json = (char *)malloc(needed);
        if (!health_json) {
            // Avoid ring amplification loop for /health failures.
            return http_error_send(req, 503, "no_memory", "Out of memory");
        }

        size_t health_len = 0;
        esp_err_t ret = build_health_json_compact(health_json, needed, &health_len);
        if (ret == ESP_OK) {
            esp_err_t send_ret = http_success_send_data_json(req, health_json);
            free(health_json);
            if (send_ret == ESP_ERR_NO_MEM) {
                // Avoid ring amplification loop for /health failures.
                return http_error_send(req, 503, "no_memory", "Failed to wrap health payload");
            }
            return send_ret;
        }

        free(health_json);
        if (ret != ESP_ERR_NO_MEM) {
            // Avoid ring amplification loop for /health failures.
            return http_error_send(req, 500, "internal_error", "Failed to build health payload");
        }
        needed *= 2;
    }

    // Avoid ring amplification loop for /health failures.
    return http_error_send(req, 503, "no_memory", "Health payload too large");
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

esp_err_t api_jobs_submit_handler(httpd_req_t *req)
{
    api_job_submit_request_t in = {0};
    esp_err_t err = api_parse_job_submit_request(req, &in);
    if (err != ESP_OK) {
        return http_error_send_esp(req, err, "Invalid job payload");
    }

    zgw_job_type_t type = parse_job_type(in.type);
    uint32_t job_id = 0;
    err = job_queue_submit(type, in.reboot_delay_ms, &job_id);
    if (err != ESP_OK) {
        return http_error_send_esp(req, err, "Failed to queue job");
    }

    char data_json[160];
    int written = snprintf(data_json, sizeof(data_json),
                           "{\"job_id\":%" PRIu32 ",\"type\":\"%s\",\"state\":\"queued\"}",
                           job_id, job_queue_type_to_string(type));
    if (written < 0 || (size_t)written >= sizeof(data_json)) {
        return http_error_send_esp(req, ESP_ERR_NO_MEM, "Failed to build job response");
    }
    return http_success_send_data_json(req, data_json);
}

esp_err_t api_jobs_get_handler(httpd_req_t *req)
{
    uint32_t job_id = 0;
    esp_err_t err = parse_job_id_from_uri(req->uri, &job_id);
    if (err != ESP_OK) {
        return http_error_send_esp(req, err, "Invalid job id");
    }

    zgw_job_info_t *info = (zgw_job_info_t *)calloc(1, sizeof(zgw_job_info_t));
    if (!info) {
        return http_error_send_esp(req, ESP_ERR_NO_MEM, "Out of memory");
    }

    err = job_queue_get(job_id, info);
    if (err != ESP_OK) {
        free(info);
        return http_error_send_esp(req, err, "Job not found");
    }

    const char *result_json = "null";
    char truncated_result_json[96];
    size_t result_limit = job_result_json_limit_for_type(info->type);
    if (info->has_result) {
        size_t result_len = strnlen(info->result_json, ZGW_JOB_RESULT_MAX_LEN);
        if (result_len > result_limit) {
            int t_written = snprintf(
                truncated_result_json, sizeof(truncated_result_json),
                "{\"truncated\":true,\"original_len\":%u,\"max_len\":%u}",
                (unsigned)result_len, (unsigned)result_limit);
            if (t_written < 0 || (size_t)t_written >= sizeof(truncated_result_json)) {
                free(info);
                return http_error_send_esp(req, ESP_ERR_NO_MEM, "Failed to build truncated result");
            }
            result_json = truncated_result_json;
        } else {
            result_json = info->result_json;
        }
    }
    int needed = snprintf(
        NULL, 0,
        "{\"job_id\":%" PRIu32 ",\"type\":\"%s\",\"state\":\"%s\",\"done\":%s,"
        "\"created_ms\":%" PRIu64 ",\"updated_ms\":%" PRIu64 ",\"error\":\"%s\",\"result\":%s}",
        info->id,
        job_queue_type_to_string(info->type),
        job_queue_state_to_string(info->state),
        (info->state == ZGW_JOB_STATE_SUCCEEDED || info->state == ZGW_JOB_STATE_FAILED) ? "true" : "false",
        info->created_ms,
        info->updated_ms,
        esp_err_to_name(info->err),
        result_json);
    if (needed < 0) {
        free(info);
        return http_error_send_esp(req, ESP_FAIL, "Failed to build job response");
    }

    size_t data_len = (size_t)needed + 1;
    char *data_json = (char *)malloc(data_len);
    if (!data_json) {
        free(info);
        return http_error_send_esp(req, ESP_ERR_NO_MEM, "Job response too large");
    }

    int written = snprintf(
        data_json, data_len,
        "{\"job_id\":%" PRIu32 ",\"type\":\"%s\",\"state\":\"%s\",\"done\":%s,"
        "\"created_ms\":%" PRIu64 ",\"updated_ms\":%" PRIu64 ",\"error\":\"%s\",\"result\":%s}",
        info->id,
        job_queue_type_to_string(info->type),
        job_queue_state_to_string(info->state),
        (info->state == ZGW_JOB_STATE_SUCCEEDED || info->state == ZGW_JOB_STATE_FAILED) ? "true" : "false",
        info->created_ms,
        info->updated_ms,
        esp_err_to_name(info->err),
        result_json);
    if (written < 0 || (size_t)written >= data_len) {
        free(data_json);
        free(info);
        return http_error_send_esp(req, ESP_ERR_NO_MEM, "Job response too large");
    }

    esp_err_t send_ret = http_success_send_data_json(req, data_json);
    free(data_json);
    free(info);
    return send_ret;
}
