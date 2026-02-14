#include "unity.h"
#include "api_handlers.h"
#include "api_contracts.h"
#include "api_usecases.h"
#include "lqi_json_mapper.h"
#include "error_ring.h"
#include "gateway_state.h"
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

static cJSON *find_neighbor_by_addr(cJSON *neighbors, uint16_t short_addr)
{
    if (!cJSON_IsArray(neighbors)) {
        return NULL;
    }
    const int n = cJSON_GetArraySize(neighbors);
    for (int i = 0; i < n; i++) {
        cJSON *item = cJSON_GetArrayItem(neighbors, i);
        cJSON *addr = cJSON_GetObjectItem(item, "short_addr");
        if (cJSON_IsNumber(addr) && (uint16_t)addr->valueint == short_addr) {
            return item;
        }
    }
    return NULL;
}

static void test_lqi_json_mapper_uses_cached_snapshot_contract(void)
{
    zb_device_t devices[2] = {
        {.short_addr = 0x1001, .name = "Dev A"},
        {.short_addr = 0x1002, .name = "Dev B"},
    };
    TEST_ASSERT_EQUAL(ESP_OK, gateway_state_set_devices(devices, 2));
    TEST_ASSERT_EQUAL(ESP_OK, gateway_state_update_device_lqi(0x1001, 150, 127, GATEWAY_LQI_SOURCE_MGMT_LQI, 1000));
    TEST_ASSERT_EQUAL(ESP_OK, gateway_state_update_device_lqi(0x1002, 70, -80, GATEWAY_LQI_SOURCE_NEIGHBOR_TABLE, 900));

    char buf[2048];
    size_t out_len = 0;
    esp_err_t ret = build_lqi_json_compact(buf, sizeof(buf), &out_len);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_GREATER_THAN_UINT32(0, (uint32_t)out_len);

    cJSON *root = cJSON_ParseWithLength(buf, out_len);
    TEST_ASSERT_NOT_NULL(root);

    cJSON *neighbors = cJSON_GetObjectItem(root, "neighbors");
    TEST_ASSERT_TRUE(cJSON_IsArray(neighbors));
    TEST_ASSERT_EQUAL_INT(2, cJSON_GetArraySize(neighbors));

    cJSON *a = find_neighbor_by_addr(neighbors, 0x1001);
    cJSON *b = find_neighbor_by_addr(neighbors, 0x1002);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(b);

    TEST_ASSERT_TRUE(cJSON_IsNumber(cJSON_GetObjectItem(a, "lqi")));
    TEST_ASSERT_EQUAL_INT(150, cJSON_GetObjectItem(a, "lqi")->valueint);
    TEST_ASSERT_TRUE(cJSON_IsNull(cJSON_GetObjectItem(a, "rssi")));
    TEST_ASSERT_EQUAL_STRING("warn", cJSON_GetObjectItem(a, "quality")->valuestring);
    TEST_ASSERT_EQUAL_STRING("mgmt_lqi", cJSON_GetObjectItem(a, "source")->valuestring);

    TEST_ASSERT_TRUE(cJSON_IsNumber(cJSON_GetObjectItem(b, "lqi")));
    TEST_ASSERT_EQUAL_INT(70, cJSON_GetObjectItem(b, "lqi")->valueint);
    TEST_ASSERT_TRUE(cJSON_IsNumber(cJSON_GetObjectItem(b, "rssi")));
    TEST_ASSERT_EQUAL_INT(-80, cJSON_GetObjectItem(b, "rssi")->valueint);
    TEST_ASSERT_EQUAL_STRING("bad", cJSON_GetObjectItem(b, "quality")->valuestring);
    TEST_ASSERT_EQUAL_STRING("neighbor_table", cJSON_GetObjectItem(b, "source")->valuestring);

    TEST_ASSERT_EQUAL_STRING("mgmt_lqi", cJSON_GetObjectItem(root, "source")->valuestring);
    TEST_ASSERT_EQUAL_INT(1000, cJSON_GetObjectItem(root, "updated_ms")->valueint);

    cJSON_Delete(root);
}

static void test_health_snapshot_usecase_contract(void)
{
    gateway_network_state_t net = {
        .zigbee_started = true,
        .factory_new = false,
        .pan_id = 0x1234,
        .channel = 15,
        .short_addr = 0x0000,
    };
    gateway_wifi_state_t wifi = {
        .sta_connected = true,
        .fallback_ap_active = false,
        .loaded_from_nvs = true,
        .active_ssid = "SelfTestNet",
    };
    TEST_ASSERT_EQUAL(ESP_OK, gateway_state_set_network(&net));
    TEST_ASSERT_EQUAL(ESP_OK, gateway_state_set_wifi(&wifi));

    api_health_snapshot_t snap = {0};
    esp_err_t ret = api_usecase_collect_health_snapshot(&snap);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    TEST_ASSERT_TRUE(snap.zigbee_started);
    TEST_ASSERT_EQUAL_UINT16(0x1234, (uint16_t)snap.zigbee_pan_id);
    TEST_ASSERT_EQUAL_UINT8(15, (uint8_t)snap.zigbee_channel);
    TEST_ASSERT_TRUE(snap.wifi_sta_connected);
    TEST_ASSERT_FALSE(snap.wifi_fallback_ap_active);
    TEST_ASSERT_EQUAL_STRING("SelfTestNet", snap.wifi_active_ssid);
    TEST_ASSERT_TRUE(snap.telemetry.uptime_ms >= 0);
    TEST_ASSERT_TRUE(snap.telemetry.heap_free > 0);
}

static void test_health_json_wifi_active_ssid_is_canonical(void)
{
    char buf[4096];
    size_t out_len = 0;
    esp_err_t ret = build_health_json_compact(buf, sizeof(buf), &out_len);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    cJSON *root = cJSON_ParseWithLength(buf, out_len);
    TEST_ASSERT_NOT_NULL(root);
    cJSON *wifi = cJSON_GetObjectItem(root, "wifi");
    TEST_ASSERT_TRUE(cJSON_IsObject(wifi));
    TEST_ASSERT_TRUE(cJSON_IsString(cJSON_GetObjectItem(wifi, "active_ssid")));
    TEST_ASSERT_NULL(cJSON_GetObjectItem(wifi, "ssid"));
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

static void test_ws_lqi_update_envelope_smoke(void)
{
    const char *json =
        "{\"version\":1,\"seq\":42,\"ts\":1700000000,\"type\":\"lqi_update\","
        "\"data\":{\"neighbors\":[{\"short_addr\":4097,\"name\":\"Dev A\",\"lqi\":150,\"rssi\":null,"
        "\"quality\":\"warn\",\"direct\":true,\"source\":\"mgmt_lqi\",\"updated_ms\":1000}],"
        "\"updated_ms\":1000,\"source\":\"mgmt_lqi\"}}";

    cJSON *root = cJSON_Parse(json);
    TEST_ASSERT_NOT_NULL(root);

    TEST_ASSERT_TRUE(cJSON_IsNumber(cJSON_GetObjectItem(root, "version")));
    TEST_ASSERT_EQUAL_INT(1, cJSON_GetObjectItem(root, "version")->valueint);
    TEST_ASSERT_TRUE(cJSON_IsNumber(cJSON_GetObjectItem(root, "seq")));
    TEST_ASSERT_TRUE(cJSON_IsNumber(cJSON_GetObjectItem(root, "ts")));
    TEST_ASSERT_TRUE(cJSON_IsString(cJSON_GetObjectItem(root, "type")));
    TEST_ASSERT_EQUAL_STRING("lqi_update", cJSON_GetObjectItem(root, "type")->valuestring);

    cJSON *data = cJSON_GetObjectItem(root, "data");
    TEST_ASSERT_TRUE(cJSON_IsObject(data));
    cJSON *neighbors = cJSON_GetObjectItem(data, "neighbors");
    TEST_ASSERT_TRUE(cJSON_IsArray(neighbors));
    TEST_ASSERT_EQUAL_INT(1, cJSON_GetArraySize(neighbors));

    cJSON *n0 = cJSON_GetArrayItem(neighbors, 0);
    TEST_ASSERT_TRUE(cJSON_IsObject(n0));
    TEST_ASSERT_TRUE(cJSON_IsNumber(cJSON_GetObjectItem(n0, "short_addr")));
    TEST_ASSERT_TRUE(cJSON_IsNumber(cJSON_GetObjectItem(n0, "lqi")));
    TEST_ASSERT_TRUE(cJSON_IsString(cJSON_GetObjectItem(n0, "quality")));
    TEST_ASSERT_TRUE(cJSON_IsBool(cJSON_GetObjectItem(n0, "direct")));
    TEST_ASSERT_TRUE(cJSON_IsString(cJSON_GetObjectItem(n0, "source")));

    cJSON_Delete(root);
}

void gateway_web_register_self_tests(void)
{
    RUN_TEST(test_devices_json_builder_small_buffer_fails);
    RUN_TEST(test_status_json_builder_small_buffer_fails);
    RUN_TEST(test_devices_json_builder_ok);
    RUN_TEST(test_status_json_builder_ok);
    RUN_TEST(test_health_json_builder_with_large_error_ring_truncates_and_stays_valid);
    RUN_TEST(test_lqi_json_mapper_uses_cached_snapshot_contract);
    RUN_TEST(test_health_snapshot_usecase_contract);
    RUN_TEST(test_health_json_wifi_active_ssid_is_canonical);
    RUN_TEST(test_contract_boundaries_control);
    RUN_TEST(test_contract_boundaries_wifi_settings);
    RUN_TEST(test_contract_boundaries_rename);
    RUN_TEST(test_factory_reset_report_smoke);
    RUN_TEST(test_frontend_contract_status_envelope_smoke);
    RUN_TEST(test_frontend_contract_scan_envelope_smoke);
    RUN_TEST(test_frontend_contract_factory_reset_envelope_smoke);
    RUN_TEST(test_ws_lqi_update_envelope_smoke);
}
