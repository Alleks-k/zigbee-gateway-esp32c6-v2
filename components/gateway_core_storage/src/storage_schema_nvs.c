#include "storage_schema.h"

#include "storage_kv.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *NVS_NAMESPACE = "storage";
static const char *KEY_SCHEMA_VER = "schema_ver";
static SemaphoreHandle_t s_schema_mutex = NULL;

static esp_err_t schema_lock(void)
{
    if (s_schema_mutex == NULL) {
        s_schema_mutex = xSemaphoreCreateMutex();
        if (s_schema_mutex == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }
    xSemaphoreTake(s_schema_mutex, portMAX_DELAY);
    return ESP_OK;
}

static void schema_unlock(void)
{
    if (s_schema_mutex != NULL) {
        xSemaphoreGive(s_schema_mutex);
    }
}

esp_err_t storage_schema_init(void)
{
    return storage_kv_init();
}

esp_err_t storage_schema_get_version(int32_t *out_version, bool *out_found)
{
    if (!out_version || !out_found) {
        return ESP_ERR_INVALID_ARG;
    }

    *out_version = 0;
    *out_found = false;

    esp_err_t err = schema_lock();
    if (err != ESP_OK) {
        return err;
    }

    storage_kv_handle_t handle = NULL;
    err = storage_kv_open_readonly(NVS_NAMESPACE, &handle);
    if (err == ESP_OK) {
        err = storage_kv_get_i32(handle, KEY_SCHEMA_VER, out_version, out_found);
        storage_kv_close(handle);
    } else if (err == ESP_ERR_NOT_FOUND) {
        err = ESP_OK;
    }

    schema_unlock();
    return err;
}

esp_err_t storage_schema_set_version(int32_t version)
{
    if (version < 0) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = schema_lock();
    if (err != ESP_OK) {
        return err;
    }

    storage_kv_handle_t handle = NULL;
    err = storage_kv_open_readwrite(NVS_NAMESPACE, &handle);
    if (err == ESP_OK) {
        err = storage_kv_set_i32(handle, KEY_SCHEMA_VER, version);
        if (err == ESP_OK) {
            err = storage_kv_commit(handle);
        }
        storage_kv_close(handle);
    }

    schema_unlock();
    return err;
}
