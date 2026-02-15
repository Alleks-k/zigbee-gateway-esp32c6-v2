#pragma once

#include <stdint.h>
#include "esp_err.h"

/* Project-wide limits and shared DTOs used across core/web/storage layers. */
#define GATEWAY_WIFI_SSID_MAX_LEN 32
#define GATEWAY_WIFI_PASSWORD_MAX_LEN 64
#define GATEWAY_DEVICE_NAME_MAX_LEN 31

#ifdef CONFIG_GATEWAY_MAX_DEVICES
#define GATEWAY_MAX_DEVICES CONFIG_GATEWAY_MAX_DEVICES
#else
#define GATEWAY_MAX_DEVICES 10
#endif

/* Canonical 64-bit IEEE address type without Zigbee SDK coupling. */
typedef uint8_t gateway_ieee_addr_t[8];

typedef struct {
    char ssid[GATEWAY_WIFI_SSID_MAX_LEN + 1];
    char password[GATEWAY_WIFI_PASSWORD_MAX_LEN + 1];
} gateway_wifi_credentials_t;

typedef struct {
    uint16_t short_addr;
    gateway_ieee_addr_t ieee_addr;
    char name[GATEWAY_DEVICE_NAME_MAX_LEN + 1];
} gateway_device_record_t;

typedef struct {
    esp_err_t wifi_err;
    esp_err_t devices_err;
    esp_err_t zigbee_storage_err;
    esp_err_t zigbee_fct_err;
} gateway_factory_reset_report_t;
