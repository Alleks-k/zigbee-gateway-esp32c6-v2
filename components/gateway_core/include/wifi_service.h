#pragma once

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

typedef struct {
    char ssid[33];
    int8_t rssi;
    uint8_t auth;
} wifi_ap_info_t;

typedef esp_err_t (*wifi_service_scan_impl_t)(wifi_ap_info_t **out_list, size_t *out_count);
typedef void (*wifi_service_scan_free_impl_t)(wifi_ap_info_t *list);

void wifi_service_register_scan_impl(wifi_service_scan_impl_t scan_impl, wifi_service_scan_free_impl_t free_impl);

esp_err_t wifi_service_scan(wifi_ap_info_t **out_list, size_t *out_count);
void wifi_service_scan_free(wifi_ap_info_t *list);
esp_err_t wifi_service_save_credentials(const char *ssid, const char *password);
