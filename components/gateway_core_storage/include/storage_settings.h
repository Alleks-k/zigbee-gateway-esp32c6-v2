#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "gateway_config_types.h"

typedef gateway_device_record_t zb_device_t;

#define SETTINGS_SCHEMA_VERSION_CURRENT 1

esp_err_t settings_manager_init_or_migrate(void);
esp_err_t settings_manager_get_schema_version(int32_t *out_version);

esp_err_t settings_manager_load_wifi_credentials(char *ssid, size_t ssid_size,
                                                 char *password, size_t password_size,
                                                 bool *loaded);

esp_err_t settings_manager_save_wifi_credentials(const char *ssid, const char *password);

esp_err_t settings_manager_load_devices(zb_device_t *devices, size_t max_devices,
                                        int *device_count, bool *loaded);

esp_err_t settings_manager_save_devices(const zb_device_t *devices, size_t max_devices,
                                        int device_count);

esp_err_t settings_manager_clear_wifi_credentials(void);
esp_err_t settings_manager_clear_devices(void);
esp_err_t settings_manager_erase_zigbee_storage_partition(void);
esp_err_t settings_manager_erase_zigbee_factory_partition(void);
