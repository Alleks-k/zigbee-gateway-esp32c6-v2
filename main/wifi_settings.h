#pragma once

#include <stdbool.h>
#include "esp_wifi_types_generic.h"

/* STA connection policy */
#define WIFI_STA_MAX_RETRY                    10
#define WIFI_STA_CONNECT_TIMEOUT_MS           30000
#define WIFI_STA_AUTHMODE_THRESHOLD           WIFI_AUTH_WPA2_PSK
#define WIFI_STA_PMF_CAPABLE                  true
#define WIFI_STA_PMF_REQUIRED                 false

/* Fallback AP policy */
#define WIFI_AP_FALLBACK_SSID_PREFIX          "ZigbeeGW-"
#define WIFI_AP_FALLBACK_CHANNEL              1
#define WIFI_AP_FALLBACK_MAX_CONNECTIONS      4
#define WIFI_AP_FALLBACK_PASSWORD             "zigbeegw123"
#define WIFI_AP_FALLBACK_AUTHMODE             WIFI_AUTH_WPA2_PSK
#define WIFI_AP_FALLBACK_DISABLE_POWER_SAVE   true
