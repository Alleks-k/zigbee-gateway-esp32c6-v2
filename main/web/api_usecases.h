#pragma once

#include "esp_err.h"
#include "api_contracts.h"
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
