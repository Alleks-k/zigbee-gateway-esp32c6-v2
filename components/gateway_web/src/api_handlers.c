#include "api_handlers.h"
#include "api_contracts.h"
#include "api_usecases.h"
#include "http_error.h"
#include "gateway_state.h"
#include "settings_manager.h"
#include "job_queue.h"
#include "ws_manager.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#if __has_include("driver/temperature_sensor.h")
#include "driver/temperature_sensor.h"
#define GATEWAY_HAS_TEMP_SENSOR 1
#else
#define GATEWAY_HAS_TEMP_SENSOR 0
#endif
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
#define LQI_UNKNOWN_VALUE                       (-1)

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

static bool read_temperature_c(float *out_temp_c)
{
    if (!out_temp_c) {
        return false;
    }
#if GATEWAY_HAS_TEMP_SENSOR
    if (!s_temp_sensor_init_attempted) {
        s_temp_sensor_init_attempted = true;
        temperature_sensor_config_t cfg = TEMPERATURE_SENSOR_CONFIG_DEFAULT(-20, 100);
        if (temperature_sensor_install(&cfg, &s_temp_sensor_handle) == ESP_OK &&
            temperature_sensor_enable(s_temp_sensor_handle) == ESP_OK)
        {
            s_temp_sensor_available = true;
        } else {
            s_temp_sensor_available = false;
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

static bool lqi_measurement_invalid(int lqi, int rssi)
{
    return (lqi <= 0 || rssi == 127 || rssi <= -127);
}

static const char *lqi_quality_label(int lqi, int rssi)
{
    if (lqi_measurement_invalid(lqi, rssi)) {
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

static const char *lqi_source_label(zigbee_lqi_source_t source)
{
    switch (source) {
    case ZIGBEE_LQI_SOURCE_MGMT_LQI:
        return "mgmt_lqi";
    case ZIGBEE_LQI_SOURCE_NEIGHBOR_TABLE:
        return "neighbor_table";
    case ZIGBEE_LQI_SOURCE_UNKNOWN:
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

esp_err_t build_lqi_json_compact(char *out, size_t out_size, size_t *out_len)
{
    if (!out || out_size < 2) {
        return ESP_ERR_INVALID_ARG;
    }

    zb_device_t devices[MAX_DEVICES] = {0};
    zigbee_neighbor_lqi_t neighbors[MAX_DEVICES] = {0};
    int dev_count = api_usecase_get_devices_snapshot(devices, MAX_DEVICES);
    int nbr_count = 0;
    zigbee_lqi_source_t source = ZIGBEE_LQI_SOURCE_UNKNOWN;
    uint64_t updated_ms = 0;

    esp_err_t cached_ret = api_usecase_get_cached_lqi_snapshot(
        neighbors, MAX_DEVICES, &nbr_count, &source, &updated_ms);
    if (cached_ret != ESP_OK) {
        return cached_ret;
    }

    if (updated_ms == 0) {
        /* Prime cache from neighbor table if cache is empty. */
        nbr_count = api_usecase_get_neighbor_lqi_snapshot(neighbors, MAX_DEVICES);
        source = ZIGBEE_LQI_SOURCE_NEIGHBOR_TABLE;
        updated_ms = (uint64_t)(esp_timer_get_time() / 1000);
    }

    char *cursor = out;
    size_t remaining = out_size;
    if (!append_literal(&cursor, &remaining, "{\"neighbors\":[")) {
        return ESP_ERR_NO_MEM;
    }

    for (int i = 0; i < dev_count; i++) {
        int lqi = LQI_UNKNOWN_VALUE;
        int rssi = 0;
        bool direct = false;
        zigbee_lqi_source_t row_source = ZIGBEE_LQI_SOURCE_UNKNOWN;
        uint64_t row_updated_ms = 0;
        for (int j = 0; j < nbr_count; j++) {
            if (neighbors[j].short_addr == devices[i].short_addr) {
                lqi = neighbors[j].lqi;
                rssi = neighbors[j].rssi;
                direct = true;
                row_source = neighbors[j].source;
                row_updated_ms = neighbors[j].updated_ms;
                break;
            }
        }

        if (i > 0 && !append_literal(&cursor, &remaining, ",")) {
            return ESP_ERR_NO_MEM;
        }
        if (!append_literal(&cursor, &remaining, "{\"short_addr\":") ||
            !append_u32(&cursor, &remaining, devices[i].short_addr) ||
            !append_literal(&cursor, &remaining, ",\"name\":\"") ||
            !append_json_escaped(&cursor, &remaining, devices[i].name) ||
            !append_literal(&cursor, &remaining, "\",\"lqi\":"))
        {
            return ESP_ERR_NO_MEM;
        }

        if (lqi_measurement_invalid(lqi, rssi)) {
            if (!append_literal(&cursor, &remaining, "null")) {
                return ESP_ERR_NO_MEM;
            }
        } else if (!append_i32(&cursor, &remaining, lqi)) {
            return ESP_ERR_NO_MEM;
        }

        if (!append_literal(&cursor, &remaining, ",\"rssi\":")) {
            return ESP_ERR_NO_MEM;
        }
        if (lqi_measurement_invalid(lqi, rssi)) {
            if (!append_literal(&cursor, &remaining, "null")) {
                return ESP_ERR_NO_MEM;
            }
        } else if (!append_i32(&cursor, &remaining, rssi)) {
            return ESP_ERR_NO_MEM;
        }

        if (!append_literal(&cursor, &remaining, ",\"quality\":\"") ||
            !append_literal(&cursor, &remaining, lqi_quality_label(lqi, rssi)) ||
            !append_literal(&cursor, &remaining, "\",\"direct\":") ||
            !append_literal(&cursor, &remaining, direct ? "true" : "false") ||
            !append_literal(&cursor, &remaining, ",\"source\":\"") ||
            !append_literal(&cursor, &remaining, lqi_source_label(row_source)) ||
            !append_literal(&cursor, &remaining, "\",\"updated_ms\":") ||
            !append_u64(&cursor, &remaining, row_updated_ms) ||
            !append_literal(&cursor, &remaining, "}"))
        {
            return ESP_ERR_NO_MEM;
        }
    }

    if (!append_literal(&cursor, &remaining, "],\"updated_ms\":") ||
        !append_u64(&cursor, &remaining, updated_ms) ||
        !append_literal(&cursor, &remaining, ",\"source\":\"") ||
        !append_literal(&cursor, &remaining, lqi_source_label(source)) ||
        !append_literal(&cursor, &remaining, "\"}"))
    {
        return ESP_ERR_NO_MEM;
    }

    if (out_len) {
        *out_len = (size_t)(cursor - out);
    }
    return ESP_OK;
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

    gateway_network_state_t gw_state = {0};
    esp_err_t gw_ret = gateway_state_get_network(&gw_state);
    if (gw_ret != ESP_OK) {
        return gw_ret;
    }
    gateway_wifi_state_t wifi_state = {0};
    esp_err_t wifi_ret = gateway_state_get_wifi(&wifi_state);
    if (wifi_ret != ESP_OK) {
        return wifi_ret;
    }

    int32_t schema_version = 0;
    esp_err_t schema_ret = settings_manager_get_schema_version(&schema_version);

    const bool fallback_ap = wifi_state.fallback_ap_active;
    const bool sta_connected = wifi_state.sta_connected;
    const bool loaded_from_nvs = wifi_state.loaded_from_nvs;
    const char *active_ssid = wifi_state.active_ssid;
    if (!active_ssid) {
        active_ssid = "";
    }
    const uint64_t uptime_ms = (uint64_t)(esp_timer_get_time() / 1000);
    const uint32_t heap_free = esp_get_free_heap_size();
    const uint32_t heap_min_free = esp_get_minimum_free_heap_size();
    int32_t wifi_rssi = 0;
    const bool has_wifi_rssi = read_wifi_rssi(&wifi_rssi);
    float temp_c = 0.0f;
    const bool has_temp_c = read_temperature_c(&temp_c);

    char *cursor = out;
    size_t remaining = out_size;

    if (!append_literal(&cursor, &remaining, "{\"wifi\":{")) {
        return ESP_ERR_NO_MEM;
    }
    if (!append_literal(&cursor, &remaining, "\"sta_connected\":") ||
        !append_literal(&cursor, &remaining, sta_connected ? "true" : "false") ||
        !append_literal(&cursor, &remaining, ",\"fallback_ap_active\":") ||
        !append_literal(&cursor, &remaining, fallback_ap ? "true" : "false") ||
        !append_literal(&cursor, &remaining, ",\"loaded_from_nvs\":") ||
        !append_literal(&cursor, &remaining, loaded_from_nvs ? "true" : "false") ||
        !append_literal(&cursor, &remaining, ",\"active_ssid\":\"") ||
        !append_json_escaped(&cursor, &remaining, active_ssid) ||
        !append_literal(&cursor, &remaining, "\",\"rssi\":"))
    {
        return ESP_ERR_NO_MEM;
    }
    if (has_wifi_rssi) {
        if (!append_i32(&cursor, &remaining, wifi_rssi)) {
            return ESP_ERR_NO_MEM;
        }
    } else if (!append_literal(&cursor, &remaining, "null")) {
        return ESP_ERR_NO_MEM;
    }

    if (!append_literal(&cursor, &remaining, "\"},\"zigbee\":{") ||
        !append_literal(&cursor, &remaining, "\"started\":") ||
        !append_literal(&cursor, &remaining, gw_state.zigbee_started ? "true" : "false") ||
        !append_literal(&cursor, &remaining, ",\"factory_new\":") ||
        !append_literal(&cursor, &remaining, gw_state.factory_new ? "true" : "false") ||
        !append_literal(&cursor, &remaining, ",\"pan_id\":") ||
        !append_u32(&cursor, &remaining, gw_state.pan_id) ||
        !append_literal(&cursor, &remaining, ",\"channel\":") ||
        !append_u32(&cursor, &remaining, gw_state.channel) ||
        !append_literal(&cursor, &remaining, ",\"short_addr\":") ||
        !append_u32(&cursor, &remaining, gw_state.short_addr) ||
        !append_literal(&cursor, &remaining, "},\"web\":{") ||
        !append_literal(&cursor, &remaining, "\"ws_clients\":") ||
        !append_u32(&cursor, &remaining, (uint32_t)ws_manager_get_client_count()) ||
        !append_literal(&cursor, &remaining, ",\"ready\":true},\"nvs\":{") ||
        !append_literal(&cursor, &remaining, "\"ok\":") ||
        !append_literal(&cursor, &remaining, schema_ret == ESP_OK ? "true" : "false") ||
        !append_literal(&cursor, &remaining, ",\"schema_version\":") ||
        !append_i32(&cursor, &remaining, schema_ret == ESP_OK ? schema_version : -1) ||
        !append_literal(&cursor, &remaining, "},\"system\":{") ||
        !append_literal(&cursor, &remaining, "\"uptime_ms\":") ||
        !append_u64(&cursor, &remaining, uptime_ms) ||
        !append_literal(&cursor, &remaining, ",\"temperature_c\":"))
    {
        return ESP_ERR_NO_MEM;
    }
    if (has_temp_c) {
        int written = snprintf(cursor, remaining, "%.2f", (double)temp_c);
        if (written < 0 || (size_t)written >= remaining) {
            return ESP_ERR_NO_MEM;
        }
        cursor += written;
        remaining -= (size_t)written;
    } else if (!append_literal(&cursor, &remaining, "null")) {
        return ESP_ERR_NO_MEM;
    }

    if (!append_literal(&cursor, &remaining, "},\"heap\":{") ||
        !append_literal(&cursor, &remaining, "\"free\":") ||
        !append_u32(&cursor, &remaining, heap_free) ||
        !append_literal(&cursor, &remaining, ",\"minimum_free\":") ||
        !append_u32(&cursor, &remaining, heap_min_free) ||
        !append_literal(&cursor, &remaining, "}}"))
    {
        return ESP_ERR_NO_MEM;
    }

    if (out_len) {
        *out_len = (size_t)(cursor - out);
    }
    return ESP_OK;
}

esp_err_t api_health_handler(httpd_req_t *req)
{
    char health_json[768];
    size_t health_len = 0;
    esp_err_t ret = build_health_json_compact(health_json, sizeof(health_json), &health_len);
    if (ret != ESP_OK) {
        return http_error_send_esp(req, ret, "Failed to build health payload");
    }

    esp_err_t send_ret = http_success_send_data_json(req, health_json);
    if (send_ret == ESP_ERR_NO_MEM) {
        return http_error_send_esp(req, ESP_ERR_NO_MEM, "Failed to wrap health payload");
    }
    return send_ret;
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
