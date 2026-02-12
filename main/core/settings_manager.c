#include "settings_manager.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>

static SemaphoreHandle_t s_settings_mutex = NULL;

static esp_err_t settings_lock(void)
{
    if (s_settings_mutex == NULL) {
        s_settings_mutex = xSemaphoreCreateMutex();
        if (s_settings_mutex == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }
    xSemaphoreTake(s_settings_mutex, portMAX_DELAY);
    return ESP_OK;
}

static void settings_unlock(void)
{
    if (s_settings_mutex != NULL) {
        xSemaphoreGive(s_settings_mutex);
    }
}

esp_err_t settings_manager_load_wifi_credentials(char *ssid, size_t ssid_size,
                                                 char *password, size_t password_size,
                                                 bool *loaded)
{
    if (!ssid || !password || !loaded || ssid_size < 2 || password_size < 2) {
        return ESP_ERR_INVALID_ARG;
    }

    *loaded = false;
    ssid[0] = '\0';
    password[0] = '\0';

    esp_err_t err = settings_lock();
    if (err != ESP_OK) {
        return err;
    }

    nvs_handle_t handle;
    err = nvs_open("storage", NVS_READONLY, &handle);
    if (err == ESP_OK) {
        size_t ssid_len = ssid_size;
        size_t pass_len = password_size;
        if (nvs_get_str(handle, "wifi_ssid", ssid, &ssid_len) == ESP_OK &&
            nvs_get_str(handle, "wifi_pass", password, &pass_len) == ESP_OK) {
            *loaded = true;
        }
        nvs_close(handle);
        err = ESP_OK;
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        err = ESP_OK;
    }

    settings_unlock();
    return err;
}

esp_err_t settings_manager_save_wifi_credentials(const char *ssid, const char *password)
{
    if (!ssid || !password) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t ssid_len = strlen(ssid);
    size_t pass_len = strlen(password);
    if (ssid_len == 0 || ssid_len > 32 || pass_len < 8 || pass_len > 64) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = settings_lock();
    if (err != ESP_OK) {
        return err;
    }

    nvs_handle_t handle;
    err = nvs_open("storage", NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        err = nvs_set_str(handle, "wifi_ssid", ssid);
        if (err == ESP_OK) {
            err = nvs_set_str(handle, "wifi_pass", password);
        }
        if (err == ESP_OK) {
            err = nvs_commit(handle);
        }
        nvs_close(handle);
    }

    settings_unlock();
    return err;
}

esp_err_t settings_manager_load_devices(zb_device_t *devices, size_t max_devices,
                                        int *device_count, bool *loaded)
{
    if (!devices || !device_count || !loaded || max_devices == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    *loaded = false;
    *device_count = 0;
    memset(devices, 0, sizeof(zb_device_t) * max_devices);

    esp_err_t err = settings_lock();
    if (err != ESP_OK) {
        return err;
    }

    nvs_handle_t handle;
    err = nvs_open("storage", NVS_READONLY, &handle);
    if (err == ESP_OK) {
        int32_t count = 0;
        if (nvs_get_i32(handle, "dev_count", &count) == ESP_OK) {
            if (count < 0) {
                count = 0;
            }
            if ((size_t)count > max_devices) {
                count = (int32_t)max_devices;
            }
            size_t size = sizeof(zb_device_t) * max_devices;
            if (nvs_get_blob(handle, "dev_list", devices, &size) == ESP_OK) {
                *device_count = (int)count;
                *loaded = true;
            }
        }
        nvs_close(handle);
        err = ESP_OK;
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        err = ESP_OK;
    }

    settings_unlock();
    return err;
}

esp_err_t settings_manager_save_devices(const zb_device_t *devices, size_t max_devices,
                                        int device_count)
{
    if (!devices || max_devices == 0 || device_count < 0 || (size_t)device_count > max_devices) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = settings_lock();
    if (err != ESP_OK) {
        return err;
    }

    nvs_handle_t handle;
    err = nvs_open("storage", NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        err = nvs_set_i32(handle, "dev_count", device_count);
        if (err == ESP_OK) {
            err = nvs_set_blob(handle, "dev_list", devices, sizeof(zb_device_t) * max_devices);
        }
        if (err == ESP_OK) {
            err = nvs_commit(handle);
        }
        nvs_close(handle);
    }

    settings_unlock();
    return err;
}

esp_err_t settings_manager_factory_reset(void)
{
    esp_err_t err = settings_lock();
    if (err != ESP_OK) {
        return err;
    }

    nvs_handle_t handle;
    err = nvs_open("storage", NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        err = nvs_erase_all(handle);
        if (err == ESP_OK) {
            err = nvs_commit(handle);
        }
        nvs_close(handle);
    }

    settings_unlock();
    return err;
}

