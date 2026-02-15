#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "config_service.h"
#include "config_repository.h"
#include "device_repository.h"
#include "storage_partitions.h"
#include "storage_schema.h"

typedef struct {
    esp_err_t clear_wifi_ret;
    esp_err_t clear_devices_ret;
    esp_err_t erase_zb_storage_ret;
    esp_err_t erase_zb_fct_ret;
} factory_reset_stub_t;

static factory_reset_stub_t g_stub;

static void reset_stub(void)
{
    memset(&g_stub, 0, sizeof(g_stub));
    g_stub.clear_wifi_ret = ESP_OK;
    g_stub.clear_devices_ret = ESP_OK;
    g_stub.erase_zb_storage_ret = ESP_OK;
    g_stub.erase_zb_fct_ret = ESP_OK;
}

esp_err_t storage_schema_init(void)
{
    return ESP_OK;
}

esp_err_t storage_schema_get_version(int32_t *out_version, bool *out_found)
{
    if (!out_version || !out_found) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_version = CONFIG_SERVICE_SCHEMA_VERSION_CURRENT;
    *out_found = true;
    return ESP_OK;
}

esp_err_t storage_schema_set_version(int32_t version)
{
    (void)version;
    return ESP_OK;
}

esp_err_t config_repository_load_wifi_credentials(char *ssid, size_t ssid_size,
                                                  char *password, size_t password_size,
                                                  bool *loaded)
{
    (void)ssid;
    (void)ssid_size;
    (void)password;
    (void)password_size;
    if (!loaded) {
        return ESP_ERR_INVALID_ARG;
    }
    *loaded = false;
    return ESP_OK;
}

esp_err_t config_repository_save_wifi_credentials(const char *ssid, const char *password)
{
    (void)ssid;
    (void)password;
    return ESP_OK;
}

esp_err_t device_repository_load(gateway_device_record_t *devices, size_t max_devices,
                                 int *device_count, bool *loaded)
{
    (void)devices;
    (void)max_devices;
    if (!device_count || !loaded) {
        return ESP_ERR_INVALID_ARG;
    }
    *device_count = 0;
    *loaded = false;
    return ESP_OK;
}

esp_err_t device_repository_save(const gateway_device_record_t *devices, size_t max_devices,
                                 int device_count)
{
    (void)devices;
    (void)max_devices;
    (void)device_count;
    return ESP_OK;
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

static void assert_last_report(esp_err_t wifi_err, esp_err_t devices_err,
                               esp_err_t zb_storage_err, esp_err_t zb_fct_err)
{
    config_service_factory_reset_report_t out = {0};
    assert(config_service_get_last_factory_reset_report(&out) == ESP_OK);
    assert(out.wifi_err == wifi_err);
    assert(out.devices_err == devices_err);
    assert(out.zigbee_storage_err == zb_storage_err);
    assert(out.zigbee_fct_err == zb_fct_err);
}

static void test_factory_reset_error_priority(void)
{
    reset_stub();
    g_stub.clear_wifi_ret = ESP_FAIL;
    assert(config_service_factory_reset() == ESP_FAIL);
    assert_last_report(ESP_FAIL, ESP_OK, ESP_OK, ESP_OK);

    reset_stub();
    g_stub.clear_devices_ret = ESP_ERR_INVALID_ARG;
    assert(config_service_factory_reset() == ESP_ERR_INVALID_ARG);
    assert_last_report(ESP_OK, ESP_ERR_INVALID_ARG, ESP_OK, ESP_OK);

    reset_stub();
    g_stub.erase_zb_storage_ret = ESP_ERR_NOT_FOUND;
    g_stub.erase_zb_fct_ret = ESP_ERR_INVALID_SIZE;
    assert(config_service_factory_reset() == ESP_ERR_INVALID_SIZE);
    assert_last_report(ESP_OK, ESP_OK, ESP_ERR_NOT_FOUND, ESP_ERR_INVALID_SIZE);
}

static void test_factory_reset_tolerates_missing_zigbee_partitions(void)
{
    reset_stub();
    g_stub.erase_zb_storage_ret = ESP_ERR_NOT_FOUND;
    g_stub.erase_zb_fct_ret = ESP_ERR_NOT_FOUND;
    assert(config_service_factory_reset() == ESP_OK);
    assert_last_report(ESP_OK, ESP_OK, ESP_ERR_NOT_FOUND, ESP_ERR_NOT_FOUND);
}

int main(void)
{
    printf("Running host tests: factory_reset_policy_host_test\n");
    assert(config_service_get_last_factory_reset_report(NULL) == ESP_ERR_INVALID_ARG);
    test_factory_reset_error_priority();
    test_factory_reset_tolerates_missing_zigbee_partitions();
    printf("Host tests passed: factory_reset_policy_host_test\n");
    return 0;
}
