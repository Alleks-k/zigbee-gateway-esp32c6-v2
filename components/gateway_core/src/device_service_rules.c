#include "device_service_rules.h"

#include <stdio.h>
#include <string.h>

static const char *device_rules_name_prefix(const char *prefix)
{
    return (prefix && prefix[0] != '\0') ? prefix : "Device";
}

device_service_rules_result_t device_service_rules_upsert(
    gateway_device_record_t *devices,
    int *device_count,
    size_t max_devices,
    uint16_t short_addr,
    const gateway_ieee_addr_t ieee_addr,
    const char *default_name_prefix)
{
    if (!devices || !device_count || !ieee_addr || *device_count < 0) {
        return DEVICE_SERVICE_RULES_RESULT_INVALID_ARG;
    }

    for (int i = 0; i < *device_count; i++) {
        if (devices[i].short_addr == short_addr) {
            memcpy(devices[i].ieee_addr, ieee_addr, sizeof(devices[i].ieee_addr));
            return DEVICE_SERVICE_RULES_RESULT_UPDATED;
        }
    }

    if ((size_t)(*device_count) >= max_devices) {
        return DEVICE_SERVICE_RULES_RESULT_LIMIT_REACHED;
    }

    gateway_device_record_t *slot = &devices[*device_count];
    slot->short_addr = short_addr;
    memcpy(slot->ieee_addr, ieee_addr, sizeof(slot->ieee_addr));
    snprintf(slot->name, sizeof(slot->name), "%s 0x%04x", device_rules_name_prefix(default_name_prefix), short_addr);
    slot->name[sizeof(slot->name) - 1] = '\0';
    (*device_count)++;
    return DEVICE_SERVICE_RULES_RESULT_ADDED;
}

bool device_service_rules_rename(
    gateway_device_record_t *devices,
    int device_count,
    uint16_t short_addr,
    const char *new_name)
{
    if (!devices || device_count < 0 || !new_name) {
        return false;
    }

    for (int i = 0; i < device_count; i++) {
        if (devices[i].short_addr == short_addr) {
            strncpy(devices[i].name, new_name, sizeof(devices[i].name) - 1);
            devices[i].name[sizeof(devices[i].name) - 1] = '\0';
            return true;
        }
    }

    return false;
}

int device_service_rules_find_index_by_short_addr(
    const gateway_device_record_t *devices,
    int device_count,
    uint16_t short_addr)
{
    if (!devices || device_count <= 0) {
        return -1;
    }

    for (int i = 0; i < device_count; i++) {
        if (devices[i].short_addr == short_addr) {
            return i;
        }
    }

    return -1;
}

bool device_service_rules_delete_by_short_addr(
    gateway_device_record_t *devices,
    int *device_count,
    uint16_t short_addr,
    gateway_device_record_t *deleted_record)
{
    if (!devices || !device_count || *device_count <= 0) {
        return false;
    }

    int found_idx = device_service_rules_find_index_by_short_addr(devices, *device_count, short_addr);
    if (found_idx < 0) {
        return false;
    }

    if (deleted_record) {
        *deleted_record = devices[found_idx];
    }

    for (int i = found_idx; i < (*device_count - 1); i++) {
        devices[i] = devices[i + 1];
    }
    (*device_count)--;
    return true;
}
