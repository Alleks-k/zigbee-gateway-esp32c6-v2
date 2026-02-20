#include "gateway_app.h"

#include "config_service.h"
#include "device_service.h"
#include "error_ring.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_spiffs.h"
#include "esp_timer.h"
#include "gateway_app_runtime.h"
#include "gateway_events.h"
#include "gateway_status_esp.h"
#include "gateway_zigbee_runtime.h"
#include "http_error.h"
#include "nvs_flash.h"
#include "state_store.h"
#include "web_server.h"
#include "wifi_init.h"

static const char *TAG = "GATEWAY_APP";
static esp_event_handler_instance_t s_device_announce_handler = NULL;
static gateway_app_runtime_handles_t s_runtime_handles = {0};

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

static void gateway_app_device_announce_event_handler(void *arg,
                                                       esp_event_base_t event_base,
                                                       int32_t event_id,
                                                       void *event_data)
{
    device_service_handle_t device_service = (device_service_handle_t)arg;
    if (!device_service) {
        return;
    }
    if (event_base != GATEWAY_EVENT || event_id != GATEWAY_EVENT_DEVICE_ANNOUNCE || !event_data) {
        return;
    }

    gateway_device_announce_event_t *evt = (gateway_device_announce_event_t *)event_data;
    device_service_add_with_ieee(device_service, evt->short_addr, evt->ieee_addr);
}

static void gateway_app_detach_device_events(void)
{
    if (!s_device_announce_handler) {
        return;
    }

    (void)esp_event_handler_instance_unregister(
        GATEWAY_EVENT, GATEWAY_EVENT_DEVICE_ANNOUNCE, s_device_announce_handler);
    s_device_announce_handler = NULL;
}

static esp_err_t gateway_app_attach_device_events(device_service_handle_t device_service)
{
    if (!device_service) {
        return ESP_ERR_INVALID_ARG;
    }

    return esp_event_handler_instance_register(
        GATEWAY_EVENT,
        GATEWAY_EVENT_DEVICE_ANNOUNCE,
        gateway_app_device_announce_event_handler,
        device_service,
        &s_device_announce_handler);
}

void gateway_app_start(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    gateway_error_ring_set_now_ms_provider(gateway_app_now_ms_provider);
    http_error_set_map_provider(gateway_app_http_error_map_provider);
    ESP_ERROR_CHECK(gateway_status_to_esp_err(config_service_init_or_migrate()));

    gateway_app_detach_device_events();
    gateway_app_runtime_destroy(&s_runtime_handles);
    ESP_ERROR_CHECK(gateway_app_runtime_create(&s_runtime_handles));
    gateway_state_set_now_ms_provider(gateway_app_now_ms_provider);
    ESP_ERROR_CHECK(wifi_init_bind_state(s_runtime_handles.gateway_state));
    ESP_ERROR_CHECK(gateway_app_attach_device_events(s_runtime_handles.device_service));

    esp_err_t wifi_ret = wifi_init_sta_and_wait();
    if (wifi_ret != ESP_OK) {
        ESP_LOGE(TAG,
                 "Wi-Fi STA connection failed (%s). Continuing without network.",
                 esp_err_to_name(wifi_ret));
    }

    esp_vfs_spiffs_conf_t www_conf = {
        .base_path = "/www",
        .partition_label = "www",
        .max_files = 5,
        .format_if_mount_failed = true,
    };
    ESP_ERROR_CHECK(esp_vfs_spiffs_register(&www_conf));

    start_web_server(s_runtime_handles.ws_manager, s_runtime_handles.api_usecases);

    gateway_wifi_state_t wifi_state = {0};
    esp_err_t state_ret = gateway_status_to_esp_err(gateway_state_get_wifi(s_runtime_handles.gateway_state, &wifi_state));
    if (state_ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read Wi-Fi state: %s", esp_err_to_name(state_ret));
    }
    if (state_ret == ESP_OK && wifi_state.fallback_ap_active) {
        ESP_LOGW(TAG, "Fallback AP mode active: Zigbee stack startup is postponed to keep web setup stable");
        return;
    }

    ESP_ERROR_CHECK(gateway_zigbee_runtime_start());
}
