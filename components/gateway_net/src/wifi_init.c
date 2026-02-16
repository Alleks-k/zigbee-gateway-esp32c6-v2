#include "wifi_init.h"
#include "esp_log.h"
#include "wifi_context.h"
#include "wifi_sta.h"
#include "wifi_ap_fallback.h"
#include "net_platform_services.h"
#include "state_store.h"
#include <string.h>

static const char *TAG = "wifi_init";
static wifi_runtime_ctx_t s_ctx = {0};
static gateway_state_handle_t s_gateway_state = NULL;

static esp_err_t ensure_gateway_state_handle(void)
{
    if (s_gateway_state) {
        return ESP_OK;
    }
    return gateway_state_get_default(&s_gateway_state);
}

#if CONFIG_GATEWAY_SELF_TEST_APP
static void wifi_init_default_net_platform_services_init(void)
{
    net_platform_services_init();
}

static esp_err_t wifi_init_default_sta_connect_and_wait(wifi_runtime_ctx_t *ctx)
{
    return wifi_sta_connect_and_wait(ctx);
}

static esp_err_t wifi_init_default_start_fallback_ap(wifi_runtime_ctx_t *ctx)
{
    return wifi_start_fallback_ap(ctx);
}

static wifi_init_ops_t s_wifi_init_ops = {
    .net_platform_services_init = wifi_init_default_net_platform_services_init,
    .wifi_sta_connect_and_wait = wifi_init_default_sta_connect_and_wait,
    .wifi_start_fallback_ap = wifi_init_default_start_fallback_ap,
};
#endif

void wifi_state_store_update(void)
{
    esp_err_t handle_ret = ensure_gateway_state_handle();
    if (handle_ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to access gateway state handle: %s", esp_err_to_name(handle_ret));
        return;
    }

    gateway_wifi_state_t state = {
        .sta_connected = s_ctx.sta_connected,
        .fallback_ap_active = s_ctx.fallback_ap_active,
        .loaded_from_nvs = s_ctx.loaded_from_nvs,
    };
    strlcpy(state.active_ssid, s_ctx.active_ssid, sizeof(state.active_ssid));
    esp_err_t ret = gateway_state_set_wifi(s_gateway_state, &state);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to publish Wi-Fi state: %s", esp_err_to_name(ret));
    }
}

esp_err_t wifi_init_sta_and_wait(void)
{
#if CONFIG_GATEWAY_SELF_TEST_APP
    if (s_wifi_init_ops.net_platform_services_init) {
        s_wifi_init_ops.net_platform_services_init();
    }
#else
    net_platform_services_init();
#endif
    wifi_state_store_update();
    esp_err_t ret = ESP_FAIL;
#if CONFIG_GATEWAY_SELF_TEST_APP
    if (s_wifi_init_ops.wifi_sta_connect_and_wait) {
        ret = s_wifi_init_ops.wifi_sta_connect_and_wait(&s_ctx);
    }
#else
    ret = wifi_sta_connect_and_wait(&s_ctx);
#endif
    if (ret == ESP_OK) {
        wifi_state_store_update();
        return ESP_OK;
    }

    ESP_LOGE(TAG, "Failed to connect to SSID:%s", s_ctx.active_ssid);
    ret = ESP_FAIL;
#if CONFIG_GATEWAY_SELF_TEST_APP
    if (s_wifi_init_ops.wifi_start_fallback_ap) {
        ret = s_wifi_init_ops.wifi_start_fallback_ap(&s_ctx);
    }
#else
    ret = wifi_start_fallback_ap(&s_ctx);
#endif
    wifi_state_store_update();
    return (ret == ESP_OK) ? ESP_OK : ESP_FAIL;
}

#if CONFIG_GATEWAY_SELF_TEST_APP
void wifi_init_set_ops_for_test(const wifi_init_ops_t *ops)
{
    if (!ops) {
        return;
    }
    s_wifi_init_ops = *ops;
}

void wifi_init_reset_ops_for_test(void)
{
    s_wifi_init_ops.net_platform_services_init = wifi_init_default_net_platform_services_init;
    s_wifi_init_ops.wifi_sta_connect_and_wait = wifi_init_default_sta_connect_and_wait;
    s_wifi_init_ops.wifi_start_fallback_ap = wifi_init_default_start_fallback_ap;
}
#endif
