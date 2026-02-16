#include "self_tests.h"
#include "device_service.h"
#include "config_service.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "SELF_TEST_APP";

void app_main(void)
{
    device_service_handle_t device_service = NULL;

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(config_service_init_or_migrate());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_ERROR_CHECK(device_service_get_default(&device_service));
    ESP_ERROR_CHECK(device_service_init(device_service));
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
