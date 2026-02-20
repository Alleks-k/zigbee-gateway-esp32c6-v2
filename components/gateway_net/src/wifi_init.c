#include "wifi_init.h"
#include "esp_log.h"
#include "wifi_context.h"
#include "wifi_sta.h"
#include "wifi_ap_fallback.h"
#include "net_platform_services.h"
#include "gateway_status_esp.h"
#include "state_store.h"
#include <string.h>

static const char *TAG = "wifi_init";

esp_err_t wifi_init_bind_state(wifi_runtime_ctx_t *ctx,
                               gateway_state_handle_t state_handle,
                               struct wifi_service *wifi_service,
                               struct system_service *system_service)
{
    if (!ctx || !state_handle || !wifi_service || !system_service) {
        return ESP_ERR_INVALID_ARG;
    }
    ctx->gateway_state = state_handle;
    ctx->wifi_service = wifi_service;
    ctx->system_service = system_service;
#if CONFIG_GATEWAY_SELF_TEST_APP
    wifi_init_reset_ops_for_test(ctx);
#endif
    return ESP_OK;
}

#if CONFIG_GATEWAY_SELF_TEST_APP
static void wifi_init_default_net_platform_services_init(wifi_runtime_ctx_t *ctx)
{
    if (!ctx) {
        return;
    }
    net_platform_services_init(ctx->wifi_service, ctx->system_service);
}

static esp_err_t wifi_init_default_sta_connect_and_wait(wifi_runtime_ctx_t *ctx)
{
    return wifi_sta_connect_and_wait(ctx);
}

static esp_err_t wifi_init_default_start_fallback_ap(wifi_runtime_ctx_t *ctx)
{
    return wifi_start_fallback_ap(ctx);
}

static void wifi_init_apply_default_ops_if_needed(wifi_runtime_ctx_t *ctx)
{
    if (!ctx) {
        return;
    }
    if (!ctx->ops.net_platform_services_init) {
        ctx->ops.net_platform_services_init = wifi_init_default_net_platform_services_init;
    }
    if (!ctx->ops.wifi_sta_connect_and_wait) {
        ctx->ops.wifi_sta_connect_and_wait = wifi_init_default_sta_connect_and_wait;
    }
    if (!ctx->ops.wifi_start_fallback_ap) {
        ctx->ops.wifi_start_fallback_ap = wifi_init_default_start_fallback_ap;
    }
}
#endif

void wifi_state_store_update(wifi_runtime_ctx_t *ctx)
{
    if (!ctx || !ctx->gateway_state) {
        ESP_LOGW(TAG, "Gateway state handle is not bound");
        return;
    }

    gateway_wifi_state_t state = {
        .sta_connected = ctx->sta_connected,
        .fallback_ap_active = ctx->fallback_ap_active,
        .loaded_from_nvs = ctx->loaded_from_nvs,
    };
    strlcpy(state.active_ssid, ctx->active_ssid, sizeof(state.active_ssid));
    esp_err_t ret = gateway_status_to_esp_err(gateway_state_set_wifi(ctx->gateway_state, &state));
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to publish Wi-Fi state: %s", esp_err_to_name(ret));
    }
}

esp_err_t wifi_init_sta_and_wait(wifi_runtime_ctx_t *ctx)
{
    if (!ctx) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!ctx->gateway_state || !ctx->wifi_service || !ctx->system_service) {
        return ESP_ERR_INVALID_STATE;
    }
#if CONFIG_GATEWAY_SELF_TEST_APP
    wifi_init_apply_default_ops_if_needed(ctx);
    if (ctx->ops.net_platform_services_init) {
        ctx->ops.net_platform_services_init(ctx);
    }
#else
    net_platform_services_init(ctx->wifi_service, ctx->system_service);
#endif
    wifi_state_store_update(ctx);
    esp_err_t ret = ESP_FAIL;
#if CONFIG_GATEWAY_SELF_TEST_APP
    if (ctx->ops.wifi_sta_connect_and_wait) {
        ret = ctx->ops.wifi_sta_connect_and_wait(ctx);
    }
#else
    ret = wifi_sta_connect_and_wait(ctx);
#endif
    if (ret == ESP_OK) {
        wifi_state_store_update(ctx);
        return ESP_OK;
    }

    ESP_LOGE(TAG, "Failed to connect to SSID:%s", ctx->active_ssid);
    ret = ESP_FAIL;
#if CONFIG_GATEWAY_SELF_TEST_APP
    if (ctx->ops.wifi_start_fallback_ap) {
        ret = ctx->ops.wifi_start_fallback_ap(ctx);
    }
#else
    ret = wifi_start_fallback_ap(ctx);
#endif
    wifi_state_store_update(ctx);
    return (ret == ESP_OK) ? ESP_OK : ESP_FAIL;
}

#if CONFIG_GATEWAY_SELF_TEST_APP
void wifi_init_set_ops_for_test(wifi_runtime_ctx_t *ctx, const wifi_init_ops_t *ops)
{
    if (!ctx || !ops) {
        return;
    }
    ctx->ops = *ops;
}

void wifi_init_reset_ops_for_test(wifi_runtime_ctx_t *ctx)
{
    if (!ctx) {
        return;
    }
    ctx->ops.net_platform_services_init = wifi_init_default_net_platform_services_init;
    ctx->ops.wifi_sta_connect_and_wait = wifi_init_default_sta_connect_and_wait;
    ctx->ops.wifi_start_fallback_ap = wifi_init_default_start_fallback_ap;
}
#endif
