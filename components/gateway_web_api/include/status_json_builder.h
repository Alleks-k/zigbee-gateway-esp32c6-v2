#pragma once

#include "esp_err.h"

#include <stddef.h>

char *create_status_json(void);
esp_err_t build_status_json_compact(char *out, size_t out_size, size_t *out_len);
esp_err_t build_devices_json_compact(char *out, size_t out_size, size_t *out_len);

