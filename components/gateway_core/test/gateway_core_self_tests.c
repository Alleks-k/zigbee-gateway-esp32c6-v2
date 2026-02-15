#include "unity.h"
#include "device_manager.h"
#include "zigbee_service.h"
#include "wifi_service.h"
#include "config_service.h"

static void test_device_snapshot_null_buffer(void)
{
    TEST_ASSERT_EQUAL_INT(0, device_manager_get_snapshot(NULL, 0));
}

static void test_service_rename_device_rejects_null_name(void)
{
    esp_err_t ret = zigbee_service_rename_device(0x1234, NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

static void test_service_wifi_save_rejects_invalid_input(void)
{
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, wifi_service_save_credentials(NULL, "12345678"));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, wifi_service_save_credentials("ssid", NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, wifi_service_save_credentials("", "12345678"));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, wifi_service_save_credentials("ssid", "1234567"));
}

static void test_settings_schema_migration_smoke(void)
{
    TEST_ASSERT_EQUAL(ESP_OK, config_service_init_or_migrate());
    int32_t ver = 0;
    TEST_ASSERT_EQUAL(ESP_OK, config_service_get_schema_version(&ver));
    TEST_ASSERT_EQUAL_INT(CONFIG_SERVICE_SCHEMA_VERSION_CURRENT, ver);

    TEST_ASSERT_EQUAL(ESP_OK, config_service_init_or_migrate());
    TEST_ASSERT_EQUAL(ESP_OK, config_service_get_schema_version(&ver));
    TEST_ASSERT_EQUAL_INT(CONFIG_SERVICE_SCHEMA_VERSION_CURRENT, ver);
}

void gateway_core_register_self_tests(void)
{
    RUN_TEST(test_device_snapshot_null_buffer);
    RUN_TEST(test_service_rename_device_rejects_null_name);
    RUN_TEST(test_service_wifi_save_rejects_invalid_input);
    RUN_TEST(test_settings_schema_migration_smoke);
}
