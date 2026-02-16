#pragma once

#include "esp_err.h"

#include <stddef.h>

/**
 * @brief Побудувати компактний JSON зі станом health.
 * @param out буфер призначення
 * @param out_size розмір буфера
 * @param out_len опціональна довжина результату
 */
esp_err_t build_health_json_compact(char *out, size_t out_size, size_t *out_len);

