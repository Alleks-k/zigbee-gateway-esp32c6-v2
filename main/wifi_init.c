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
#include "esp_netif.h"
#include "nvs.h"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static const char *TAG = "wifi_init";
static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < 10) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
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
        strncpy((char *)wifi_config.sta.ssid, MY_WIFI_SSID, sizeof(wifi_config.sta.ssid));
        strncpy((char *)wifi_config.sta.password, MY_WIFI_PASSWORD, sizeof(wifi_config.sta.password));
    }

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
        ESP_LOGI(TAG, "connected to ap SSID:%s", MY_WIFI_SSID);
        esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
        esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT20);
        return ESP_OK;
    } else {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s", MY_WIFI_SSID);
        return ESP_FAIL;
    }
}