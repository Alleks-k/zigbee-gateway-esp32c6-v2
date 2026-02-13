#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"
#include "device_manager.h"

typedef struct {
    esp_err_t wifi_err;
    esp_err_t devices_err;
    esp_err_t zigbee_storage_err;
    esp_err_t zigbee_fct_err;
} settings_manager_factory_reset_report_t;

esp_err_t settings_manager_load_wifi_credentials(char *ssid, size_t ssid_size,
                                                 char *password, size_t password_size,
                                                 bool *loaded);

esp_err_t settings_manager_save_wifi_credentials(const char *ssid, const char *password);

esp_err_t settings_manager_load_devices(zb_device_t *devices, size_t max_devices,
                                        int *device_count, bool *loaded);

esp_err_t settings_manager_save_devices(const zb_device_t *devices, size_t max_devices,
                                        int device_count);

esp_err_t settings_manager_factory_reset(void);
esp_err_t settings_manager_get_last_factory_reset_report(settings_manager_factory_reset_report_t *out_report);
