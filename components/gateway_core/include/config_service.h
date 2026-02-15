#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "gateway_config_types.h"

#define CONFIG_SERVICE_SCHEMA_VERSION_CURRENT 1

typedef gateway_factory_reset_report_t config_service_factory_reset_report_t;

esp_err_t config_service_init_or_migrate(void);
esp_err_t config_service_get_schema_version(int32_t *out_version);

esp_err_t config_service_validate_wifi_credentials(const char *ssid, const char *password);
esp_err_t config_service_save_wifi_credentials(const char *ssid, const char *password);
esp_err_t config_service_load_wifi_credentials(char *ssid, size_t ssid_size,
                                               char *password, size_t password_size,
                                               bool *loaded_from_storage);
esp_err_t config_service_factory_reset(void);
esp_err_t config_service_get_last_factory_reset_report(config_service_factory_reset_report_t *out_report);
