#pragma once

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"
#include "device_manager.h"

typedef struct {
    uint32_t pan_id;
    uint32_t channel;
    uint32_t short_addr;
} zgw_network_status_t;

typedef struct {
    char ssid[33];
    int8_t rssi;
    uint8_t auth;
} zgw_wifi_ap_info_t;

esp_err_t zgw_service_get_network_status(zgw_network_status_t *out);
esp_err_t zgw_service_permit_join(uint16_t seconds);
esp_err_t zgw_service_send_on_off(uint16_t short_addr, uint8_t endpoint, uint8_t on_off);
int zgw_service_get_devices_snapshot(zb_device_t *out, size_t max_items);
esp_err_t zgw_service_delete_device(uint16_t short_addr);
esp_err_t zgw_service_rename_device(uint16_t short_addr, const char *name);
esp_err_t zgw_service_wifi_scan(zgw_wifi_ap_info_t **out_list, size_t *out_count);
void zgw_service_wifi_scan_free(zgw_wifi_ap_info_t *list);
esp_err_t zgw_service_wifi_save_credentials(const char *ssid, const char *password);
void zgw_service_reboot(void);
esp_err_t zgw_service_schedule_reboot(uint32_t delay_ms);
