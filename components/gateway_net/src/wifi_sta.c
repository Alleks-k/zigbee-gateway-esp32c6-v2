#include "wifi_sta.h"
#include "wifi_credentials.h"
#include "wifi_settings.h"
#include "wifi_init.h"
#include "settings_manager.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include <string.h>

static const char *TAG = "wifi_init";

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static const char *wifi_reason_to_str(uint8_t reason)
{
    switch (reason) {
        case WIFI_REASON_AUTH_EXPIRE: return "AUTH_EXPIRE";
        case WIFI_REASON_AUTH_FAIL: return "AUTH_FAIL";
        case WIFI_REASON_ASSOC_FAIL: return "ASSOC_FAIL";
        case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT: return "4WAY_HANDSHAKE_TIMEOUT";
        case WIFI_REASON_HANDSHAKE_TIMEOUT: return "HANDSHAKE_TIMEOUT";
        case WIFI_REASON_CONNECTION_FAIL: return "CONNECTION_FAIL";
        case WIFI_REASON_NO_AP_FOUND: return "NO_AP_FOUND";
        case WIFI_REASON_NO_AP_FOUND_W_COMPATIBLE_SECURITY: return "NO_AP_COMPATIBLE_SECURITY";
        case WIFI_REASON_NO_AP_FOUND_IN_AUTHMODE_THRESHOLD: return "NO_AP_AUTHMODE_THRESHOLD";
        case WIFI_REASON_NO_AP_FOUND_IN_RSSI_THRESHOLD: return "NO_AP_RSSI_THRESHOLD";
        case WIFI_REASON_BEACON_TIMEOUT: return "BEACON_TIMEOUT";
        default: return "UNKNOWN";
    }
}

static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    wifi_runtime_ctx_t *ctx = (wifi_runtime_ctx_t *)arg;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ctx->sta_connected = false;
        wifi_state_store_update();
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ctx->sta_connected = false;
        wifi_state_store_update();
        if (ctx->fallback_ap_active) {
            return;
        }
        wifi_event_sta_disconnected_t *disconn = (wifi_event_sta_disconnected_t *)event_data;
        if (ctx->retry_num < WIFI_STA_MAX_RETRY) {
            esp_wifi_connect();
            ctx->retry_num++;
            ESP_LOGW(TAG, "retry to connect to AP (attempt %d/%d), reason=%d (%s)",
                     ctx->retry_num, WIFI_STA_MAX_RETRY, disconn->reason, wifi_reason_to_str(disconn->reason));
        } else {
            if (ctx->wifi_event_group) {
                xEventGroupSetBits(ctx->wifi_event_group, WIFI_FAIL_BIT);
            }
        }
        ESP_LOGW(TAG, "connect to AP failed, reason=%d (%s)",
                 disconn->reason, wifi_reason_to_str(disconn->reason));
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t *evt = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAG, "Fallback AP: station connected, aid=%d, mac=%02x:%02x:%02x:%02x:%02x:%02x",
                 evt->aid, evt->mac[0], evt->mac[1], evt->mac[2], evt->mac[3], evt->mac[4], evt->mac[5]);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t *evt = (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI(TAG, "Fallback AP: station disconnected, aid=%d, mac=%02x:%02x:%02x:%02x:%02x:%02x",
                 evt->aid, evt->mac[0], evt->mac[1], evt->mac[2], evt->mac[3], evt->mac[4], evt->mac[5]);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        ctx->retry_num = 0;
        ctx->sta_connected = true;
        wifi_state_store_update();
        if (ctx->wifi_event_group) {
            xEventGroupSetBits(ctx->wifi_event_group, WIFI_CONNECTED_BIT);
        }
    }
}

static void load_wifi_credentials(wifi_runtime_ctx_t *ctx, wifi_config_t *wifi_config)
{
    bool loaded_from_nvs = false;
    esp_err_t err = settings_manager_load_wifi_credentials(
        (char *)wifi_config->sta.ssid, sizeof(wifi_config->sta.ssid),
        (char *)wifi_config->sta.password, sizeof(wifi_config->sta.password),
        &loaded_from_nvs);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to load Wi-Fi settings from storage: %s", esp_err_to_name(err));
    }
    if (loaded_from_nvs) {
        ESP_LOGI(TAG, "Loaded Wi-Fi settings from NVS");
    }

    if (!loaded_from_nvs) {
        ESP_LOGI(TAG, "Using default credentials from wifi_credentials.h");
        strlcpy((char *)wifi_config->sta.ssid, MY_WIFI_SSID, sizeof(wifi_config->sta.ssid));
        strlcpy((char *)wifi_config->sta.password, MY_WIFI_PASSWORD, sizeof(wifi_config->sta.password));
    }

    ctx->loaded_from_nvs = loaded_from_nvs;
    strlcpy(ctx->active_ssid, (char *)wifi_config->sta.ssid, sizeof(ctx->active_ssid));
    wifi_state_store_update();
}

esp_err_t wifi_sta_connect_and_wait(wifi_runtime_ctx_t *ctx)
{
    ctx->fallback_ap_active = false;
    ctx->sta_connected = false;
    ctx->retry_num = 0;
    ctx->instance_any_id = NULL;
    ctx->instance_got_ip = NULL;
    wifi_state_store_update();

    ctx->wifi_event_group = xEventGroupCreate();
    ESP_RETURN_ON_FALSE(ctx->wifi_event_group != NULL, ESP_ERR_NO_MEM, TAG, "Failed to create Wi-Fi event group");

    esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (!sta_netif) {
        sta_netif = esp_netif_create_default_wifi_sta();
    }
    ESP_RETURN_ON_FALSE(sta_netif != NULL, ESP_FAIL, TAG, "Failed to create default Wi-Fi STA netif");

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK && ret != ESP_ERR_WIFI_INIT_STATE) {
        ESP_LOGE(TAG, "esp_wifi_init failed: %s", esp_err_to_name(ret));
        vEventGroupDelete(ctx->wifi_event_group);
        ctx->wifi_event_group = NULL;
        return ret;
    }

    ret = esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, ctx, &ctx->instance_any_id);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "register WIFI_EVENT handler failed: %s", esp_err_to_name(ret));
        vEventGroupDelete(ctx->wifi_event_group);
        ctx->wifi_event_group = NULL;
        return ret;
    }

    ret = esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, ctx, &ctx->instance_got_ip);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "register IP_EVENT handler failed: %s", esp_err_to_name(ret));
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, ctx->instance_any_id);
        ctx->instance_any_id = NULL;
        vEventGroupDelete(ctx->wifi_event_group);
        ctx->wifi_event_group = NULL;
        return ret;
    }

    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = WIFI_STA_PMF_CAPABLE,
                .required = WIFI_STA_PMF_REQUIRED,
            },
        },
    };
    wifi_config.sta.threshold.authmode = WIFI_STA_AUTHMODE_THRESHOLD;

    load_wifi_credentials(ctx, &wifi_config);

    ESP_GOTO_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), fail, TAG, "Failed to set STA mode");
    ESP_GOTO_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &wifi_config), fail, TAG, "Failed to set STA config");
    ESP_GOTO_ON_ERROR(esp_wifi_start(), fail, TAG, "Failed to start Wi-Fi STA");

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    EventBits_t bits = xEventGroupWaitBits(
        ctx->wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE,
        pdFALSE,
        pdMS_TO_TICKS(WIFI_STA_CONNECT_TIMEOUT_MS));

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s", ctx->active_ssid);
        esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
        esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT20);
        ret = ESP_OK;
    } else {
        if ((bits & WIFI_FAIL_BIT) == 0) {
            ESP_LOGE(TAG, "Timed out waiting for Wi-Fi connection (%d ms)", WIFI_STA_CONNECT_TIMEOUT_MS);
        }
        ret = ESP_FAIL;
    }

    if (ctx->instance_got_ip) {
        esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, ctx->instance_got_ip);
        ctx->instance_got_ip = NULL;
    }
    if (ctx->instance_any_id) {
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, ctx->instance_any_id);
        ctx->instance_any_id = NULL;
    }
    if (ctx->wifi_event_group) {
        vEventGroupDelete(ctx->wifi_event_group);
        ctx->wifi_event_group = NULL;
    }

    return ret;

fail:
    if (ctx->instance_got_ip) {
        esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, ctx->instance_got_ip);
        ctx->instance_got_ip = NULL;
    }
    if (ctx->instance_any_id) {
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, ctx->instance_any_id);
        ctx->instance_any_id = NULL;
    }
    if (ctx->wifi_event_group) {
        vEventGroupDelete(ctx->wifi_event_group);
        ctx->wifi_event_group = NULL;
    }
    return ret;
}
