#include "gateway_app.h"

#include "config_service.h"
#include "device_service.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_spiffs.h"
#include "state_store.h"
#include "gateway_zigbee_runtime.h"
#include "nvs_flash.h"
#include "web_server.h"
#include "wifi_init.h"

static const char *TAG = "GATEWAY_APP";

void gateway_app_start(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(config_service_init_or_migrate());
    ESP_ERROR_CHECK(device_service_init());
    ESP_ERROR_CHECK(gateway_state_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(gateway_zigbee_runtime_prepare());

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
    esp_err_t state_ret = gateway_state_get_wifi(&wifi_state);
    if (state_ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read Wi-Fi state: %s", esp_err_to_name(state_ret));
    }
    if (state_ret == ESP_OK && wifi_state.fallback_ap_active) {
        ESP_LOGW(TAG, "Fallback AP mode active: Zigbee stack startup is postponed to keep web setup stable");
        return;
    }

    ESP_ERROR_CHECK(gateway_zigbee_runtime_start());
}
