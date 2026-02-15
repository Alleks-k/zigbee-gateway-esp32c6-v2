#include "config_repository.h"

#include "storage_kv.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *NVS_NAMESPACE = "storage";
static SemaphoreHandle_t s_config_mutex = NULL;

static esp_err_t config_lock(void)
{
    if (s_config_mutex == NULL) {
        s_config_mutex = xSemaphoreCreateMutex();
        if (s_config_mutex == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }
    xSemaphoreTake(s_config_mutex, portMAX_DELAY);
    return ESP_OK;
}

static void config_unlock(void)
{
    if (s_config_mutex != NULL) {
        xSemaphoreGive(s_config_mutex);
    }
}

static esp_err_t erase_wifi_keys_locked(void)
{
    storage_kv_handle_t handle = NULL;
    esp_err_t err = storage_kv_open_readwrite(NVS_NAMESPACE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    err = storage_kv_erase_key(handle, "wifi_ssid", NULL);
    if (err == ESP_OK) {
        err = storage_kv_erase_key(handle, "wifi_pass", NULL);
    }
    if (err == ESP_OK) {
        err = storage_kv_commit(handle);
    }
    storage_kv_close(handle);
    return err;
}

esp_err_t config_repository_load_wifi_credentials(char *ssid, size_t ssid_size,
                                                  char *password, size_t password_size,
                                                  bool *loaded)
{
    if (!ssid || !password || !loaded || ssid_size < 2 || password_size < 2) {
        return ESP_ERR_INVALID_ARG;
    }

    *loaded = false;
    ssid[0] = '\0';
    password[0] = '\0';

    esp_err_t err = config_lock();
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

    config_unlock();
    return err;
}

esp_err_t config_repository_save_wifi_credentials(const char *ssid, const char *password)
{
    if (!ssid || !password) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = config_lock();
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

    config_unlock();
    return err;
}

esp_err_t config_repository_clear_wifi_credentials(void)
{
    esp_err_t err = config_lock();
    if (err != ESP_OK) {
        return err;
    }

    err = erase_wifi_keys_locked();
    config_unlock();
    return err;
}
