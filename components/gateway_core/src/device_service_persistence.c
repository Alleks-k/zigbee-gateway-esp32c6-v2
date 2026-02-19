#include "device_service_internal.h"

#include <stdbool.h>

#include "gateway_persistence_adapter.h"
#include "gateway_status_esp.h"
#include "esp_log.h"

static const char *TAG = "DEV_SERVICE_STORE";

gateway_status_t device_service_storage_save_locked(device_service_handle_t handle)
{
    if (!handle) {
        return GATEWAY_STATUS_INVALID_ARG;
    }

    gateway_status_t status =
        gateway_persistence_devices_save(handle->devices, MAX_DEVICES, handle->device_count);
    if (status == GATEWAY_STATUS_OK) {
        ESP_LOGI(TAG, "Device list successfully saved");
    } else {
        ESP_LOGW(TAG, "Failed to save devices: %s", esp_err_to_name(gateway_status_to_esp_err(status)));
    }
    return status;
}

gateway_status_t device_service_storage_load_locked(device_service_handle_t handle)
{
    if (!handle) {
        return GATEWAY_STATUS_INVALID_ARG;
    }

    bool loaded = false;
    int count = 0;
    gateway_status_t status = gateway_persistence_devices_load(handle->devices, MAX_DEVICES, &count, &loaded);
    if (status == GATEWAY_STATUS_OK && loaded) {
        handle->device_count = count;
        ESP_LOGI(TAG, "Loaded %d devices", handle->device_count);
    } else if (status == GATEWAY_STATUS_OK) {
        handle->device_count = 0;
        ESP_LOGW(TAG, "No device data found (first boot?)");
    } else {
        handle->device_count = 0;
        ESP_LOGW(TAG, "Failed to load device data: %s", esp_err_to_name(gateway_status_to_esp_err(status)));
    }

    return status;
}
