#pragma once

#include "esp_err.h"
#include "api_usecases.h"

#include <stddef.h>

char *create_status_json(api_usecases_handle_t usecases);
esp_err_t build_status_json_compact(api_usecases_handle_t usecases, char *out, size_t out_size, size_t *out_len);
esp_err_t build_devices_json_compact(api_usecases_handle_t usecases, char *out, size_t out_size, size_t *out_len);
