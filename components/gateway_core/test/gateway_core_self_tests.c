#include "unity.h"
#include "device_service.h"
#include "gateway_status.h"
#include "zigbee_service.h"
#include "config_service.h"
#include "gateway_persistence_adapter.h"
#include "device_service_lock_freertos_port.h"

static gateway_status_t core_selftest_device_repo_load(void *ctx,
                                                       gateway_device_record_t *devices,
                                                       size_t max_devices,
                                                       int *device_count,
                                                       bool *loaded)
{
    (void)ctx;
    return gateway_persistence_devices_load(devices, max_devices, device_count, loaded);
}

static gateway_status_t core_selftest_device_repo_save(void *ctx,
                                                       const gateway_device_record_t *devices,
                                                       size_t max_devices,
                                                       int device_count)
{
    (void)ctx;
    return gateway_persistence_devices_save(devices, max_devices, device_count);
}

static const device_service_repo_port_t s_core_selftest_device_repo_port = {
    .load = core_selftest_device_repo_load,
    .save = core_selftest_device_repo_save,
    .ctx = NULL,
};

static void test_device_snapshot_null_buffer(void)
{
    device_service_handle_t device_service = NULL;
    device_service_init_params_t params = {
        .lock_port = device_service_lock_port_freertos(),
        .repo_port = &s_core_selftest_device_repo_port,
        .notifier = NULL,
    };
    TEST_ASSERT_EQUAL(GATEWAY_STATUS_OK, device_service_create_with_params(&params, &device_service));
    TEST_ASSERT_EQUAL(GATEWAY_STATUS_OK, device_service_init(device_service));
    TEST_ASSERT_EQUAL_INT(0, device_service_get_snapshot(device_service, NULL, 0));
    device_service_destroy(device_service);
}

static void test_service_rename_device_rejects_null_name(void)
{
    esp_err_t ret = zigbee_service_rename_device(NULL, 0x1234, NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

static void test_settings_schema_migration_smoke(void)
{
    TEST_ASSERT_EQUAL(GATEWAY_STATUS_OK, config_service_init_or_migrate());
    int32_t ver = 0;
    TEST_ASSERT_EQUAL(GATEWAY_STATUS_OK, config_service_get_schema_version(&ver));
    TEST_ASSERT_EQUAL_INT(CONFIG_SERVICE_SCHEMA_VERSION_CURRENT, ver);

    TEST_ASSERT_EQUAL(GATEWAY_STATUS_OK, config_service_init_or_migrate());
    TEST_ASSERT_EQUAL(GATEWAY_STATUS_OK, config_service_get_schema_version(&ver));
    TEST_ASSERT_EQUAL_INT(CONFIG_SERVICE_SCHEMA_VERSION_CURRENT, ver);
}

void gateway_core_register_self_tests(void)
{
    RUN_TEST(test_device_snapshot_null_buffer);
    RUN_TEST(test_service_rename_device_rejects_null_name);
    RUN_TEST(test_settings_schema_migration_smoke);
}
