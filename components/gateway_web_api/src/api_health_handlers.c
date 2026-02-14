#include "api_handlers.h"
#include "api_usecases.h"
#include "http_error.h"
#include "error_ring.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

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
        !append_literal(&cursor, &remaining, ",\"active_ssid\":\"") ||
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
            return http_error_send(req, 503, "no_memory", "Out of memory");
        }

        size_t health_len = 0;
        esp_err_t ret = build_health_json_compact(health_json, needed, &health_len);
        if (ret == ESP_OK) {
            esp_err_t send_ret = http_success_send_data_json(req, health_json);
            free(health_json);
            if (send_ret == ESP_ERR_NO_MEM) {
                return http_error_send(req, 503, "no_memory", "Failed to wrap health payload");
            }
            return send_ret;
        }

        free(health_json);
        if (ret != ESP_ERR_NO_MEM) {
            return http_error_send(req, 500, "internal_error", "Failed to build health payload");
        }
        needed *= 2;
    }

    return http_error_send(req, 503, "no_memory", "Health payload too large");
}
