#pragma once

#include <stdbool.h>
#include "esp_err.h"

/**
 * @brief Initialize Wi-Fi STA and wait until GOT_IP
 *
 * @return esp_err_t
 */
esp_err_t wifi_init_sta_and_wait(void);
bool wifi_is_fallback_ap_active(void);
bool wifi_loaded_credentials_from_nvs(void);
bool wifi_sta_is_connected(void);
const char *wifi_get_active_ssid(void);
void wifi_state_store_update(void);
