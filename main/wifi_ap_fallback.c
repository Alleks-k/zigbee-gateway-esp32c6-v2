#include "wifi_ap_fallback.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include <string.h>

static const char *TAG = "wifi_init";

#define FALLBACK_AP_CHANNEL   1
#define FALLBACK_AP_MAX_CONN  4
#define FALLBACK_AP_PASSWORD  "zigbeegw123"

esp_err_t wifi_start_fallback_ap(wifi_runtime_ctx_t *ctx)
{
    if (!ctx->ap_netif_created) {
        ctx->ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
        if (!ctx->ap_netif) {
            ctx->ap_netif = esp_netif_create_default_wifi_ap();
        }
        if (!ctx->ap_netif) {
            ESP_LOGE(TAG, "Failed to create default Wi-Fi AP netif");
            return ESP_FAIL;
        }
        ctx->ap_netif_created = true;
    }

    uint8_t ap_mac[6] = {0};
    char fallback_ssid[33];
    ESP_RETURN_ON_ERROR(esp_read_mac(ap_mac, ESP_MAC_WIFI_SOFTAP), TAG, "Failed to read SoftAP MAC");
    snprintf(fallback_ssid, sizeof(fallback_ssid), "ZigbeeGW-%02X%02X", ap_mac[4], ap_mac[5]);

    wifi_config_t ap_cfg = {
        .ap = {
            .channel = FALLBACK_AP_CHANNEL,
            .max_connection = FALLBACK_AP_MAX_CONN,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .required = false,
            },
        },
    };

    strlcpy((char *)ap_cfg.ap.ssid, fallback_ssid, sizeof(ap_cfg.ap.ssid));
    strlcpy((char *)ap_cfg.ap.password, FALLBACK_AP_PASSWORD, sizeof(ap_cfg.ap.password));
    ap_cfg.ap.ssid_len = strlen(fallback_ssid);

    esp_err_t stop_ret = esp_wifi_stop();
    if (stop_ret != ESP_OK && stop_ret != ESP_ERR_WIFI_NOT_INIT && stop_ret != ESP_ERR_WIFI_NOT_STARTED) {
        ESP_LOGW(TAG, "esp_wifi_stop returned: %s", esp_err_to_name(stop_ret));
    }

    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_AP), TAG, "Failed to set AP mode");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg), TAG, "Failed to configure AP");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "Failed to start fallback AP");
    ESP_RETURN_ON_ERROR(esp_wifi_set_protocol(WIFI_IF_AP, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N),
                        TAG, "Failed to set AP protocol");
    ESP_RETURN_ON_ERROR(esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW_HT20),
                        TAG, "Failed to set AP bandwidth");
    ESP_RETURN_ON_ERROR(esp_wifi_set_ps(WIFI_PS_NONE), TAG, "Failed to disable Wi-Fi power save");
    ctx->fallback_ap_active = true;

    esp_netif_ip_info_t ap_ip;
    if (ctx->ap_netif && esp_netif_get_ip_info(ctx->ap_netif, &ap_ip) == ESP_OK) {
        ESP_LOGW(TAG, "Fallback AP started: SSID=%s, PASS=%s, IP=" IPSTR,
                 fallback_ssid, FALLBACK_AP_PASSWORD, IP2STR(&ap_ip.ip));
    } else {
        ESP_LOGW(TAG, "Fallback AP started: SSID=%s, PASS=%s, IP=192.168.4.1",
                 fallback_ssid, FALLBACK_AP_PASSWORD);
    }

    return ESP_OK;
}
