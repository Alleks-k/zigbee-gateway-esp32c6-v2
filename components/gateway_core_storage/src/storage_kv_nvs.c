#include "storage_kv.h"
#include "nvs.h"
#include "esp_partition.h"
#include <stdlib.h>

struct storage_kv_handle_s {
    nvs_handle_t nvs;
};

static esp_err_t storage_kv_open_internal(const char *ns, nvs_open_mode_t mode, storage_kv_handle_t *out_handle)
{
    if (!ns || !out_handle) {
        return ESP_ERR_INVALID_ARG;
    }

    *out_handle = NULL;
    nvs_handle_t nvs = 0;
    esp_err_t err = nvs_open(ns, mode, &nvs);
    if (err != ESP_OK) {
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            return ESP_ERR_NOT_FOUND;
        }
        return err;
    }

    storage_kv_handle_t handle = calloc(1, sizeof(*handle));
    if (!handle) {
        nvs_close(nvs);
        return ESP_ERR_NO_MEM;
    }
    handle->nvs = nvs;
    *out_handle = handle;
    return ESP_OK;
}

esp_err_t storage_kv_init(void)
{
    return ESP_OK;
}

esp_err_t storage_kv_open_readonly(const char *ns, storage_kv_handle_t *out_handle)
{
    return storage_kv_open_internal(ns, NVS_READONLY, out_handle);
}

esp_err_t storage_kv_open_readwrite(const char *ns, storage_kv_handle_t *out_handle)
{
    return storage_kv_open_internal(ns, NVS_READWRITE, out_handle);
}

void storage_kv_close(storage_kv_handle_t handle)
{
    if (!handle) {
        return;
    }
    nvs_close(handle->nvs);
    free(handle);
}

esp_err_t storage_kv_commit(storage_kv_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    return nvs_commit(handle->nvs);
}

esp_err_t storage_kv_get_i32(storage_kv_handle_t handle, const char *key, int32_t *out_value, bool *out_found)
{
    if (!handle || !key || !out_value) {
        return ESP_ERR_INVALID_ARG;
    }

    if (out_found) {
        *out_found = false;
    }
    esp_err_t err = nvs_get_i32(handle->nvs, key, out_value);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_OK;
    }
    if (err == ESP_OK && out_found) {
        *out_found = true;
    }
    return err;
}

esp_err_t storage_kv_set_i32(storage_kv_handle_t handle, const char *key, int32_t value)
{
    if (!handle || !key) {
        return ESP_ERR_INVALID_ARG;
    }
    return nvs_set_i32(handle->nvs, key, value);
}

esp_err_t storage_kv_get_u32(storage_kv_handle_t handle, const char *key, uint32_t *out_value, bool *out_found)
{
    if (!handle || !key || !out_value) {
        return ESP_ERR_INVALID_ARG;
    }

    if (out_found) {
        *out_found = false;
    }
    esp_err_t err = nvs_get_u32(handle->nvs, key, out_value);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_OK;
    }
    if (err == ESP_OK && out_found) {
        *out_found = true;
    }
    return err;
}

esp_err_t storage_kv_set_u32(storage_kv_handle_t handle, const char *key, uint32_t value)
{
    if (!handle || !key) {
        return ESP_ERR_INVALID_ARG;
    }
    return nvs_set_u32(handle->nvs, key, value);
}

esp_err_t storage_kv_get_str(storage_kv_handle_t handle, const char *key, char *out_value, size_t out_size,
                             bool *out_found)
{
    if (!handle || !key || !out_value || out_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (out_found) {
        *out_found = false;
    }
    size_t size = out_size;
    esp_err_t err = nvs_get_str(handle->nvs, key, out_value, &size);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        out_value[0] = '\0';
        return ESP_OK;
    }
    if (err == ESP_OK && out_found) {
        *out_found = true;
    }
    return err;
}

esp_err_t storage_kv_set_str(storage_kv_handle_t handle, const char *key, const char *value)
{
    if (!handle || !key || !value) {
        return ESP_ERR_INVALID_ARG;
    }
    return nvs_set_str(handle->nvs, key, value);
}

esp_err_t storage_kv_get_blob(storage_kv_handle_t handle, const char *key, void *out_value, size_t out_size,
                              size_t *out_len, bool *out_found)
{
    if (!handle || !key || !out_value || out_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (out_found) {
        *out_found = false;
    }
    if (out_len) {
        *out_len = 0;
    }
    size_t size = out_size;
    esp_err_t err = nvs_get_blob(handle->nvs, key, out_value, &size);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_OK;
    }
    if (err == ESP_OK) {
        if (out_found) {
            *out_found = true;
        }
        if (out_len) {
            *out_len = size;
        }
    }
    return err;
}

esp_err_t storage_kv_set_blob(storage_kv_handle_t handle, const char *key, const void *value, size_t value_len)
{
    if (!handle || !key || !value || value_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    return nvs_set_blob(handle->nvs, key, value, value_len);
}

esp_err_t storage_kv_erase_key(storage_kv_handle_t handle, const char *key, bool *out_existed)
{
    if (!handle || !key) {
        return ESP_ERR_INVALID_ARG;
    }

    if (out_existed) {
        *out_existed = false;
    }
    esp_err_t err = nvs_erase_key(handle->nvs, key);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_OK;
    }
    if (err == ESP_OK && out_existed) {
        *out_existed = true;
    }
    return err;
}

esp_err_t storage_kv_erase_partition(const char *partition_label, bool *out_found)
{
    if (!partition_label || partition_label[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }
    if (out_found) {
        *out_found = false;
    }

    const esp_partition_t *part =
        esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, partition_label);
    if (!part) {
        return ESP_ERR_NOT_FOUND;
    }
    if (out_found) {
        *out_found = true;
    }
    return esp_partition_erase_range(part, 0, part->size);
}
