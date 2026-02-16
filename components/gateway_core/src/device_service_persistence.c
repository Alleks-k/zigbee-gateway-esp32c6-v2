#include "device_service_internal.h"

#include <stdbool.h>

#include "device_repository.h"
#include "esp_log.h"

static const char *TAG = "DEV_SERVICE_STORE";

esp_err_t device_service_storage_save_locked(device_service_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = device_repository_save(handle->devices, MAX_DEVICES, handle->device_count);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Device list successfully saved");
    } else {
        ESP_LOGW(TAG, "Failed to save devices: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t device_service_storage_load_locked(device_service_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }

    bool loaded = false;
    int count = 0;
    esp_err_t err = device_repository_load(handle->devices, MAX_DEVICES, &count, &loaded);
    if (err == ESP_OK && loaded) {
        handle->device_count = count;
        ESP_LOGI(TAG, "Loaded %d devices", handle->device_count);
    } else if (err == ESP_OK) {
        handle->device_count = 0;
        ESP_LOGW(TAG, "No device data found (first boot?)");
    } else {
        handle->device_count = 0;
        ESP_LOGW(TAG, "Failed to load device data: %s", esp_err_to_name(err));
    }

    return err;
}
