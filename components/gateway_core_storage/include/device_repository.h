#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "esp_err.h"
#include "gateway_config_types.h"

esp_err_t device_repository_load(gateway_device_record_t *devices, size_t max_devices, int *device_count, bool *loaded);
esp_err_t device_repository_save(const gateway_device_record_t *devices, size_t max_devices, int device_count);
