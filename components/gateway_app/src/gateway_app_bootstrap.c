#include "gateway_app.h"

#include "config_service.h"
#include "device_service.h"
#include "error_ring.h"
#include "gateway_events.h"
#include "gateway_wifi_system_facade.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_spiffs.h"
#include "esp_timer.h"
#include "http_error.h"
#include "nvs.h"
#include "gateway_status_esp.h"
#include "state_store.h"
#include "gateway_zigbee_runtime.h"
#include "nvs_flash.h"
#include "web_server.h"
#include "wifi_init.h"

#include <string.h>

static const char *TAG = "GATEWAY_APP";
static esp_event_handler_instance_t s_device_announce_handler = NULL;

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

static void gateway_app_device_announce_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
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

static void gateway_app_on_device_list_changed(void *ctx)
{
    (void)ctx;
    (void)esp_event_post(GATEWAY_EVENT, GATEWAY_EVENT_DEVICE_LIST_CHANGED, NULL, 0, 0);
}

static void gateway_app_on_device_delete_request(void *ctx, uint16_t short_addr, const gateway_ieee_addr_t ieee_addr)
{
    (void)ctx;
    gateway_device_delete_request_event_t evt = {
        .short_addr = short_addr,
    };
    memcpy(evt.ieee_addr, ieee_addr, sizeof(evt.ieee_addr));
    (void)esp_event_post(GATEWAY_EVENT, GATEWAY_EVENT_DEVICE_DELETE_REQUEST, &evt, sizeof(evt), 0);
}

void gateway_app_start(void)
{
    device_service_handle_t device_service = NULL;
    gateway_state_handle_t gateway_state = NULL;
    gateway_runtime_context_t runtime_ctx = {0};
    gateway_wifi_system_init_params_t wifi_system_params = {0};
    device_service_notifier_t device_notifier = {
        .on_list_changed = gateway_app_on_device_list_changed,
        .on_delete_request = gateway_app_on_device_delete_request,
        .ctx = NULL,
    };

    ESP_ERROR_CHECK(nvs_flash_init());
    gateway_error_ring_set_now_ms_provider(gateway_app_now_ms_provider);
    http_error_set_map_provider(gateway_app_http_error_map_provider);
    ESP_ERROR_CHECK(gateway_status_to_esp_err(config_service_init_or_migrate()));
    ESP_ERROR_CHECK(gateway_status_to_esp_err(device_service_create(&device_service)));
    ESP_ERROR_CHECK(gateway_status_to_esp_err(gateway_state_create(&gateway_state)));
    ESP_ERROR_CHECK(gateway_status_to_esp_err(device_service_set_notifier(device_service, &device_notifier)));
    ESP_ERROR_CHECK(gateway_status_to_esp_err(device_service_init(device_service)));
    ESP_ERROR_CHECK(gateway_status_to_esp_err(gateway_state_init(gateway_state)));
    gateway_state_set_now_ms_provider(gateway_app_now_ms_provider);
    runtime_ctx.device_service = device_service;
    runtime_ctx.gateway_state = gateway_state;
    wifi_system_params.gateway_state_handle = gateway_state;
    ESP_ERROR_CHECK(gateway_wifi_system_init(&wifi_system_params));
    ESP_ERROR_CHECK(wifi_init_bind_state(gateway_state));
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    gateway_app_detach_device_events();
    ESP_ERROR_CHECK(gateway_app_attach_device_events(device_service));
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
    esp_err_t state_ret = gateway_status_to_esp_err(gateway_state_get_wifi(gateway_state, &wifi_state));
    if (state_ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read Wi-Fi state: %s", esp_err_to_name(state_ret));
    }
    if (state_ret == ESP_OK && wifi_state.fallback_ap_active) {
        ESP_LOGW(TAG, "Fallback AP mode active: Zigbee stack startup is postponed to keep web setup stable");
        return;
    }

    ESP_ERROR_CHECK(gateway_zigbee_runtime_start());
}
