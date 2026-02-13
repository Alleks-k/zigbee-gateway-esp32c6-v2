#pragma once

#include <stdint.h>
#include "esp_err.h"

void system_service_reboot(void);
esp_err_t system_service_schedule_reboot(uint32_t delay_ms);
esp_err_t system_service_factory_reset_and_reboot(uint32_t reboot_delay_ms);

