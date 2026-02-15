#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "esp_err.h"

esp_err_t config_repository_load_wifi_credentials(char *ssid, size_t ssid_size,
                                                  char *password, size_t password_size,
                                                  bool *loaded);
esp_err_t config_repository_save_wifi_credentials(const char *ssid, const char *password);
esp_err_t config_repository_clear_wifi_credentials(void);
