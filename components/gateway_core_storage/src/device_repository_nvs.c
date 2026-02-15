#include "device_repository.h"

#include "device_manager.h"
#include "settings_manager.h"

esp_err_t device_repository_load(gateway_device_record_t *devices, size_t max_devices, int *device_count, bool *loaded)
{
    if (!devices || !device_count || !loaded || max_devices == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    return settings_manager_load_devices((zb_device_t *)devices, max_devices, device_count, loaded);
}

esp_err_t device_repository_save(const gateway_device_record_t *devices, size_t max_devices, int device_count)
{
    if (!devices || max_devices == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    return settings_manager_save_devices((const zb_device_t *)devices, max_devices, device_count);
}
