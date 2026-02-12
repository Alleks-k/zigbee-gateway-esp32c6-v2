#include "wifi_sta.h"
#include "wifi_credentials.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include <assert.h>
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
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (ctx->fallback_ap_active) {
            return;
        }
        wifi_event_sta_disconnected_t *disconn = (wifi_event_sta_disconnected_t *)event_data;
        if (ctx->retry_num < 10) {
            esp_wifi_connect();
            ctx->retry_num++;
            ESP_LOGW(TAG, "retry to connect to AP (attempt %d/10), reason=%d (%s)",
                     ctx->retry_num, disconn->reason, wifi_reason_to_str(disconn->reason));
        } else {
            xEventGroupSetBits(ctx->wifi_event_group, WIFI_FAIL_BIT);
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
        xEventGroupSetBits(ctx->wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void load_wifi_credentials(wifi_runtime_ctx_t *ctx, wifi_config_t *wifi_config)
{
    nvs_handle_t my_handle;
    bool loaded_from_nvs = false;
    if (nvs_open("storage", NVS_READONLY, &my_handle) == ESP_OK) {
        size_t ssid_len = sizeof(wifi_config->sta.ssid);
        size_t pass_len = sizeof(wifi_config->sta.password);
        if (nvs_get_str(my_handle, "wifi_ssid", (char *)wifi_config->sta.ssid, &ssid_len) == ESP_OK &&
            nvs_get_str(my_handle, "wifi_pass", (char *)wifi_config->sta.password, &pass_len) == ESP_OK) {
            loaded_from_nvs = true;
            ESP_LOGI(TAG, "Loaded Wi-Fi settings from NVS");
        }
        nvs_close(my_handle);
    }

    if (!loaded_from_nvs) {
        ESP_LOGI(TAG, "Using default credentials from wifi_credentials.h");
        strlcpy((char *)wifi_config->sta.ssid, MY_WIFI_SSID, sizeof(wifi_config->sta.ssid));
        strlcpy((char *)wifi_config->sta.password, MY_WIFI_PASSWORD, sizeof(wifi_config->sta.password));
    }

    ctx->loaded_from_nvs = loaded_from_nvs;
    strlcpy(ctx->active_ssid, (char *)wifi_config->sta.ssid, sizeof(ctx->active_ssid));
}

esp_err_t wifi_sta_connect_and_wait(wifi_runtime_ctx_t *ctx)
{
    ctx->fallback_ap_active = false;
    ctx->retry_num = 0;

    ctx->wifi_event_group = xEventGroupCreate();
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, ctx, &ctx->instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, ctx, &ctx->instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false,
            },
        },
    };

    load_wifi_credentials(ctx, &wifi_config);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    EventBits_t bits = xEventGroupWaitBits(
        ctx->wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s", ctx->active_ssid);
        esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
        esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT20);
        return ESP_OK;
    }
    return ESP_FAIL;
}
