#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "gateway_config_types.h"

typedef enum {
    DEVICE_SERVICE_RULES_RESULT_INVALID_ARG = -1,
    DEVICE_SERVICE_RULES_RESULT_NO_CHANGE = 0,
    DEVICE_SERVICE_RULES_RESULT_UPDATED = 1,
    DEVICE_SERVICE_RULES_RESULT_ADDED = 2,
    DEVICE_SERVICE_RULES_RESULT_LIMIT_REACHED = 3,
} device_service_rules_result_t;

device_service_rules_result_t device_service_rules_upsert(
    gateway_device_record_t *devices,
    int *device_count,
    size_t max_devices,
    uint16_t short_addr,
    const gateway_ieee_addr_t ieee_addr,
    const char *default_name_prefix);

bool device_service_rules_rename(
    gateway_device_record_t *devices,
    int device_count,
    uint16_t short_addr,
    const char *new_name);

int device_service_rules_find_index_by_short_addr(
    const gateway_device_record_t *devices,
    int device_count,
    uint16_t short_addr);

bool device_service_rules_delete_by_short_addr(
    gateway_device_record_t *devices,
    int *device_count,
    uint16_t short_addr,
    gateway_device_record_t *deleted_record);
