#include "device_repository.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "storage_kv.h"

static const char *NVS_NAMESPACE = "storage";
static SemaphoreHandle_t s_devices_mutex = NULL;

static esp_err_t devices_lock(void)
{
    if (s_devices_mutex == NULL) {
        s_devices_mutex = xSemaphoreCreateMutex();
        if (s_devices_mutex == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }
    xSemaphoreTake(s_devices_mutex, portMAX_DELAY);
    return ESP_OK;
}

static void devices_unlock(void)
{
    if (s_devices_mutex != NULL) {
        xSemaphoreGive(s_devices_mutex);
    }
}

static esp_err_t erase_devices_locked(void)
{
    storage_kv_handle_t handle = NULL;
    esp_err_t err = storage_kv_open_readwrite(NVS_NAMESPACE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    err = storage_kv_erase_key(handle, "dev_count", NULL);
    if (err == ESP_OK) {
        err = storage_kv_erase_key(handle, "dev_list", NULL);
    }
    if (err == ESP_OK) {
        err = storage_kv_commit(handle);
    }
    storage_kv_close(handle);
    return err;
}

esp_err_t device_repository_load(gateway_device_record_t *devices, size_t max_devices, int *device_count, bool *loaded)
{
    if (!devices || !device_count || !loaded || max_devices == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    *loaded = false;
    *device_count = 0;
    memset(devices, 0, sizeof(gateway_device_record_t) * max_devices);

    esp_err_t err = devices_lock();
    if (err != ESP_OK) {
        return err;
    }

    storage_kv_handle_t handle = NULL;
    err = storage_kv_open_readonly(NVS_NAMESPACE, &handle);
    if (err == ESP_OK) {
        int32_t count = 0;
        bool count_found = false;
        if (storage_kv_get_i32(handle, "dev_count", &count, &count_found) == ESP_OK && count_found) {
            if (count < 0) {
                count = 0;
            }
            if ((size_t)count > max_devices) {
                count = (int32_t)max_devices;
            }
            size_t out_len = 0;
            bool blob_found = false;
            if (storage_kv_get_blob(handle, "dev_list", devices, sizeof(gateway_device_record_t) * max_devices, &out_len,
                                    &blob_found) == ESP_OK &&
                blob_found) {
                *device_count = (int)count;
                *loaded = true;
            }
        }
        storage_kv_close(handle);
        err = ESP_OK;
    } else if (err == ESP_ERR_NOT_FOUND) {
        err = ESP_OK;
    }

    devices_unlock();
    return err;
}

esp_err_t device_repository_save(const gateway_device_record_t *devices, size_t max_devices, int device_count)
{
    if (!devices || max_devices == 0 || device_count < 0 || (size_t)device_count > max_devices) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = devices_lock();
    if (err != ESP_OK) {
        return err;
    }

    storage_kv_handle_t handle = NULL;
    err = storage_kv_open_readwrite(NVS_NAMESPACE, &handle);
    if (err == ESP_OK) {
        err = storage_kv_set_i32(handle, "dev_count", device_count);
        if (err == ESP_OK) {
            err = storage_kv_set_blob(handle, "dev_list", devices, sizeof(gateway_device_record_t) * max_devices);
        }
        if (err == ESP_OK) {
            err = storage_kv_commit(handle);
        }
        storage_kv_close(handle);
    }

    devices_unlock();
    return err;
}

esp_err_t device_repository_clear(void)
{
    esp_err_t err = devices_lock();
    if (err != ESP_OK) {
        return err;
    }

    err = erase_devices_locked();
    devices_unlock();
    return err;
}
