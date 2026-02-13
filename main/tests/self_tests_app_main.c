#include "self_tests.h"
#include "device_manager.h"
#include "settings_manager.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "SELF_TEST_APP";

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(settings_manager_init_or_migrate());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    device_manager_init();
    int failures = zgw_run_self_tests();
    if (failures > 0) {
        ESP_LOGE(TAG, "Self-tests failed: %d", failures);
    } else {
        ESP_LOGI(TAG, "Self-tests passed");
    }

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
