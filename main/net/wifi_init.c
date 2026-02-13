#include "wifi_init.h"
#include "esp_log.h"
#include "wifi_context.h"
#include "wifi_sta.h"
#include "wifi_ap_fallback.h"
#include "gateway_state.h"
#include <string.h>

static const char *TAG = "wifi_init";
static wifi_runtime_ctx_t s_ctx = {0};

void wifi_state_store_update(void)
{
    gateway_wifi_state_t state = {
        .sta_connected = s_ctx.sta_connected,
        .fallback_ap_active = s_ctx.fallback_ap_active,
        .loaded_from_nvs = s_ctx.loaded_from_nvs,
    };
    strlcpy(state.active_ssid, s_ctx.active_ssid, sizeof(state.active_ssid));
    esp_err_t ret = gateway_state_set_wifi(&state);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to publish Wi-Fi state: %s", esp_err_to_name(ret));
    }
}

esp_err_t wifi_init_sta_and_wait(void)
{
    wifi_state_store_update();
    esp_err_t ret = wifi_sta_connect_and_wait(&s_ctx);
    if (ret == ESP_OK) {
        wifi_state_store_update();
        return ESP_OK;
    }

    ESP_LOGE(TAG, "Failed to connect to SSID:%s", s_ctx.active_ssid);
    ret = wifi_start_fallback_ap(&s_ctx);
    wifi_state_store_update();
    return (ret == ESP_OK) ? ESP_OK : ESP_FAIL;
}
