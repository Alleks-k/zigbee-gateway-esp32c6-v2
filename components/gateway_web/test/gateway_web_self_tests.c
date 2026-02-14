#include "unity.h"
#include "api_handlers.h"
#include "api_contracts.h"
#include "api_usecases.h"
#include "error_ring.h"
#include "cJSON.h"
#include <stdio.h>
#include <string.h>

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

static void test_health_json_builder_with_large_error_ring_truncates_and_stays_valid(void)
{
    for (int i = 0; i < 10; i++) {
        char msg[48];
        snprintf(msg, sizeof(msg), "selftest error %d", i);
        gateway_error_ring_add("api", -100 - i, msg);
    }

    char buf[4096];
    size_t out_len = 0;
    esp_err_t ret = build_health_json_compact(buf, sizeof(buf), &out_len);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_GREATER_THAN_UINT32(0, (uint32_t)out_len);

    cJSON *root = cJSON_ParseWithLength(buf, out_len);
    TEST_ASSERT_NOT_NULL(root);

    cJSON *errors = cJSON_GetObjectItem(root, "errors");
    TEST_ASSERT_TRUE(cJSON_IsArray(errors));
    TEST_ASSERT_TRUE(cJSON_GetArraySize(errors) <= 5);

    cJSON *system = cJSON_GetObjectItem(root, "system");
    TEST_ASSERT_TRUE(cJSON_IsObject(system));
    TEST_ASSERT_TRUE(cJSON_IsNumber(cJSON_GetObjectItem(system, "uptime_ms")));
    TEST_ASSERT_TRUE(cJSON_IsNumber(cJSON_GetObjectItem(system, "heap_free")));
    TEST_ASSERT_TRUE(cJSON_IsNumber(cJSON_GetObjectItem(system, "heap_min")));
    TEST_ASSERT_TRUE(cJSON_IsNumber(cJSON_GetObjectItem(system, "heap_largest_block")));

    cJSON_Delete(root);
}

static void test_contract_boundaries_control(void)
{
    api_control_request_t req = {0};

    TEST_ASSERT_EQUAL(ESP_OK, api_parse_control_json("{\"addr\":1,\"ep\":1,\"cmd\":0}", &req));
    TEST_ASSERT_EQUAL(ESP_OK, api_parse_control_json("{\"addr\":65535,\"ep\":240,\"cmd\":1}", &req));

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, api_parse_control_json("{\"addr\":0,\"ep\":1,\"cmd\":1}", &req));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, api_parse_control_json("{\"addr\":65536,\"ep\":1,\"cmd\":1}", &req));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, api_parse_control_json("{\"addr\":1,\"ep\":0,\"cmd\":1}", &req));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, api_parse_control_json("{\"addr\":1,\"ep\":241,\"cmd\":1}", &req));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, api_parse_control_json("{\"addr\":1,\"ep\":1,\"cmd\":2}", &req));
}

static void test_contract_boundaries_wifi_settings(void)
{
    api_wifi_save_request_t req = {0};

    TEST_ASSERT_EQUAL(ESP_OK, api_parse_wifi_save_json("{\"ssid\":\"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\",\"password\":\"12345678\"}", &req));
    TEST_ASSERT_EQUAL(ESP_OK, api_parse_wifi_save_json("{\"ssid\":\"S\",\"password\":\"1234567890123456789012345678901234567890123456789012345678901234\"}", &req));

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, api_parse_wifi_save_json("{\"ssid\":\"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\",\"password\":\"12345678\"}", &req));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, api_parse_wifi_save_json("{\"ssid\":\"S\",\"password\":\"1234567\"}", &req));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, api_parse_wifi_save_json("{\"ssid\":\"S\",\"password\":\"12345678901234567890123456789012345678901234567890123456789012345\"}", &req));
}

static void test_contract_boundaries_rename(void)
{
    api_rename_request_t req = {0};

    TEST_ASSERT_EQUAL(ESP_OK, api_parse_rename_json("{\"short_addr\":1,\"name\":\"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\"}", &req));

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, api_parse_rename_json("{\"short_addr\":0,\"name\":\"Lamp\"}", &req));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, api_parse_rename_json("{\"short_addr\":1,\"name\":\"\"}", &req));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, api_parse_rename_json("{\"short_addr\":1,\"name\":\"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\"}", &req));
}

static void test_factory_reset_report_smoke(void)
{
    system_factory_reset_report_t report = {0};
    TEST_ASSERT_EQUAL(ESP_OK, api_usecase_get_factory_reset_report(&report));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, report.wifi_err);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, report.devices_err);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, report.zigbee_storage_err);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, report.zigbee_fct_err);
}

static void test_frontend_contract_status_envelope_smoke(void)
{
    const char *json = "{\"status\":\"ok\",\"data\":{\"pan_id\":4660,\"channel\":13,\"short_addr\":0,\"devices\":[]}}";
    cJSON *root = cJSON_Parse(json);
    TEST_ASSERT_NOT_NULL(root);

    cJSON *status = cJSON_GetObjectItem(root, "status");
    cJSON *data = cJSON_GetObjectItem(root, "data");
    TEST_ASSERT_TRUE(cJSON_IsString(status));
    TEST_ASSERT_EQUAL_STRING("ok", status->valuestring);
    TEST_ASSERT_TRUE(cJSON_IsObject(data));
    TEST_ASSERT_TRUE(cJSON_IsArray(cJSON_GetObjectItem(data, "devices")));

    cJSON_Delete(root);
}

static void test_frontend_contract_scan_envelope_smoke(void)
{
    const char *json = "{\"status\":\"ok\",\"data\":[{\"ssid\":\"Net\",\"rssi\":-42,\"auth\":3}]}";
    cJSON *root = cJSON_Parse(json);
    TEST_ASSERT_NOT_NULL(root);

    cJSON *data = cJSON_GetObjectItem(root, "data");
    TEST_ASSERT_TRUE(cJSON_IsArray(data));
    TEST_ASSERT_EQUAL_INT(1, cJSON_GetArraySize(data));

    cJSON_Delete(root);
}

static void test_frontend_contract_factory_reset_envelope_smoke(void)
{
    const char *json = "{\"status\":\"ok\",\"data\":{\"message\":\"Factory reset done. Rebooting...\",\"details\":{\"wifi\":\"ESP_OK\",\"devices\":\"ESP_OK\",\"zigbee_storage\":\"ESP_OK\",\"zigbee_fct\":\"ESP_OK\"}}}";
    cJSON *root = cJSON_Parse(json);
    TEST_ASSERT_NOT_NULL(root);

    cJSON *data = cJSON_GetObjectItem(root, "data");
    cJSON *details = cJSON_GetObjectItem(data, "details");
    TEST_ASSERT_TRUE(cJSON_IsObject(data));
    TEST_ASSERT_TRUE(cJSON_IsObject(details));
    TEST_ASSERT_TRUE(cJSON_IsString(cJSON_GetObjectItem(details, "wifi")));
    TEST_ASSERT_TRUE(cJSON_IsString(cJSON_GetObjectItem(details, "devices")));
    TEST_ASSERT_TRUE(cJSON_IsString(cJSON_GetObjectItem(details, "zigbee_storage")));
    TEST_ASSERT_TRUE(cJSON_IsString(cJSON_GetObjectItem(details, "zigbee_fct")));

    cJSON_Delete(root);
}

void gateway_web_register_self_tests(void)
{
    RUN_TEST(test_devices_json_builder_small_buffer_fails);
    RUN_TEST(test_status_json_builder_small_buffer_fails);
    RUN_TEST(test_devices_json_builder_ok);
    RUN_TEST(test_status_json_builder_ok);
    RUN_TEST(test_health_json_builder_with_large_error_ring_truncates_and_stays_valid);
    RUN_TEST(test_contract_boundaries_control);
    RUN_TEST(test_contract_boundaries_wifi_settings);
    RUN_TEST(test_contract_boundaries_rename);
    RUN_TEST(test_factory_reset_report_smoke);
    RUN_TEST(test_frontend_contract_status_envelope_smoke);
    RUN_TEST(test_frontend_contract_scan_envelope_smoke);
    RUN_TEST(test_frontend_contract_factory_reset_envelope_smoke);
}
