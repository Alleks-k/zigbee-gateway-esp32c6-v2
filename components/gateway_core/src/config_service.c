#include "config_service.h"
#include "config_repository.h"
#include "device_repository.h"
#include "storage_schema.h"
#include "storage_partitions.h"
#include "gateway_config_types.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "CONFIG_SERVICE";
static config_service_factory_reset_report_t s_last_factory_reset_report = {
    .wifi_err = ESP_FAIL,
    .devices_err = ESP_FAIL,
    .zigbee_storage_err = ESP_FAIL,
    .zigbee_fct_err = ESP_FAIL,
};

static esp_err_t config_schema_get_effective_version(int32_t *out_version)
{
    if (!out_version) {
        return ESP_ERR_INVALID_ARG;
    }

    bool found = false;
    int32_t version = 0;
    esp_err_t err = storage_schema_get_version(&version, &found);
    if (err != ESP_OK) {
        return err;
    }

    *out_version = found ? version : 0;
    return ESP_OK;
}

static esp_err_t config_schema_migrate_v0_to_v1(void)
{
    // v0 schema is key-compatible; v1 only introduces explicit schema version key.
    return ESP_OK;
}

esp_err_t config_service_init_or_migrate(void)
{
    esp_err_t err = storage_schema_init();
    if (err != ESP_OK) {
        return err;
    }

    int32_t version = 0;
    err = config_schema_get_effective_version(&version);
    if (err != ESP_OK) {
        return err;
    }

    if (version > CONFIG_SERVICE_SCHEMA_VERSION_CURRENT) {
        ESP_LOGE(TAG, "Unsupported settings schema version: %ld > %d",
                 (long)version, CONFIG_SERVICE_SCHEMA_VERSION_CURRENT);
        return ESP_ERR_INVALID_VERSION;
    }

    while (version < CONFIG_SERVICE_SCHEMA_VERSION_CURRENT) {
        int32_t next_version = version;
        switch (version) {
        case 0:
            err = config_schema_migrate_v0_to_v1();
            next_version = 1;
            break;
        default:
            ESP_LOGE(TAG, "Missing migration step from v%ld", (long)version);
            return ESP_ERR_NOT_SUPPORTED;
        }

        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Migration function failed: v%ld -> v%ld (%s)",
                     (long)version, (long)next_version, esp_err_to_name(err));
            return err;
        }

        err = storage_schema_set_version(next_version);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to persist schema version v%ld: %s",
                     (long)next_version, esp_err_to_name(err));
            return err;
        }

        ESP_LOGI(TAG, "Settings schema migrated: v%ld -> v%ld",
                 (long)version, (long)next_version);
        version = next_version;
    }

    ESP_LOGI(TAG, "Settings schema up-to-date: v%ld", (long)version);
    return ESP_OK;
}

esp_err_t config_service_get_schema_version(int32_t *out_version)
{
    return config_schema_get_effective_version(out_version);
}

esp_err_t config_service_validate_wifi_credentials(const char *ssid, const char *password)
{
    if (!ssid || !password) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t ssid_len = strlen(ssid);
    size_t pass_len = strlen(password);
    if (ssid_len == 0 || ssid_len > GATEWAY_WIFI_SSID_MAX_LEN) {
        return ESP_ERR_INVALID_ARG;
    }
    if (pass_len < 8 || pass_len > GATEWAY_WIFI_PASSWORD_MAX_LEN) {
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

esp_err_t config_service_save_wifi_credentials(const char *ssid, const char *password)
{
    esp_err_t err = config_service_validate_wifi_credentials(ssid, password);
    if (err != ESP_OK) {
        return err;
    }

    return config_repository_save_wifi_credentials(ssid, password);
}

esp_err_t config_service_load_wifi_credentials(char *ssid, size_t ssid_size,
                                               char *password, size_t password_size,
                                               bool *loaded_from_storage)
{
    if (!ssid || !password || !loaded_from_storage || ssid_size < 2 || password_size < 2) {
        return ESP_ERR_INVALID_ARG;
    }

    *loaded_from_storage = false;
    ssid[0] = '\0';
    password[0] = '\0';

    esp_err_t err = config_repository_load_wifi_credentials(
        ssid, ssid_size, password, password_size, loaded_from_storage);
    if (err != ESP_OK || !(*loaded_from_storage)) {
        return err;
    }

    err = config_service_validate_wifi_credentials(ssid, password);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Ignoring invalid Wi-Fi credentials loaded from storage");
        ssid[0] = '\0';
        password[0] = '\0';
        *loaded_from_storage = false;
        return ESP_OK;
    }

    return ESP_OK;
}

esp_err_t config_service_factory_reset(void)
{
    esp_err_t wifi_err = config_repository_clear_wifi_credentials();
    esp_err_t devices_err = device_repository_clear();
    esp_err_t zb_storage_err = storage_partitions_erase_zigbee_storage();
    esp_err_t zb_fct_err = storage_partitions_erase_zigbee_factory();

    ESP_LOGI(TAG, "Factory reset result: wifi=%s, devices=%s, zigbee_storage=%s, zigbee_fct=%s",
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

esp_err_t config_service_get_last_factory_reset_report(config_service_factory_reset_report_t *out_report)
{
    if (!out_report) {
        return ESP_ERR_INVALID_ARG;
    }

    *out_report = s_last_factory_reset_report;
    return ESP_OK;
}
