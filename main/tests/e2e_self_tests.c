#include "unity.h"
#include "api_contracts.h"
#include "api_usecases.h"
#include "wifi_service.h"
#include "wifi_init.h"
#include "gateway_state.h"
#include <stdlib.h>
#include <string.h>

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
static int s_mock_wifi_scan_called = 0;
static int s_mock_wifi_scan_free_called = 0;
static int s_mock_wifi_net_platform_init_called = 0;
static int s_mock_wifi_sta_connect_called = 0;
static int s_mock_wifi_fallback_called = 0;

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

static esp_err_t mock_wifi_scan_impl(wifi_ap_info_t **out_list, size_t *out_count)
{
    s_mock_wifi_scan_called++;
    if (!out_list || !out_count) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_count = 2;
    wifi_ap_info_t *list = (wifi_ap_info_t *)calloc(*out_count, sizeof(wifi_ap_info_t));
    TEST_ASSERT_NOT_NULL(list);
    strlcpy(list[0].ssid, "TestNet-A", sizeof(list[0].ssid));
    list[0].rssi = -48;
    list[0].auth = 3;
    strlcpy(list[1].ssid, "TestNet-B", sizeof(list[1].ssid));
    list[1].rssi = -71;
    list[1].auth = 0;
    *out_list = list;
    return ESP_OK;
}

static void mock_wifi_scan_free_impl(wifi_ap_info_t *list)
{
    s_mock_wifi_scan_free_called++;
    free(list);
}

static void mock_net_platform_services_init(void)
{
    s_mock_wifi_net_platform_init_called++;
}

static esp_err_t mock_wifi_sta_connect_and_wait_fail_after_retries(wifi_runtime_ctx_t *ctx)
{
    s_mock_wifi_sta_connect_called++;
    if (!ctx) {
        return ESP_ERR_INVALID_ARG;
    }
    ctx->sta_connected = false;
    ctx->fallback_ap_active = false;
    ctx->loaded_from_nvs = true;
    ctx->retry_num = 5;
    strlcpy(ctx->active_ssid, "HomeNet", sizeof(ctx->active_ssid));
    return ESP_FAIL;
}

static esp_err_t mock_wifi_start_fallback_ap_ok(wifi_runtime_ctx_t *ctx)
{
    s_mock_wifi_fallback_called++;
    if (!ctx) {
        return ESP_ERR_INVALID_ARG;
    }
    ctx->fallback_ap_active = true;
    ctx->sta_connected = false;
    strlcpy(ctx->active_ssid, "ZGW-Fallback", sizeof(ctx->active_ssid));
    return ESP_OK;
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
    s_mock_wifi_scan_called = 0;
    s_mock_wifi_scan_free_called = 0;
    s_mock_wifi_net_platform_init_called = 0;
    s_mock_wifi_sta_connect_called = 0;
    s_mock_wifi_fallback_called = 0;
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

static void test_e2e_control_contract_and_usecase(void)
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

static void test_e2e_endpoint_control_invalid_json_rejected(void)
{
    api_control_request_t req = {0};
    esp_err_t parse_ret = api_parse_control_json("{\"addr\":0,\"ep\":1,\"cmd\":1}", &req);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, parse_ret);

    parse_ret = api_parse_control_json("{\"addr\":18842,\"ep\":1,\"cmd\":3}", &req);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, parse_ret);
}

static void test_e2e_endpoint_control_service_error_propagates(void)
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

static void test_e2e_wifi_settings_contract_and_usecase(void)
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

static void test_e2e_endpoint_wifi_settings_invalid_json_rejected(void)
{
    api_wifi_save_request_t req = {0};
    esp_err_t parse_ret = api_parse_wifi_save_json("{\"ssid\":\"A\",\"password\":\"1234567\"}", &req);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, parse_ret);

    parse_ret = api_parse_wifi_save_json("{\"ssid\":\"\",\"password\":\"12345678\"}", &req);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, parse_ret);
}

static void test_e2e_endpoint_wifi_settings_reboot_failure_propagates(void)
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

static void test_e2e_factory_reset_usecase_mock(void)
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

static void test_e2e_endpoint_factory_reset_success(void)
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

static void test_e2e_wifi_scan_contract_and_usecase(void)
{
    reset_api_mocks();
    wifi_service_register_scan_impl(mock_wifi_scan_impl, mock_wifi_scan_free_impl);

    wifi_ap_info_t *list = NULL;
    size_t count = 0;
    esp_err_t ret = api_usecase_wifi_scan(&list, &count);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_EQUAL_INT(1, s_mock_wifi_scan_called);
    TEST_ASSERT_EQUAL_UINT32(2, (uint32_t)count);
    TEST_ASSERT_NOT_NULL(list);
    TEST_ASSERT_EQUAL_STRING("TestNet-A", list[0].ssid);
    TEST_ASSERT_EQUAL_INT(-48, list[0].rssi);
    TEST_ASSERT_EQUAL_INT(3, list[0].auth);
    TEST_ASSERT_EQUAL_STRING("TestNet-B", list[1].ssid);

    api_usecase_wifi_scan_free(list);
    TEST_ASSERT_EQUAL_INT(1, s_mock_wifi_scan_free_called);

    wifi_service_register_scan_impl(NULL, NULL);
}

static void test_e2e_wifi_connect_retry_exhausted_switches_to_ap_fallback(void)
{
    reset_api_mocks();
    wifi_init_ops_t ops = {
        .net_platform_services_init = mock_net_platform_services_init,
        .wifi_sta_connect_and_wait = mock_wifi_sta_connect_and_wait_fail_after_retries,
        .wifi_start_fallback_ap = mock_wifi_start_fallback_ap_ok,
    };
    wifi_init_set_ops_for_test(&ops);

    esp_err_t ret = wifi_init_sta_and_wait();
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_EQUAL_INT(1, s_mock_wifi_net_platform_init_called);
    TEST_ASSERT_EQUAL_INT(1, s_mock_wifi_sta_connect_called);
    TEST_ASSERT_EQUAL_INT(1, s_mock_wifi_fallback_called);

    gateway_wifi_state_t state = {0};
    TEST_ASSERT_EQUAL(ESP_OK, gateway_state_get_wifi(&state));
    TEST_ASSERT_FALSE(state.sta_connected);
    TEST_ASSERT_TRUE(state.fallback_ap_active);
    TEST_ASSERT_EQUAL_STRING("ZGW-Fallback", state.active_ssid);

    wifi_init_reset_ops_for_test();
}

void zgw_register_e2e_self_tests(void)
{
    RUN_TEST(test_e2e_control_contract_and_usecase);
    RUN_TEST(test_e2e_endpoint_control_invalid_json_rejected);
    RUN_TEST(test_e2e_endpoint_control_service_error_propagates);
    RUN_TEST(test_e2e_wifi_settings_contract_and_usecase);
    RUN_TEST(test_e2e_endpoint_wifi_settings_invalid_json_rejected);
    RUN_TEST(test_e2e_endpoint_wifi_settings_reboot_failure_propagates);
    RUN_TEST(test_e2e_wifi_scan_contract_and_usecase);
    RUN_TEST(test_e2e_wifi_connect_retry_exhausted_switches_to_ap_fallback);
    RUN_TEST(test_e2e_factory_reset_usecase_mock);
    RUN_TEST(test_e2e_endpoint_factory_reset_success);
}
