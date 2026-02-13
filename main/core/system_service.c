#include "system_service.h"
#include "settings_manager.h"
#include "esp_system.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdint.h>

static const char *TAG = "SYSTEM_SERVICE";

static void reboot_task(void *arg)
{
    uint32_t delay_ms = (uint32_t)(uintptr_t)arg;
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
    esp_restart();
}

void system_service_reboot(void)
{
    ESP_LOGI(TAG, "Reboot requested by system service");
    esp_restart();
}

esp_err_t system_service_schedule_reboot(uint32_t delay_ms)
{
    BaseType_t ok = xTaskCreate(reboot_task, "zgw_reboot", 2048, (void *)(uintptr_t)delay_ms, 5, NULL);
    if (ok != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t system_service_factory_reset_and_reboot(uint32_t reboot_delay_ms)
{
    esp_err_t err = settings_manager_factory_reset();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Factory reset failed: %s", esp_err_to_name(err));
        return err;
    }
    return system_service_schedule_reboot(reboot_delay_ms);
}
