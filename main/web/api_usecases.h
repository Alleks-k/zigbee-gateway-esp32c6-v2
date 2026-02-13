#pragma once

#include "esp_err.h"
#include "api_contracts.h"
#include "zigbee_service.h"
#include "wifi_service.h"
#include "system_service.h"
#include <stdint.h>

typedef struct {
    esp_err_t (*send_on_off)(uint16_t short_addr, uint8_t endpoint, uint8_t on_off);
    esp_err_t (*wifi_save_credentials)(const char *ssid, const char *password);
    esp_err_t (*schedule_reboot)(uint32_t delay_ms);
    esp_err_t (*factory_reset_and_reboot)(uint32_t reboot_delay_ms);
} api_service_ops_t;

void api_usecases_set_service_ops(const api_service_ops_t *ops);

esp_err_t api_usecase_control(const api_control_request_t *in);
esp_err_t api_usecase_wifi_save(const api_wifi_save_request_t *in);
esp_err_t api_usecase_factory_reset(void);
esp_err_t api_usecase_get_network_status(zigbee_network_status_t *out_status);
int api_usecase_get_devices_snapshot(zb_device_t *out_devices, int max_devices);
esp_err_t api_usecase_permit_join(uint8_t duration_seconds);
esp_err_t api_usecase_delete_device(uint16_t short_addr);
esp_err_t api_usecase_rename_device(uint16_t short_addr, const char *name);
esp_err_t api_usecase_wifi_scan(wifi_ap_info_t **out_list, size_t *out_count);
void api_usecase_wifi_scan_free(wifi_ap_info_t *list);
esp_err_t api_usecase_schedule_reboot(uint32_t delay_ms);
esp_err_t api_usecase_get_factory_reset_report(system_factory_reset_report_t *out_report);
