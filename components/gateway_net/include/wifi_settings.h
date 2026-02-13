#pragma once

/*
 * Tracked defaults for Wi-Fi behavior.
 * Optional local overrides can be placed into:
 *   main/net/wifi_settings_local.h
 *
 * File wifi_settings_local.h is gitignored.
 */

#if __has_include("wifi_settings_local.h")
#include "wifi_settings_local.h"
#endif

/* STA connection policy */
#ifndef WIFI_STA_MAX_RETRY
#define WIFI_STA_MAX_RETRY                    10
#endif
#ifndef WIFI_STA_CONNECT_TIMEOUT_MS
#define WIFI_STA_CONNECT_TIMEOUT_MS           30000
#endif
#ifndef WIFI_STA_AUTHMODE_THRESHOLD
#define WIFI_STA_AUTHMODE_THRESHOLD           WIFI_AUTH_WPA2_PSK
#endif
#ifndef WIFI_STA_PMF_CAPABLE
#define WIFI_STA_PMF_CAPABLE                  true
#endif
#ifndef WIFI_STA_PMF_REQUIRED
#define WIFI_STA_PMF_REQUIRED                 false
#endif

/* Fallback AP policy */
#ifndef WIFI_AP_FALLBACK_SSID_PREFIX
#define WIFI_AP_FALLBACK_SSID_PREFIX          "ZigbeeGW-"
#endif
#ifndef WIFI_AP_FALLBACK_CHANNEL
#define WIFI_AP_FALLBACK_CHANNEL              1
#endif
#ifndef WIFI_AP_FALLBACK_MAX_CONNECTIONS
#define WIFI_AP_FALLBACK_MAX_CONNECTIONS      4
#endif
#ifndef WIFI_AP_FALLBACK_PASSWORD
#define WIFI_AP_FALLBACK_PASSWORD             "Zigbee-1234"
#endif
#ifndef WIFI_AP_FALLBACK_AUTHMODE
#define WIFI_AP_FALLBACK_AUTHMODE             WIFI_AUTH_WPA2_PSK
#endif
#ifndef WIFI_AP_FALLBACK_DISABLE_POWER_SAVE
#define WIFI_AP_FALLBACK_DISABLE_POWER_SAVE   true
#endif
