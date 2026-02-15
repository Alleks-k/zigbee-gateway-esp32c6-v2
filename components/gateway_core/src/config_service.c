#include "config_service.h"
#include "storage_settings.h"
#include "gateway_config_types.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "CONFIG_SERVICE";

esp_err_t config_service_init_or_migrate(void)
{
    return settings_manager_init_or_migrate();
}

esp_err_t config_service_get_schema_version(int32_t *out_version)
{
    return settings_manager_get_schema_version(out_version);
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

    return settings_manager_save_wifi_credentials(ssid, password);
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

    esp_err_t err = settings_manager_load_wifi_credentials(
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
    return settings_manager_factory_reset();
}

esp_err_t config_service_get_last_factory_reset_report(config_service_factory_reset_report_t *out_report)
{
    if (!out_report) {
        return ESP_ERR_INVALID_ARG;
    }

    return settings_manager_get_last_factory_reset_report(out_report);
}
