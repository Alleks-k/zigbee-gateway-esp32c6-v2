#include "lqi_json_mapper.h"
#include "api_usecases.h"
#include <stdbool.h>
#include <inttypes.h>
#include <stdio.h>

#define LQI_UNKNOWN_VALUE (-1)

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

static bool lqi_value_invalid(int lqi)
{
    return (lqi <= 0);
}

static bool rssi_value_invalid(int rssi)
{
    return (rssi == 127 || rssi <= -127);
}

static const char *lqi_quality_label(int lqi)
{
    if (lqi_value_invalid(lqi)) {
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

esp_err_t build_lqi_json_compact(api_usecases_handle_t usecases, char *out, size_t out_size, size_t *out_len)
{
    if (!usecases || !out || out_size < 2) {
        return ESP_ERR_INVALID_ARG;
    }

    zb_device_t devices[MAX_DEVICES] = {0};
    zigbee_neighbor_lqi_t neighbors[MAX_DEVICES] = {0};
    const int nbr_capacity = (int)(sizeof(neighbors) / sizeof(neighbors[0]));
    int dev_count = api_usecase_get_devices_snapshot(usecases, devices, MAX_DEVICES);
    int nbr_count = 0;
    zigbee_lqi_source_t source = ZIGBEE_LQI_SOURCE_UNKNOWN;
    uint64_t updated_ms = 0;

    esp_err_t cached_ret = api_usecase_get_cached_lqi_snapshot(
        usecases,
        neighbors, MAX_DEVICES, &nbr_count, &source, &updated_ms);
    if (cached_ret != ESP_OK) {
        return cached_ret;
    }
    if (nbr_count < 0) {
        nbr_count = 0;
    }
    if (nbr_count > nbr_capacity) {
        nbr_count = nbr_capacity;
    }

    if (updated_ms == 0) {
        nbr_count = api_usecase_get_neighbor_lqi_snapshot(usecases, neighbors, MAX_DEVICES);
        if (nbr_count < 0) {
            nbr_count = 0;
        }
        if (nbr_count > nbr_capacity) {
            nbr_count = nbr_capacity;
        }
        source = ZIGBEE_LQI_SOURCE_NEIGHBOR_TABLE;
        if (nbr_count > 0) {
            updated_ms = neighbors[0].updated_ms;
            for (int i = 1; i < nbr_count && i < nbr_capacity; i++) {
                if (neighbors[i].updated_ms > updated_ms) {
                    updated_ms = neighbors[i].updated_ms;
                }
            }
        }
    }

    char *cursor = out;
    size_t remaining = out_size;
    if (!append_literal(&cursor, &remaining, "{\"neighbors\":[")) {
        return ESP_ERR_NO_MEM;
    }

    for (int i = 0; i < dev_count; i++) {
        int lqi = LQI_UNKNOWN_VALUE;
        int rssi = 127;
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

        if (lqi_value_invalid(lqi)) {
            if (!append_literal(&cursor, &remaining, "null")) {
                return ESP_ERR_NO_MEM;
            }
        } else if (!append_i32(&cursor, &remaining, lqi)) {
            return ESP_ERR_NO_MEM;
        }

        if (!append_literal(&cursor, &remaining, ",\"rssi\":")) {
            return ESP_ERR_NO_MEM;
        }
        if (rssi_value_invalid(rssi)) {
            if (!append_literal(&cursor, &remaining, "null")) {
                return ESP_ERR_NO_MEM;
            }
        } else if (!append_i32(&cursor, &remaining, rssi)) {
            return ESP_ERR_NO_MEM;
        }

        if (!append_literal(&cursor, &remaining, ",\"quality\":\"") ||
            !append_literal(&cursor, &remaining, lqi_quality_label(lqi)) ||
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
