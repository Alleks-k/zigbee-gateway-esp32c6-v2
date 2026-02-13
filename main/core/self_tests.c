#include "self_tests.h"

#include "unity.h"
#include "api_handlers.h"
#include "api_contracts.h"
#include "api_usecases.h"
#include "device_manager.h"
#include "zigbee_service.h"
#include "wifi_service.h"
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

static int s_mock_send_on_off_called = 0;
static uint16_t s_mock_send_on_off_addr = 0;
static uint8_t s_mock_send_on_off_ep = 0;
static uint8_t s_mock_send_on_off_cmd = 0;
static int s_mock_wifi_save_called = 0;
static char s_mock_wifi_ssid[API_WIFI_SSID_MAX_LEN + 1];
static char s_mock_wifi_pass[API_WIFI_PASSWORD_MAX_LEN + 1];
static int s_mock_schedule_reboot_called = 0;
static uint32_t s_mock_schedule_reboot_delay = 0;
static int s_mock_factory_reset_called = 0;
static uint32_t s_mock_factory_reset_delay = 0;
static esp_err_t s_mock_wifi_save_ret = ESP_OK;
static esp_err_t s_mock_schedule_reboot_ret = ESP_OK;
static esp_err_t s_mock_factory_reset_ret = ESP_OK;

static esp_err_t mock_send_on_off(uint16_t short_addr, uint8_t endpoint, uint8_t on_off)
{
    s_mock_send_on_off_called++;
    s_mock_send_on_off_addr = short_addr;
    s_mock_send_on_off_ep = endpoint;
    s_mock_send_on_off_cmd = on_off;
    return ESP_OK;
}

static esp_err_t mock_wifi_save_credentials(const char *ssid, const char *password)
{
    s_mock_wifi_save_called++;
    strlcpy(s_mock_wifi_ssid, ssid ? ssid : "", sizeof(s_mock_wifi_ssid));
    strlcpy(s_mock_wifi_pass, password ? password : "", sizeof(s_mock_wifi_pass));
    return s_mock_wifi_save_ret;
}

static esp_err_t mock_schedule_reboot(uint32_t delay_ms)
{
    s_mock_schedule_reboot_called++;
    s_mock_schedule_reboot_delay = delay_ms;
    return s_mock_schedule_reboot_ret;
}

static esp_err_t mock_factory_reset_and_reboot(uint32_t reboot_delay_ms)
{
    s_mock_factory_reset_called++;
    s_mock_factory_reset_delay = reboot_delay_ms;
    return s_mock_factory_reset_ret;
}

static void reset_api_mocks(void)
{
    s_mock_send_on_off_called = 0;
    s_mock_send_on_off_addr = 0;
    s_mock_send_on_off_ep = 0;
    s_mock_send_on_off_cmd = 0;
    s_mock_wifi_save_called = 0;
    s_mock_wifi_ssid[0] = '\0';
    s_mock_wifi_pass[0] = '\0';
    s_mock_schedule_reboot_called = 0;
    s_mock_schedule_reboot_delay = 0;
    s_mock_factory_reset_called = 0;
    s_mock_factory_reset_delay = 0;
    s_mock_wifi_save_ret = ESP_OK;
    s_mock_schedule_reboot_ret = ESP_OK;
    s_mock_factory_reset_ret = ESP_OK;
}

static api_service_ops_t make_mock_ops(void)
{
    api_service_ops_t ops = {
        .send_on_off = mock_send_on_off,
        .wifi_save_credentials = mock_wifi_save_credentials,
        .schedule_reboot = mock_schedule_reboot,
        .factory_reset_and_reboot = mock_factory_reset_and_reboot,
    };
    return ops;
}

static void test_integration_control_contract_and_usecase(void)
{
    api_control_request_t req = {0};
    esp_err_t parse_ret = api_parse_control_json("{\"addr\":18842,\"ep\":1,\"cmd\":1}", &req);
    TEST_ASSERT_EQUAL(ESP_OK, parse_ret);
    TEST_ASSERT_EQUAL_UINT16(18842, req.addr);
    TEST_ASSERT_EQUAL_UINT8(1, req.ep);
    TEST_ASSERT_EQUAL_UINT8(1, req.cmd);

    reset_api_mocks();
    const api_service_ops_t mock_ops = make_mock_ops();
    api_usecases_set_service_ops(&mock_ops);

    esp_err_t ret = api_usecase_control(&req);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_EQUAL_INT(1, s_mock_send_on_off_called);
    TEST_ASSERT_EQUAL_UINT16(18842, s_mock_send_on_off_addr);
    TEST_ASSERT_EQUAL_UINT8(1, s_mock_send_on_off_ep);
    TEST_ASSERT_EQUAL_UINT8(1, s_mock_send_on_off_cmd);

    api_usecases_set_service_ops(NULL);
}

static void test_integration_endpoint_control_invalid_json_rejected(void)
{
    api_control_request_t req = {0};
    esp_err_t parse_ret = api_parse_control_json("{\"addr\":0,\"ep\":1,\"cmd\":1}", &req);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, parse_ret);

    parse_ret = api_parse_control_json("{\"addr\":18842,\"ep\":1,\"cmd\":3}", &req);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, parse_ret);
}

static void test_integration_endpoint_control_service_error_propagates(void)
{
    api_control_request_t req = {0};
    esp_err_t parse_ret = api_parse_control_json("{\"addr\":18842,\"ep\":1,\"cmd\":1}", &req);
    TEST_ASSERT_EQUAL(ESP_OK, parse_ret);

    reset_api_mocks();
    api_service_ops_t mock_ops = make_mock_ops();
    mock_ops.send_on_off = NULL;
    api_usecases_set_service_ops(&mock_ops);

    esp_err_t ret = api_usecase_control(&req);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

    api_usecases_set_service_ops(NULL);
}

static void test_integration_wifi_settings_contract_and_usecase(void)
{
    api_wifi_save_request_t req = {0};
    esp_err_t parse_ret = api_parse_wifi_save_json("{\"ssid\":\"TestNet\",\"password\":\"12345678\"}", &req);
    TEST_ASSERT_EQUAL(ESP_OK, parse_ret);
    TEST_ASSERT_EQUAL_STRING("TestNet", req.ssid);
    TEST_ASSERT_EQUAL_STRING("12345678", req.password);

    reset_api_mocks();
    const api_service_ops_t mock_ops = make_mock_ops();
    api_usecases_set_service_ops(&mock_ops);

    esp_err_t ret = api_usecase_wifi_save(&req);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_EQUAL_INT(1, s_mock_wifi_save_called);
    TEST_ASSERT_EQUAL_STRING("TestNet", s_mock_wifi_ssid);
    TEST_ASSERT_EQUAL_STRING("12345678", s_mock_wifi_pass);
    TEST_ASSERT_EQUAL_INT(1, s_mock_schedule_reboot_called);
    TEST_ASSERT_EQUAL_UINT32(1000, s_mock_schedule_reboot_delay);

    api_usecases_set_service_ops(NULL);
}

static void test_integration_endpoint_wifi_settings_invalid_json_rejected(void)
{
    api_wifi_save_request_t req = {0};
    esp_err_t parse_ret = api_parse_wifi_save_json("{\"ssid\":\"A\",\"password\":\"1234567\"}", &req);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, parse_ret);

    parse_ret = api_parse_wifi_save_json("{\"ssid\":\"\",\"password\":\"12345678\"}", &req);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, parse_ret);
}

static void test_integration_endpoint_wifi_settings_reboot_failure_propagates(void)
{
    api_wifi_save_request_t req = {0};
    esp_err_t parse_ret = api_parse_wifi_save_json("{\"ssid\":\"TestNet\",\"password\":\"12345678\"}", &req);
    TEST_ASSERT_EQUAL(ESP_OK, parse_ret);

    reset_api_mocks();
    s_mock_schedule_reboot_ret = ESP_FAIL;
    const api_service_ops_t mock_ops = make_mock_ops();
    api_usecases_set_service_ops(&mock_ops);

    esp_err_t ret = api_usecase_wifi_save(&req);
    TEST_ASSERT_EQUAL(ESP_FAIL, ret);
    TEST_ASSERT_EQUAL_INT(1, s_mock_wifi_save_called);
    TEST_ASSERT_EQUAL_INT(1, s_mock_schedule_reboot_called);

    api_usecases_set_service_ops(NULL);
}

static void test_integration_factory_reset_usecase_mock(void)
{
    reset_api_mocks();
    s_mock_factory_reset_ret = ESP_FAIL;
    const api_service_ops_t mock_ops = make_mock_ops();
    api_usecases_set_service_ops(&mock_ops);

    esp_err_t ret = api_usecase_factory_reset();
    TEST_ASSERT_EQUAL(ESP_FAIL, ret);
    TEST_ASSERT_EQUAL_INT(1, s_mock_factory_reset_called);
    TEST_ASSERT_EQUAL_UINT32(1000, s_mock_factory_reset_delay);

    api_usecases_set_service_ops(NULL);
}

static void test_integration_endpoint_factory_reset_success(void)
{
    reset_api_mocks();
    s_mock_factory_reset_ret = ESP_OK;
    const api_service_ops_t mock_ops = make_mock_ops();
    api_usecases_set_service_ops(&mock_ops);

    esp_err_t ret = api_usecase_factory_reset();
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_EQUAL_INT(1, s_mock_factory_reset_called);
    TEST_ASSERT_EQUAL_UINT32(1000, s_mock_factory_reset_delay);

    api_usecases_set_service_ops(NULL);
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
    RUN_TEST(test_integration_control_contract_and_usecase);
    RUN_TEST(test_integration_endpoint_control_invalid_json_rejected);
    RUN_TEST(test_integration_endpoint_control_service_error_propagates);
    RUN_TEST(test_integration_wifi_settings_contract_and_usecase);
    RUN_TEST(test_integration_endpoint_wifi_settings_invalid_json_rejected);
    RUN_TEST(test_integration_endpoint_wifi_settings_reboot_failure_propagates);
    RUN_TEST(test_integration_factory_reset_usecase_mock);
    RUN_TEST(test_integration_endpoint_factory_reset_success);
    int failures = UNITY_END();
    ESP_LOGW(TAG, "Self-tests complete, failures=%d", failures);
    return failures;
}
