#pragma once

#include "esp_err.h"
#include <stddef.h>

esp_err_t build_lqi_json_compact(char *out, size_t out_size, size_t *out_len);

