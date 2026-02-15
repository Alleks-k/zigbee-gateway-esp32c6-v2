#include "storage_settings.h"
#include "storage_kv.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "SETTINGS_MANAGER";
static const char *NVS_NAMESPACE = "storage";
static const char *KEY_SCHEMA_VER = "schema_ver";
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

typedef esp_err_t (*settings_migration_fn_t)(storage_kv_handle_t handle);

typedef struct {
    int32_t from_version;
    int32_t to_version;
    settings_migration_fn_t migrate;
} settings_migration_step_t;

static esp_err_t migrate_v0_to_v1(storage_kv_handle_t handle)
{
    (void)handle;
    // v0 schema had no explicit version key; data keys are compatible with v1.
    return ESP_OK;
}

static const settings_migration_step_t s_migration_steps[] = {
    {0, 1, migrate_v0_to_v1},
};

static esp_err_t settings_get_schema_version_locked(storage_kv_handle_t handle, int32_t *out_version)
{
    bool found = false;
    esp_err_t err = storage_kv_get_i32(handle, KEY_SCHEMA_VER, out_version, &found);
    if (err != ESP_OK) {
        return err;
    }
    if (!found) {
        *out_version = 0; // Legacy schema: keys exist without explicit version.
    }
    return ESP_OK;
}

static const settings_migration_step_t *settings_find_migration_step(int32_t from_version)
{
    for (size_t i = 0; i < sizeof(s_migration_steps) / sizeof(s_migration_steps[0]); i++) {
        if (s_migration_steps[i].from_version == from_version) {
            return &s_migration_steps[i];
        }
    }
    return NULL;
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

    storage_kv_handle_t handle = NULL;
    err = storage_kv_open_readonly(NVS_NAMESPACE, &handle);
    if (err == ESP_OK) {
        err = settings_get_schema_version_locked(handle, out_version);
        storage_kv_close(handle);
    } else if (err == ESP_ERR_NOT_FOUND) {
        *out_version = 0;
        err = ESP_OK;
    }

    settings_unlock();
    return err;
}

esp_err_t settings_manager_init_or_migrate(void)
{
    esp_err_t err = storage_kv_init();
    if (err != ESP_OK) {
        return err;
    }

    err = settings_lock();
    if (err != ESP_OK) {
        return err;
    }

    storage_kv_handle_t handle = NULL;
    err = storage_kv_open_readwrite(NVS_NAMESPACE, &handle);
    if (err != ESP_OK) {
        settings_unlock();
        return err;
    }

    int32_t version = 0;
    err = settings_get_schema_version_locked(handle, &version);
    if (err != ESP_OK) {
        storage_kv_close(handle);
        settings_unlock();
        return err;
    }

    if (version > SETTINGS_SCHEMA_VERSION_CURRENT) {
        ESP_LOGE(TAG, "Unsupported settings schema version: %ld > %d",
                 (long)version, SETTINGS_SCHEMA_VERSION_CURRENT);
        storage_kv_close(handle);
        settings_unlock();
        return ESP_ERR_INVALID_VERSION;
    }

    if (version == SETTINGS_SCHEMA_VERSION_CURRENT) {
        ESP_LOGI(TAG, "Settings schema up-to-date: v%d", SETTINGS_SCHEMA_VERSION_CURRENT);
    } else {
        while (version < SETTINGS_SCHEMA_VERSION_CURRENT) {
            const settings_migration_step_t *step = settings_find_migration_step(version);
            if (!step || !step->migrate || step->to_version <= step->from_version) {
                storage_kv_close(handle);
                settings_unlock();
                ESP_LOGE(TAG, "Missing migration step from v%ld", (long)version);
                return ESP_ERR_NOT_SUPPORTED;
            }

            err = step->migrate(handle);
            if (err != ESP_OK) {
                storage_kv_close(handle);
                settings_unlock();
                ESP_LOGE(TAG, "Migration function failed: v%ld -> v%ld (%s)",
                         (long)step->from_version, (long)step->to_version, esp_err_to_name(err));
                return err;
            }

            err = storage_kv_set_i32(handle, KEY_SCHEMA_VER, step->to_version);
            if (err == ESP_OK) {
                err = storage_kv_commit(handle);
            }
            if (err != ESP_OK) {
                storage_kv_close(handle);
                settings_unlock();
                ESP_LOGE(TAG, "Failed to persist schema version v%ld: %s",
                         (long)step->to_version, esp_err_to_name(err));
                return err;
            }

            ESP_LOGI(TAG, "Settings schema migrated: v%ld -> v%ld",
                     (long)step->from_version, (long)step->to_version);
            version = step->to_version;
        }
    }

    storage_kv_close(handle);
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

    storage_kv_handle_t handle = NULL;
    err = storage_kv_open_readonly(NVS_NAMESPACE, &handle);
    if (err == ESP_OK) {
        bool ssid_found = false;
        bool pass_found = false;
        esp_err_t ssid_err = storage_kv_get_str(handle, "wifi_ssid", ssid, ssid_size, &ssid_found);
        esp_err_t pass_err = storage_kv_get_str(handle, "wifi_pass", password, password_size, &pass_found);
        if (ssid_err == ESP_OK && pass_err == ESP_OK && ssid_found && pass_found) {
            *loaded = true;
        }
        storage_kv_close(handle);
        err = ESP_OK;
    } else if (err == ESP_ERR_NOT_FOUND) {
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

    esp_err_t err = settings_lock();
    if (err != ESP_OK) {
        return err;
    }

    storage_kv_handle_t handle = NULL;
    err = storage_kv_open_readwrite(NVS_NAMESPACE, &handle);
    if (err == ESP_OK) {
        err = storage_kv_set_str(handle, "wifi_ssid", ssid);
        if (err == ESP_OK) {
            err = storage_kv_set_str(handle, "wifi_pass", password);
        }
        if (err == ESP_OK) {
            err = storage_kv_commit(handle);
        }
        storage_kv_close(handle);
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
            if (storage_kv_get_blob(handle, "dev_list", devices, sizeof(zb_device_t) * max_devices, &out_len,
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

    storage_kv_handle_t handle = NULL;
    err = storage_kv_open_readwrite(NVS_NAMESPACE, &handle);
    if (err == ESP_OK) {
        err = storage_kv_set_i32(handle, "dev_count", device_count);
        if (err == ESP_OK) {
            err = storage_kv_set_blob(handle, "dev_list", devices, sizeof(zb_device_t) * max_devices);
        }
        if (err == ESP_OK) {
            err = storage_kv_commit(handle);
        }
        storage_kv_close(handle);
    }

    settings_unlock();
    return err;
}

static esp_err_t erase_keys_with_commit_locked(const char *key_a, const char *key_b)
{
    if (!key_a || !key_b) {
        return ESP_ERR_INVALID_ARG;
    }

    storage_kv_handle_t handle = NULL;
    esp_err_t err = storage_kv_open_readwrite(NVS_NAMESPACE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    err = storage_kv_erase_key(handle, key_a, NULL);
    if (err == ESP_OK) {
        err = storage_kv_erase_key(handle, key_b, NULL);
    }
    if (err == ESP_OK) {
        err = storage_kv_commit(handle);
    }
    storage_kv_close(handle);
    return err;
}

esp_err_t settings_manager_clear_wifi_credentials(void)
{
    esp_err_t err = settings_lock();
    if (err != ESP_OK) {
        return err;
    }
    err = erase_keys_with_commit_locked("wifi_ssid", "wifi_pass");
    settings_unlock();
    return err;
}

esp_err_t settings_manager_clear_devices(void)
{
    esp_err_t err = settings_lock();
    if (err != ESP_OK) {
        return err;
    }
    err = erase_keys_with_commit_locked("dev_count", "dev_list");
    settings_unlock();
    return err;
}

esp_err_t settings_manager_erase_zigbee_storage_partition(void)
{
    esp_err_t err = settings_lock();
    if (err != ESP_OK) {
        return err;
    }
    err = storage_kv_erase_partition("zb_storage", NULL);
    settings_unlock();
    return err;
}

esp_err_t settings_manager_erase_zigbee_factory_partition(void)
{
    esp_err_t err = settings_lock();
    if (err != ESP_OK) {
        return err;
    }
    err = storage_kv_erase_partition("zb_fct", NULL);
    settings_unlock();
    return err;
}
