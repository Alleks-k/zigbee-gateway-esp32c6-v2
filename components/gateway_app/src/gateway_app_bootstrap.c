#include "gateway_app.h"

#include "config_service.h"
#include "device_service.h"
#include "error_ring.h"
#include "gateway_wifi_system_facade.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_spiffs.h"
#include "esp_timer.h"
#include "http_error.h"
#include "nvs.h"
#include "state_store.h"
#include "gateway_zigbee_runtime.h"
#include "nvs_flash.h"
#include "web_server.h"
#include "wifi_init.h"

static const char *TAG = "GATEWAY_APP";

static uint64_t gateway_app_now_ms_provider(void)
{
    return (uint64_t)(esp_timer_get_time() / 1000);
}

static bool gateway_app_http_error_map_provider(esp_err_t err, int *out_http_status, const char **out_error_code)
{
    if (!out_http_status || !out_error_code) {
        return false;
    }

    switch (err) {
    case ESP_ERR_NVS_NOT_FOUND:
        *out_http_status = 404;
        *out_error_code = "not_found";
        return true;
    default:
        return false;
    }
}

void gateway_app_start(void)
{
    device_service_handle_t device_service = NULL;
    gateway_state_handle_t gateway_state = NULL;
    gateway_runtime_context_t runtime_ctx = {0};

    ESP_ERROR_CHECK(nvs_flash_init());
    gateway_error_ring_set_now_ms_provider(gateway_app_now_ms_provider);
    http_error_set_map_provider(gateway_app_http_error_map_provider);
    ESP_ERROR_CHECK(config_service_init_or_migrate());
    ESP_ERROR_CHECK(device_service_get_default(&device_service));
    ESP_ERROR_CHECK(gateway_state_get_default(&gateway_state));
    ESP_ERROR_CHECK(device_service_init(device_service));
    ESP_ERROR_CHECK(gateway_state_init(gateway_state));
    runtime_ctx.device_service = device_service;
    runtime_ctx.gateway_state = gateway_state;
    ESP_ERROR_CHECK(gateway_wifi_system_init(&runtime_ctx));
    ESP_ERROR_CHECK(wifi_init_bind_state(gateway_state));
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(gateway_zigbee_runtime_prepare(&runtime_ctx));

    esp_err_t wifi_ret = wifi_init_sta_and_wait();
    if (wifi_ret != ESP_OK) {
        ESP_LOGE(TAG, "Wi-Fi STA connection failed (%s). Continuing without network.",
                 esp_err_to_name(wifi_ret));
    }

    esp_vfs_spiffs_conf_t www_conf = {
        .base_path = "/www", .partition_label = "www", .max_files = 5, .format_if_mount_failed = true
    };
    ESP_ERROR_CHECK(esp_vfs_spiffs_register(&www_conf));

    start_web_server();

    gateway_wifi_state_t wifi_state = {0};
    esp_err_t state_ret = gateway_state_get_wifi(gateway_state, &wifi_state);
    if (state_ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read Wi-Fi state: %s", esp_err_to_name(state_ret));
    }
    if (state_ret == ESP_OK && wifi_state.fallback_ap_active) {
        ESP_LOGW(TAG, "Fallback AP mode active: Zigbee stack startup is postponed to keep web setup stable");
        return;
    }

    ESP_ERROR_CHECK(gateway_zigbee_runtime_start());
}
