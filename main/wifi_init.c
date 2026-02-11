#include "wifi_init.h"
#include "wifi_credentials.h" // <--- Підключаємо файл з паролями
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_netif.h"
#include "esp_mac.h"
#include "nvs.h"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static const char *TAG = "wifi_init";
static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;
static char s_active_ssid[33] = {0};
static bool s_ap_netif_created = false;
static esp_netif_t *s_ap_netif = NULL;
static bool s_fallback_ap_active = false;
static bool s_loaded_from_nvs = false;

#define FALLBACK_AP_CHANNEL   1
#define FALLBACK_AP_MAX_CONN  4
#define FALLBACK_AP_PASSWORD  "zigbeegw123"

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

static esp_err_t start_fallback_ap(void)
{
    if (!s_ap_netif_created) {
        s_ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
        if (!s_ap_netif) {
            s_ap_netif = esp_netif_create_default_wifi_ap();
        }
        if (!s_ap_netif) {
            ESP_LOGE(TAG, "Failed to create default Wi-Fi AP netif");
            return ESP_FAIL;
        }
        s_ap_netif_created = true;
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

    /* Start AP in pure AP mode after STA failure to keep it stable for setup. */
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
    s_fallback_ap_active = true;

    esp_netif_ip_info_t ap_ip;
    if (s_ap_netif && esp_netif_get_ip_info(s_ap_netif, &ap_ip) == ESP_OK) {
        ESP_LOGW(TAG, "Fallback AP started: SSID=%s, PASS=%s, IP=" IPSTR,
                 fallback_ssid, FALLBACK_AP_PASSWORD, IP2STR(&ap_ip.ip));
    } else {
        ESP_LOGW(TAG, "Fallback AP started: SSID=%s, PASS=%s, IP=192.168.4.1",
                 fallback_ssid, FALLBACK_AP_PASSWORD);
    }

    return ESP_OK;
}

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_fallback_ap_active) {
            return;
        }
        wifi_event_sta_disconnected_t *disconn = (wifi_event_sta_disconnected_t *)event_data;
        if (s_retry_num < 10) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGW(TAG, "retry to connect to AP (attempt %d/10), reason=%d (%s)",
                     s_retry_num, disconn->reason, wifi_reason_to_str(disconn->reason));
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
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
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

esp_err_t wifi_init_sta_and_wait(void)
{
    s_wifi_event_group = xEventGroupCreate();

    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };

    /* Спроба завантажити налаштування з NVS */
    nvs_handle_t my_handle;
    bool loaded_from_nvs = false;
    if (nvs_open("storage", NVS_READONLY, &my_handle) == ESP_OK) {
        size_t ssid_len = sizeof(wifi_config.sta.ssid);
        size_t pass_len = sizeof(wifi_config.sta.password);
        if (nvs_get_str(my_handle, "wifi_ssid", (char *)wifi_config.sta.ssid, &ssid_len) == ESP_OK &&
            nvs_get_str(my_handle, "wifi_pass", (char *)wifi_config.sta.password, &pass_len) == ESP_OK) {
            loaded_from_nvs = true;
            ESP_LOGI(TAG, "Loaded Wi-Fi settings from NVS");
        }
        nvs_close(my_handle);
    }

    if (!loaded_from_nvs) {
        ESP_LOGI(TAG, "Using default credentials from wifi_credentials.h");
        strlcpy((char *)wifi_config.sta.ssid, MY_WIFI_SSID, sizeof(wifi_config.sta.ssid));
        strlcpy((char *)wifi_config.sta.password, MY_WIFI_PASSWORD, sizeof(wifi_config.sta.password));
    }
    s_loaded_from_nvs = loaded_from_nvs;
    strlcpy(s_active_ssid, (char *)wifi_config.sta.ssid, sizeof(s_active_ssid));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s", s_active_ssid);
        esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
        esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT20);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to connect to SSID:%s", s_active_ssid);
        esp_err_t ap_ret = start_fallback_ap();
        if (ap_ret == ESP_OK) {
            return ESP_OK;
        }
        return ESP_FAIL;
    }
}

bool wifi_is_fallback_ap_active(void)
{
    return s_fallback_ap_active;
}

bool wifi_loaded_credentials_from_nvs(void)
{
    return s_loaded_from_nvs;
}
