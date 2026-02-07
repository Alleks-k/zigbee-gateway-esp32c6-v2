#pragma once

#include "esp_err.h"

/**
 * @brief Initialize Wi-Fi STA and wait until GOT_IP
 *
 * @return esp_err_t
 */
esp_err_t wifi_init_sta_and_wait(void);
