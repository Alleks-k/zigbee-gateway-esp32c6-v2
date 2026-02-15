#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

esp_err_t storage_schema_init(void);
esp_err_t storage_schema_get_version(int32_t *out_version, bool *out_found);
esp_err_t storage_schema_set_version(int32_t version);
