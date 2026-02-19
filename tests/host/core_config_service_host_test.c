#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "config_service.h"
#include "config_repository.h"
#include "device_repository.h"
#include "storage_schema.h"
#include "storage_partitions.h"

typedef struct {
    esp_err_t schema_init_ret;
    esp_err_t schema_get_ret;
    esp_err_t schema_set_ret;
    int32_t schema_version;
    bool schema_found;
    int schema_set_calls;

    esp_err_t save_ret;
    int save_calls;
    char saved_ssid[GATEWAY_WIFI_SSID_MAX_LEN + 1];
    char saved_password[GATEWAY_WIFI_PASSWORD_MAX_LEN + 1];

    esp_err_t load_ret;
    bool load_found;
    char load_ssid[GATEWAY_WIFI_SSID_MAX_LEN + 1];
    char load_password[GATEWAY_WIFI_PASSWORD_MAX_LEN + 1];

    esp_err_t clear_wifi_ret;
    esp_err_t clear_devices_ret;
    esp_err_t erase_zb_storage_ret;
    esp_err_t erase_zb_fct_ret;
} settings_stub_t;

static settings_stub_t g_stub;

static void reset_stub(void)
{
    memset(&g_stub, 0, sizeof(g_stub));
    g_stub.schema_init_ret = ESP_OK;
    g_stub.schema_get_ret = ESP_OK;
    g_stub.schema_set_ret = ESP_OK;
    g_stub.schema_version = CONFIG_SERVICE_SCHEMA_VERSION_CURRENT;
    g_stub.schema_found = true;
    g_stub.save_ret = ESP_OK;
    g_stub.load_ret = ESP_OK;
    g_stub.load_found = false;
    g_stub.clear_wifi_ret = ESP_OK;
    g_stub.clear_devices_ret = ESP_OK;
    g_stub.erase_zb_storage_ret = ESP_OK;
    g_stub.erase_zb_fct_ret = ESP_OK;
}

esp_err_t storage_schema_init(void)
{
    return g_stub.schema_init_ret;
}

esp_err_t storage_schema_get_version(int32_t *out_version, bool *out_found)
{
    if (!out_version || !out_found) {
        return ESP_ERR_INVALID_ARG;
    }
    if (g_stub.schema_get_ret != ESP_OK) {
        return g_stub.schema_get_ret;
    }
    *out_version = g_stub.schema_version;
    *out_found = g_stub.schema_found;
    return ESP_OK;
}

esp_err_t storage_schema_set_version(int32_t version)
{
    g_stub.schema_set_calls++;
    g_stub.schema_version = version;
    g_stub.schema_found = true;
    if (g_stub.schema_set_ret != ESP_OK) {
        return g_stub.schema_set_ret;
    }
    return ESP_OK;
}

esp_err_t config_repository_load_wifi_credentials(char *ssid, size_t ssid_size,
                                                  char *password, size_t password_size,
                                                  bool *loaded)
{
    if (!ssid || !password || !loaded || ssid_size < 2 || password_size < 2) {
        return ESP_ERR_INVALID_ARG;
    }
    if (g_stub.load_ret != ESP_OK) {
        return g_stub.load_ret;
    }

    ssid[0] = '\0';
    password[0] = '\0';
    if (g_stub.load_found) {
        strncpy(ssid, g_stub.load_ssid, ssid_size - 1);
        ssid[ssid_size - 1] = '\0';
        strncpy(password, g_stub.load_password, password_size - 1);
        password[password_size - 1] = '\0';
    }
    *loaded = g_stub.load_found;
    return ESP_OK;
}

esp_err_t config_repository_save_wifi_credentials(const char *ssid, const char *password)
{
    g_stub.save_calls++;
    if (ssid) {
        strncpy(g_stub.saved_ssid, ssid, sizeof(g_stub.saved_ssid) - 1);
        g_stub.saved_ssid[sizeof(g_stub.saved_ssid) - 1] = '\0';
    }
    if (password) {
        strncpy(g_stub.saved_password, password, sizeof(g_stub.saved_password) - 1);
        g_stub.saved_password[sizeof(g_stub.saved_password) - 1] = '\0';
    }
    return g_stub.save_ret;
}

esp_err_t device_repository_load(gateway_device_record_t *devices, size_t max_devices,
                                 int *device_count, bool *loaded)
{
    (void)devices;
    (void)max_devices;
    (void)device_count;
    (void)loaded;
    return ESP_ERR_NOT_FOUND;
}

esp_err_t device_repository_save(const gateway_device_record_t *devices, size_t max_devices,
                                 int device_count)
{
    (void)devices;
    (void)max_devices;
    (void)device_count;
    return ESP_ERR_NOT_FOUND;
}

esp_err_t config_repository_clear_wifi_credentials(void)
{
    return g_stub.clear_wifi_ret;
}

esp_err_t device_repository_clear(void)
{
    return g_stub.clear_devices_ret;
}

esp_err_t storage_partitions_erase_zigbee_storage(void)
{
    return g_stub.erase_zb_storage_ret;
}

esp_err_t storage_partitions_erase_zigbee_factory(void)
{
    return g_stub.erase_zb_fct_ret;
}

static void test_validate_wifi_credentials_rules(void)
{
    assert(config_service_validate_wifi_credentials(NULL, "12345678") == GATEWAY_STATUS_INVALID_ARG);
    assert(config_service_validate_wifi_credentials("ssid", NULL) == GATEWAY_STATUS_INVALID_ARG);
    assert(config_service_validate_wifi_credentials("", "12345678") == GATEWAY_STATUS_INVALID_ARG);
    assert(config_service_validate_wifi_credentials("ssid", "1234567") == GATEWAY_STATUS_INVALID_ARG);
    assert(config_service_validate_wifi_credentials("ssid", "12345678") == GATEWAY_STATUS_OK);
}

static void test_save_wifi_credentials_delegates_only_when_valid(void)
{
    reset_stub();
    assert(config_service_save_wifi_credentials("HomeWiFi", "goodpass") == GATEWAY_STATUS_OK);
    assert(g_stub.save_calls == 1);
    assert(strcmp(g_stub.saved_ssid, "HomeWiFi") == 0);
    assert(strcmp(g_stub.saved_password, "goodpass") == 0);

    reset_stub();
    assert(config_service_save_wifi_credentials("HomeWiFi", "short") == GATEWAY_STATUS_INVALID_ARG);
    assert(g_stub.save_calls == 0);
}

static void test_load_wifi_credentials_sanitizes_invalid_storage_data(void)
{
    reset_stub();
    g_stub.load_found = true;
    strncpy(g_stub.load_ssid, "MySSID", sizeof(g_stub.load_ssid) - 1);
    strncpy(g_stub.load_password, "1234", sizeof(g_stub.load_password) - 1);

    char ssid[33];
    char password[65];
    bool loaded = true;
    assert(config_service_load_wifi_credentials(ssid, sizeof(ssid), password, sizeof(password), &loaded) == GATEWAY_STATUS_OK);
    assert(loaded == false);
    assert(ssid[0] == '\0');
    assert(password[0] == '\0');
}

static void test_load_wifi_credentials_passthrough_and_args(void)
{
    reset_stub();
    char ssid[33];
    char password[65];
    bool loaded = false;

    assert(config_service_load_wifi_credentials(NULL, sizeof(ssid), password, sizeof(password), &loaded) ==
           GATEWAY_STATUS_INVALID_ARG);

    g_stub.load_ret = ESP_FAIL;
    assert(config_service_load_wifi_credentials(ssid, sizeof(ssid), password, sizeof(password), &loaded) ==
           GATEWAY_STATUS_FAIL);

    reset_stub();
    g_stub.load_found = true;
    strncpy(g_stub.load_ssid, "Office", sizeof(g_stub.load_ssid) - 1);
    strncpy(g_stub.load_password, "supersecret", sizeof(g_stub.load_password) - 1);
    assert(config_service_load_wifi_credentials(ssid, sizeof(ssid), password, sizeof(password), &loaded) ==
           GATEWAY_STATUS_OK);
    assert(loaded == true);
    assert(strcmp(ssid, "Office") == 0);
    assert(strcmp(password, "supersecret") == 0);
}

static void test_schema_and_factory_report(void)
{
    reset_stub();
    g_stub.schema_found = false;
    g_stub.schema_version = 0;
    assert(config_service_init_or_migrate() == GATEWAY_STATUS_OK);
    assert(g_stub.schema_set_calls == 1);

    int32_t version = 0;
    assert(config_service_get_schema_version(&version) == GATEWAY_STATUS_OK);
    assert(version == CONFIG_SERVICE_SCHEMA_VERSION_CURRENT);
    assert(config_service_get_last_factory_reset_report(NULL) == GATEWAY_STATUS_INVALID_ARG);

    g_stub.clear_wifi_ret = ESP_OK;
    g_stub.clear_devices_ret = ESP_FAIL;
    g_stub.erase_zb_storage_ret = ESP_ERR_NOT_FOUND;
    g_stub.erase_zb_fct_ret = ESP_ERR_INVALID_SIZE;
    assert(config_service_factory_reset() == GATEWAY_STATUS_FAIL);

    config_service_factory_reset_report_t out = {0};
    assert(config_service_get_last_factory_reset_report(&out) == GATEWAY_STATUS_OK);
    assert(out.devices_err == GATEWAY_STATUS_FAIL);
    assert(out.zigbee_storage_err == GATEWAY_STATUS_NOT_FOUND);
}

int main(void)
{
    printf("Running host tests: core_config_service_host_test\n");
    test_validate_wifi_credentials_rules();
    test_save_wifi_credentials_delegates_only_when_valid();
    test_load_wifi_credentials_sanitizes_invalid_storage_data();
    test_load_wifi_credentials_passthrough_and_args();
    test_schema_and_factory_report();
    printf("Host tests passed: core_config_service_host_test\n");
    return 0;
}
