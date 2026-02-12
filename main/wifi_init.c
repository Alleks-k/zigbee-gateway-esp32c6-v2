#include "wifi_init.h"
#include "esp_log.h"
#include "wifi_context.h"
#include "wifi_sta.h"
#include "wifi_ap_fallback.h"

static const char *TAG = "wifi_init";
static wifi_runtime_ctx_t s_ctx = {0};

esp_err_t wifi_init_sta_and_wait(void)
{
    esp_err_t ret = wifi_sta_connect_and_wait(&s_ctx);
    if (ret == ESP_OK) {
        return ESP_OK;
    }

    ESP_LOGE(TAG, "Failed to connect to SSID:%s", s_ctx.active_ssid);
    ret = wifi_start_fallback_ap(&s_ctx);
    return (ret == ESP_OK) ? ESP_OK : ESP_FAIL;
}

bool wifi_is_fallback_ap_active(void)
{
    return s_ctx.fallback_ap_active;
}

bool wifi_loaded_credentials_from_nvs(void)
{
    return s_ctx.loaded_from_nvs;
}
