#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "api_usecases.h"

typedef struct {
    int inj_send_calls;
    uint16_t inj_send_short_addr;
    uint8_t inj_send_endpoint;
    uint8_t inj_send_on_off;
    esp_err_t inj_send_ret;

    int inj_wifi_save_calls;
    char inj_wifi_ssid[33];
    char inj_wifi_password[65];
    esp_err_t inj_wifi_save_ret;

    int inj_schedule_reboot_calls;
    uint32_t inj_schedule_reboot_delay_ms;
    esp_err_t inj_schedule_reboot_ret;

    int inj_factory_reset_calls;
    uint32_t inj_factory_reset_delay_ms;
    esp_err_t inj_factory_reset_ret;

    int facade_send_calls;
    uint16_t facade_send_short_addr;
    uint8_t facade_send_endpoint;
    uint8_t facade_send_on_off;
    esp_err_t facade_send_ret;

    gateway_core_factory_reset_report_t facade_factory_reset_report;
    esp_err_t facade_get_factory_report_ret;
} api_usecases_stub_t;

static api_usecases_stub_t g_stub;
static int g_wifi_system_ctx = 0;
static int g_jobs_ctx = 0;
static gateway_wifi_system_handle_t g_wifi_system_handle = (gateway_wifi_system_handle_t)&g_wifi_system_ctx;
static gateway_jobs_handle_t g_jobs_handle = (gateway_jobs_handle_t)&g_jobs_ctx;

size_t strlcpy(char *dst, const char *src, size_t dst_size)
{
    size_t src_len = strlen(src);
    if (dst_size > 0) {
        size_t copy_len = (src_len >= dst_size) ? (dst_size - 1) : src_len;
        memcpy(dst, src, copy_len);
        dst[copy_len] = '\0';
    }
    return src_len;
}

static void reset_stub(void)
{
    memset(&g_stub, 0, sizeof(g_stub));
    g_stub.inj_send_ret = ESP_OK;
    g_stub.inj_wifi_save_ret = ESP_OK;
    g_stub.inj_schedule_reboot_ret = ESP_OK;
    g_stub.inj_factory_reset_ret = ESP_OK;
    g_stub.facade_send_ret = ESP_OK;
    g_stub.facade_get_factory_report_ret = ESP_OK;
    g_stub.facade_factory_reset_report.wifi_err = ESP_OK;
    g_stub.facade_factory_reset_report.devices_err = ESP_OK;
    g_stub.facade_factory_reset_report.zigbee_storage_err = ESP_OK;
    g_stub.facade_factory_reset_report.zigbee_fct_err = ESP_OK;
    api_usecases_set_wifi_system_handle(g_wifi_system_handle);
    api_usecases_set_jobs_handle(g_jobs_handle);
}

static esp_err_t injected_send_on_off(uint16_t short_addr, uint8_t endpoint, uint8_t on_off)
{
    g_stub.inj_send_calls++;
    g_stub.inj_send_short_addr = short_addr;
    g_stub.inj_send_endpoint = endpoint;
    g_stub.inj_send_on_off = on_off;
    return g_stub.inj_send_ret;
}

static esp_err_t injected_wifi_save_credentials(const char *ssid, const char *password)
{
    g_stub.inj_wifi_save_calls++;
    strlcpy(g_stub.inj_wifi_ssid, ssid ? ssid : "", sizeof(g_stub.inj_wifi_ssid));
    strlcpy(g_stub.inj_wifi_password, password ? password : "", sizeof(g_stub.inj_wifi_password));
    return g_stub.inj_wifi_save_ret;
}

static esp_err_t injected_schedule_reboot(uint32_t delay_ms)
{
    g_stub.inj_schedule_reboot_calls++;
    g_stub.inj_schedule_reboot_delay_ms = delay_ms;
    return g_stub.inj_schedule_reboot_ret;
}

static esp_err_t injected_factory_reset_and_reboot(uint32_t reboot_delay_ms)
{
    g_stub.inj_factory_reset_calls++;
    g_stub.inj_factory_reset_delay_ms = reboot_delay_ms;
    return g_stub.inj_factory_reset_ret;
}

esp_err_t gateway_device_zigbee_send_on_off(uint16_t short_addr, uint8_t endpoint, uint8_t on_off)
{
    g_stub.facade_send_calls++;
    g_stub.facade_send_short_addr = short_addr;
    g_stub.facade_send_endpoint = endpoint;
    g_stub.facade_send_on_off = on_off;
    return g_stub.facade_send_ret;
}

esp_err_t gateway_wifi_system_create(const gateway_wifi_system_init_params_t *params, gateway_wifi_system_handle_t *out_handle)
{
    (void)params;
    if (!out_handle) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_handle = g_wifi_system_handle;
    return ESP_OK;
}

void gateway_wifi_system_destroy(gateway_wifi_system_handle_t handle)
{
    (void)handle;
}

esp_err_t gateway_wifi_system_save_credentials(gateway_wifi_system_handle_t handle, const char *ssid, const char *password)
{
    (void)handle;
    (void)ssid;
    (void)password;
    return ESP_OK;
}

esp_err_t gateway_wifi_system_schedule_reboot(gateway_wifi_system_handle_t handle, uint32_t delay_ms)
{
    (void)handle;
    (void)delay_ms;
    return ESP_OK;
}

esp_err_t gateway_wifi_system_factory_reset_and_reboot(gateway_wifi_system_handle_t handle, uint32_t reboot_delay_ms)
{
    (void)handle;
    (void)reboot_delay_ms;
    return ESP_OK;
}

esp_err_t gateway_device_zigbee_get_network_status(zigbee_network_status_t *out_status)
{
    if (!out_status) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(out_status, 0, sizeof(*out_status));
    return ESP_OK;
}

int gateway_device_zigbee_get_devices_snapshot(zb_device_t *out_devices, int max_devices)
{
    (void)out_devices;
    (void)max_devices;
    return 0;
}

int gateway_device_zigbee_get_neighbor_lqi_snapshot(zigbee_neighbor_lqi_t *out_neighbors, int max_neighbors)
{
    (void)out_neighbors;
    (void)max_neighbors;
    return 0;
}

esp_err_t gateway_device_zigbee_get_cached_lqi_snapshot(zigbee_neighbor_lqi_t *out_neighbors, int max_neighbors, int *out_count,
                                                      zigbee_lqi_source_t *out_source, uint64_t *out_updated_ms)
{
    (void)out_neighbors;
    (void)max_neighbors;
    if (out_count) {
        *out_count = 0;
    }
    if (out_source) {
        *out_source = ZIGBEE_LQI_SOURCE_UNKNOWN;
    }
    if (out_updated_ms) {
        *out_updated_ms = 0;
    }
    return ESP_OK;
}

esp_err_t gateway_device_zigbee_permit_join(uint8_t duration_seconds)
{
    (void)duration_seconds;
    return ESP_OK;
}

esp_err_t gateway_device_zigbee_delete_device(uint16_t short_addr)
{
    (void)short_addr;
    return ESP_OK;
}

esp_err_t gateway_device_zigbee_rename_device(uint16_t short_addr, const char *name)
{
    (void)short_addr;
    (void)name;
    return ESP_OK;
}

esp_err_t gateway_wifi_system_scan(gateway_wifi_system_handle_t handle, wifi_ap_info_t **out_list, size_t *out_count)
{
    (void)handle;
    if (!out_list || !out_count) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_list = NULL;
    *out_count = 0;
    return ESP_OK;
}

void gateway_wifi_system_scan_free(gateway_wifi_system_handle_t handle, wifi_ap_info_t *list)
{
    (void)handle;
    (void)list;
}

esp_err_t gateway_wifi_system_get_factory_reset_report(gateway_wifi_system_handle_t handle,
                                                       gateway_core_factory_reset_report_t *out_report)
{
    (void)handle;
    if (!out_report) {
        return ESP_ERR_INVALID_ARG;
    }
    if (g_stub.facade_get_factory_report_ret != ESP_OK) {
        return g_stub.facade_get_factory_report_ret;
    }
    *out_report = g_stub.facade_factory_reset_report;
    return ESP_OK;
}

esp_err_t gateway_wifi_system_collect_telemetry(gateway_wifi_system_handle_t handle, gateway_core_telemetry_t *out)
{
    (void)handle;
    if (!out) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));
    return ESP_OK;
}

esp_err_t gateway_jobs_create(const gateway_jobs_init_params_t *params, gateway_jobs_handle_t *out_handle)
{
    (void)params;
    if (!out_handle) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_handle = g_jobs_handle;
    return ESP_OK;
}

void gateway_jobs_destroy(gateway_jobs_handle_t handle)
{
    (void)handle;
}

esp_err_t gateway_jobs_get_metrics(gateway_jobs_handle_t handle, gateway_core_job_metrics_t *out_metrics)
{
    (void)handle;
    if (!out_metrics) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(out_metrics, 0, sizeof(*out_metrics));
    return ESP_OK;
}

esp_err_t gateway_jobs_submit(gateway_jobs_handle_t handle, gateway_core_job_type_t type, uint32_t reboot_delay_ms,
                              uint32_t *out_job_id)
{
    (void)handle;
    (void)type;
    (void)reboot_delay_ms;
    if (!out_job_id) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_job_id = 1;
    return ESP_OK;
}

esp_err_t gateway_jobs_get(gateway_jobs_handle_t handle, uint32_t job_id, gateway_core_job_info_t *out_info)
{
    (void)handle;
    if (!job_id || !out_info) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(out_info, 0, sizeof(*out_info));
    out_info->id = job_id;
    out_info->type = GATEWAY_CORE_JOB_TYPE_WIFI_SCAN;
    out_info->state = GATEWAY_CORE_JOB_STATE_QUEUED;
    out_info->err = ESP_OK;
    return ESP_OK;
}

esp_err_t gateway_wifi_system_get_network_state(gateway_wifi_system_handle_t handle, gateway_network_state_t *out_state)
{
    (void)handle;
    if (!out_state) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(out_state, 0, sizeof(*out_state));
    return ESP_OK;
}

esp_err_t gateway_wifi_system_get_wifi_state(gateway_wifi_system_handle_t handle, gateway_wifi_state_t *out_state)
{
    (void)handle;
    if (!out_state) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(out_state, 0, sizeof(*out_state));
    return ESP_OK;
}

esp_err_t gateway_wifi_system_get_schema_version(gateway_wifi_system_handle_t handle, int32_t *out_version)
{
    (void)handle;
    if (!out_version) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_version = 1;
    return ESP_OK;
}

static void test_usecase_control_with_injected_ops(void)
{
    reset_stub();
    const api_service_ops_t ops = {
        .send_on_off = injected_send_on_off,
        .wifi_save_credentials = injected_wifi_save_credentials,
        .schedule_reboot = injected_schedule_reboot,
        .factory_reset_and_reboot = injected_factory_reset_and_reboot,
    };
    api_usecases_set_service_ops(&ops);

    api_control_request_t req = {
        .addr = 0x1234,
        .ep = 2,
        .cmd = 1,
    };
    assert(api_usecase_control(&req) == ESP_OK);
    assert(g_stub.inj_send_calls == 1);
    assert(g_stub.inj_send_short_addr == 0x1234);
    assert(g_stub.inj_send_endpoint == 2);
    assert(g_stub.inj_send_on_off == 1);

    g_stub.inj_send_ret = ESP_FAIL;
    assert(api_usecase_control(&req) == ESP_FAIL);
    assert(api_usecase_control(NULL) == ESP_ERR_INVALID_ARG);
}

static void test_usecase_wifi_save_mapping(void)
{
    reset_stub();
    const api_service_ops_t ops = {
        .send_on_off = injected_send_on_off,
        .wifi_save_credentials = injected_wifi_save_credentials,
        .schedule_reboot = injected_schedule_reboot,
        .factory_reset_and_reboot = injected_factory_reset_and_reboot,
    };
    api_usecases_set_service_ops(&ops);

    api_wifi_save_request_t req = {0};
    strlcpy(req.ssid, "HomeWiFi", sizeof(req.ssid));
    strlcpy(req.password, "verysecret", sizeof(req.password));

    assert(api_usecase_wifi_save(&req) == ESP_OK);
    assert(g_stub.inj_wifi_save_calls == 1);
    assert(strcmp(g_stub.inj_wifi_ssid, "HomeWiFi") == 0);
    assert(strcmp(g_stub.inj_wifi_password, "verysecret") == 0);
    assert(g_stub.inj_schedule_reboot_calls == 1);
    assert(g_stub.inj_schedule_reboot_delay_ms == 1000);

    reset_stub();
    api_usecases_set_service_ops(&ops);
    g_stub.inj_wifi_save_ret = ESP_FAIL;
    assert(api_usecase_wifi_save(&req) == ESP_FAIL);
    assert(g_stub.inj_schedule_reboot_calls == 0);
}

static void test_usecase_factory_reset_mapping(void)
{
    reset_stub();
    const api_service_ops_t ops = {
        .send_on_off = injected_send_on_off,
        .wifi_save_credentials = injected_wifi_save_credentials,
        .schedule_reboot = injected_schedule_reboot,
        .factory_reset_and_reboot = injected_factory_reset_and_reboot,
    };
    api_usecases_set_service_ops(&ops);

    g_stub.inj_factory_reset_ret = ESP_ERR_NOT_SUPPORTED;
    assert(api_usecase_factory_reset() == ESP_ERR_NOT_SUPPORTED);
    assert(g_stub.inj_factory_reset_calls == 1);
    assert(g_stub.inj_factory_reset_delay_ms == 1000);
}

static void test_default_ops_fallback_uses_facade(void)
{
    reset_stub();
    api_usecases_set_service_ops(NULL);

    g_stub.facade_send_ret = ESP_ERR_INVALID_ARG;
    api_control_request_t req = {
        .addr = 0x2222,
        .ep = 3,
        .cmd = 0,
    };
    assert(api_usecase_control(&req) == ESP_ERR_INVALID_ARG);
    assert(g_stub.facade_send_calls == 1);
    assert(g_stub.facade_send_short_addr == 0x2222);
    assert(g_stub.facade_send_endpoint == 3);
    assert(g_stub.facade_send_on_off == 0);
}

static void test_factory_report_mapping(void)
{
    reset_stub();
    g_stub.facade_factory_reset_report.wifi_err = ESP_FAIL;
    g_stub.facade_factory_reset_report.devices_err = ESP_ERR_NOT_FOUND;
    g_stub.facade_factory_reset_report.zigbee_storage_err = ESP_ERR_INVALID_SIZE;
    g_stub.facade_factory_reset_report.zigbee_fct_err = ESP_ERR_NOT_SUPPORTED;

    api_factory_reset_report_t out = {0};
    assert(api_usecase_get_factory_reset_report(&out) == ESP_OK);
    assert(out.wifi_err == ESP_FAIL);
    assert(out.devices_err == ESP_ERR_NOT_FOUND);
    assert(out.zigbee_storage_err == ESP_ERR_INVALID_SIZE);
    assert(out.zigbee_fct_err == ESP_ERR_NOT_SUPPORTED);
    assert(api_usecase_get_factory_reset_report(NULL) == ESP_ERR_INVALID_ARG);
}

int main(void)
{
    printf("Running host tests: api_usecases_host_test\n");
    test_usecase_control_with_injected_ops();
    test_usecase_wifi_save_mapping();
    test_usecase_factory_reset_mapping();
    test_default_ops_fallback_uses_facade();
    test_factory_report_mapping();
    printf("Host tests passed: api_usecases_host_test\n");
    return 0;
}
