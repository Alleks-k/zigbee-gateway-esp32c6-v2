#include "self_tests.h"

#include "unity.h"
#include "api_handlers.h"
#include "device_manager.h"
#include "zgw_service.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "SELF_TESTS";

static void test_device_snapshot_null_buffer(void)
{
    TEST_ASSERT_EQUAL_INT(0, device_manager_get_snapshot(NULL, 0));
}

static void test_devices_json_builder_small_buffer_fails(void)
{
    char buf[16];
    size_t out_len = 0;
    esp_err_t ret = build_devices_json_compact(buf, sizeof(buf), &out_len);
    TEST_ASSERT_EQUAL(ESP_ERR_NO_MEM, ret);
}

static void test_status_json_builder_small_buffer_fails(void)
{
    char buf[24];
    size_t out_len = 0;
    esp_err_t ret = build_status_json_compact(buf, sizeof(buf), &out_len);
    TEST_ASSERT_EQUAL(ESP_ERR_NO_MEM, ret);
}

static void test_devices_json_builder_ok(void)
{
    char buf[1024];
    size_t out_len = 0;
    esp_err_t ret = build_devices_json_compact(buf, sizeof(buf), &out_len);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_GREATER_THAN_UINT32(0, (uint32_t)out_len);
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"devices\""));
}

static void test_status_json_builder_ok(void)
{
    char buf[1536];
    size_t out_len = 0;
    esp_err_t ret = build_status_json_compact(buf, sizeof(buf), &out_len);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_GREATER_THAN_UINT32(0, (uint32_t)out_len);
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"pan_id\""));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"channel\""));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"short_addr\""));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"devices\""));
}

static void test_service_rename_device_rejects_null_name(void)
{
    esp_err_t ret = zgw_service_rename_device(0x1234, NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

static void test_service_wifi_save_rejects_invalid_input(void)
{
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, zgw_service_wifi_save_credentials(NULL, "12345678"));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, zgw_service_wifi_save_credentials("ssid", NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, zgw_service_wifi_save_credentials("", "12345678"));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, zgw_service_wifi_save_credentials("ssid", "1234567"));
}

int zgw_run_self_tests(void)
{
    ESP_LOGW(TAG, "Running gateway self-tests");
    UNITY_BEGIN();
    RUN_TEST(test_device_snapshot_null_buffer);
    RUN_TEST(test_devices_json_builder_small_buffer_fails);
    RUN_TEST(test_status_json_builder_small_buffer_fails);
    RUN_TEST(test_devices_json_builder_ok);
    RUN_TEST(test_status_json_builder_ok);
    RUN_TEST(test_service_rename_device_rejects_null_name);
    RUN_TEST(test_service_wifi_save_rejects_invalid_input);
    int failures = UNITY_END();
    ESP_LOGW(TAG, "Self-tests complete, failures=%d", failures);
    return failures;
}
