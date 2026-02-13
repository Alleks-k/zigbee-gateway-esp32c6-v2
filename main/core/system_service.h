#pragma once

#include <stdint.h>
#include "esp_err.h"

typedef struct {
    esp_err_t wifi_err;
    esp_err_t devices_err;
    esp_err_t zigbee_storage_err;
    esp_err_t zigbee_fct_err;
} system_factory_reset_report_t;

void system_service_reboot(void);
esp_err_t system_service_schedule_reboot(uint32_t delay_ms);
esp_err_t system_service_factory_reset_and_reboot(uint32_t reboot_delay_ms);
esp_err_t system_service_get_last_factory_reset_report(system_factory_reset_report_t *out_report);
