#include "self_tests.h"
#include "config_service.h"
#include "device_service.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "gateway_runtime_context.h"
#include "gateway_status_esp.h"
#include "gateway_wifi_system_facade.h"
#include "state_store.h"
#include "wifi_init.h"
#include "zigbee_service.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "SELF_TEST_APP";

static uint64_t self_tests_now_ms(void)
{
    return (uint64_t)(esp_timer_get_time() / 1000);
}

void app_main(void)
{
    device_service_handle_t device_service = NULL;
    gateway_state_handle_t gateway_state = NULL;
    gateway_runtime_context_t runtime_ctx = {0};
    gateway_wifi_system_init_params_t wifi_system_params = {0};

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(gateway_status_to_esp_err(config_service_init_or_migrate()));
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_ERROR_CHECK(gateway_status_to_esp_err(device_service_create(&device_service)));
    ESP_ERROR_CHECK(gateway_status_to_esp_err(gateway_state_create(&gateway_state)));
    ESP_ERROR_CHECK(gateway_status_to_esp_err(device_service_init(device_service)));
    ESP_ERROR_CHECK(gateway_status_to_esp_err(gateway_state_init(gateway_state)));
    gateway_state_set_now_ms_provider(self_tests_now_ms);
    runtime_ctx.device_service = device_service;
    runtime_ctx.gateway_state = gateway_state;
    ESP_ERROR_CHECK(zigbee_service_bind_context(&runtime_ctx));
    wifi_system_params.gateway_state_handle = gateway_state;
    ESP_ERROR_CHECK(gateway_wifi_system_init(&wifi_system_params));
    ESP_ERROR_CHECK(wifi_init_bind_state(gateway_state));
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
