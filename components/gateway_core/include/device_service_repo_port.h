#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "gateway_config_types.h"
#include "gateway_status.h"

typedef struct {
    gateway_status_t (*load)(void *ctx,
                             gateway_device_record_t *devices,
                             size_t max_devices,
                             int *device_count,
                             bool *loaded);
    gateway_status_t (*save)(void *ctx,
                             const gateway_device_record_t *devices,
                             size_t max_devices,
                             int device_count);
    void *ctx;
} device_service_repo_port_t;
