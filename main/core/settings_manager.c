#include "settings_manager.h"
#include "nvs.h"
#include "esp_partition.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "SETTINGS_MANAGER";
static const char *NVS_NAMESPACE = "storage";
static const char *KEY_SCHEMA_VER = "schema_ver";
static SemaphoreHandle_t s_settings_mutex = NULL;
static settings_manager_factory_reset_report_t s_last_factory_reset_report = {
    .wifi_err = ESP_ERR_INVALID_STATE,
    .devices_err = ESP_ERR_INVALID_STATE,
    .zigbee_storage_err = ESP_ERR_INVALID_STATE,
    .zigbee_fct_err = ESP_ERR_INVALID_STATE,
};

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

static esp_err_t settings_get_schema_version_locked(nvs_handle_t handle, int32_t *out_version)
{
    esp_err_t err = nvs_get_i32(handle, KEY_SCHEMA_VER, out_version);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        *out_version = 0; // Legacy schema: keys exist without explicit version.
        return ESP_OK;
    }
    return err;
}

esp_err_t settings_manager_get_schema_version(int32_t *out_version)
{
    if (!out_version) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = settings_lock();
    if (err != ESP_OK) {
        return err;
    }

    nvs_handle_t handle;
    err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_OK) {
        err = settings_get_schema_version_locked(handle, out_version);
        nvs_close(handle);
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        *out_version = 0;
        err = ESP_OK;
    }

    settings_unlock();
    return err;
}

esp_err_t settings_manager_init_or_migrate(void)
{
    esp_err_t err = settings_lock();
    if (err != ESP_OK) {
        return err;
    }

    nvs_handle_t handle;
    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        settings_unlock();
        return err;
    }

    int32_t version = 0;
    err = settings_get_schema_version_locked(handle, &version);
    if (err != ESP_OK) {
        nvs_close(handle);
        settings_unlock();
        return err;
    }

    if (version > SETTINGS_SCHEMA_VERSION_CURRENT) {
        ESP_LOGE(TAG, "Unsupported settings schema version: %ld > %d",
                 (long)version, SETTINGS_SCHEMA_VERSION_CURRENT);
        nvs_close(handle);
        settings_unlock();
        return ESP_ERR_INVALID_VERSION;
    }

    if (version < SETTINGS_SCHEMA_VERSION_CURRENT) {
        // v0 -> v1: baseline migration (introduce explicit schema key).
        int32_t target = SETTINGS_SCHEMA_VERSION_CURRENT;
        err = nvs_set_i32(handle, KEY_SCHEMA_VER, target);
        if (err == ESP_OK) {
            err = nvs_commit(handle);
        }
        if (err != ESP_OK) {
            nvs_close(handle);
            settings_unlock();
            return err;
        }
        ESP_LOGI(TAG, "Settings schema migrated: v%ld -> v%d",
                 (long)version, SETTINGS_SCHEMA_VERSION_CURRENT);
    } else {
        ESP_LOGI(TAG, "Settings schema up-to-date: v%d", SETTINGS_SCHEMA_VERSION_CURRENT);
    }

    nvs_close(handle);
    settings_unlock();
    return ESP_OK;
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
    err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
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
    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
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
    err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
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
    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
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
    esp_err_t wifi_err = ESP_OK;
    esp_err_t devices_err = ESP_OK;
    esp_err_t zb_storage_err = ESP_OK;
    esp_err_t zb_fct_err = ESP_OK;

    esp_err_t err = settings_lock();
    if (err != ESP_OK) {
        return err;
    }

    nvs_handle_t handle;
    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        esp_err_t key_err = nvs_erase_key(handle, "wifi_ssid");
        if (key_err != ESP_OK && key_err != ESP_ERR_NVS_NOT_FOUND) {
            wifi_err = key_err;
        }
        key_err = nvs_erase_key(handle, "wifi_pass");
        if (wifi_err == ESP_OK && key_err != ESP_OK && key_err != ESP_ERR_NVS_NOT_FOUND) {
            wifi_err = key_err;
        }
        if (wifi_err == ESP_OK) {
            esp_err_t commit_err = nvs_commit(handle);
            if (commit_err != ESP_OK) {
                wifi_err = commit_err;
            }
        }

        key_err = nvs_erase_key(handle, "dev_count");
        if (key_err != ESP_OK && key_err != ESP_ERR_NVS_NOT_FOUND) {
            devices_err = key_err;
        }
        key_err = nvs_erase_key(handle, "dev_list");
        if (devices_err == ESP_OK && key_err != ESP_OK && key_err != ESP_ERR_NVS_NOT_FOUND) {
            devices_err = key_err;
        }
        if (devices_err == ESP_OK) {
            esp_err_t commit_err = nvs_commit(handle);
            if (commit_err != ESP_OK) {
                devices_err = commit_err;
            }
        }

        nvs_close(handle);
    } else {
        wifi_err = err;
        devices_err = err;
    }

    settings_unlock();

    const esp_partition_t *zb_storage_part =
        esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "zb_storage");
    if (!zb_storage_part) {
        zb_storage_err = ESP_ERR_NOT_FOUND;
    } else {
        zb_storage_err = esp_partition_erase_range(zb_storage_part, 0, zb_storage_part->size);
    }

    const esp_partition_t *zb_fct_part =
        esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "zb_fct");
    if (!zb_fct_part) {
        zb_fct_err = ESP_ERR_NOT_FOUND;
    } else {
        zb_fct_err = esp_partition_erase_range(zb_fct_part, 0, zb_fct_part->size);
    }

    ESP_LOGI(TAG, "Factory reset group result: wifi=%s, devices=%s, zigbee_storage=%s, zigbee_fct=%s",
             esp_err_to_name(wifi_err),
             esp_err_to_name(devices_err),
             esp_err_to_name(zb_storage_err),
             esp_err_to_name(zb_fct_err));

    s_last_factory_reset_report.wifi_err = wifi_err;
    s_last_factory_reset_report.devices_err = devices_err;
    s_last_factory_reset_report.zigbee_storage_err = zb_storage_err;
    s_last_factory_reset_report.zigbee_fct_err = zb_fct_err;

    if (wifi_err != ESP_OK) {
        return wifi_err;
    }
    if (devices_err != ESP_OK) {
        return devices_err;
    }
    if (zb_storage_err != ESP_OK && zb_storage_err != ESP_ERR_NOT_FOUND) {
        return zb_storage_err;
    }
    if (zb_fct_err != ESP_OK && zb_fct_err != ESP_ERR_NOT_FOUND) {
        return zb_fct_err;
    }
    return ESP_OK;
}

esp_err_t settings_manager_get_last_factory_reset_report(settings_manager_factory_reset_report_t *out_report)
{
    if (!out_report) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = settings_lock();
    if (err != ESP_OK) {
        return err;
    }
    *out_report = s_last_factory_reset_report;
    settings_unlock();
    return ESP_OK;
}
