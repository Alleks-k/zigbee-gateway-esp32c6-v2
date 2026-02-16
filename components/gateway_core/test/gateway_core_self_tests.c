#include "unity.h"
#include "device_service.h"
#include "zigbee_service.h"
#include "config_service.h"

static void test_device_snapshot_null_buffer(void)
{
    device_service_handle_t device_service = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, device_service_get_default(&device_service));
    TEST_ASSERT_EQUAL_INT(0, device_service_get_snapshot(device_service, NULL, 0));
}

static void test_service_rename_device_rejects_null_name(void)
{
    esp_err_t ret = zigbee_service_rename_device(0x1234, NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
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
    RUN_TEST(test_settings_schema_migration_smoke);
}
