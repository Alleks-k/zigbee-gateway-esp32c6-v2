#pragma once

#include "esp_err.h"
#include "api_usecases.h"
#include <stddef.h>

esp_err_t build_lqi_json_compact(api_usecases_handle_t usecases, char *out, size_t out_size, size_t *out_len);
