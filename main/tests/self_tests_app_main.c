#include "self_tests.h"
#include "config_service.h"
#include "device_service.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "gateway_runtime_context.h"
#include "gateway_status_esp.h"
#include "gateway_device_zigbee_facade.h"
#include "gateway_wifi_system_facade.h"
#include "gateway_persistence_adapter.h"
#include "state_store.h"
#include "wifi_init.h"
#include "zigbee_service.h"
#include "device_service_lock_freertos_port.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "SELF_TEST_APP";

static uint64_t self_tests_now_ms(void)
{
    return (uint64_t)(esp_timer_get_time() / 1000);
}

static gateway_status_t self_tests_device_repo_load(void *ctx,
                                                    gateway_device_record_t *devices,
                                                    size_t max_devices,
                                                    int *device_count,
                                                    bool *loaded)
{
    (void)ctx;
    return gateway_persistence_devices_load(devices, max_devices, device_count, loaded);
}

static gateway_status_t self_tests_device_repo_save(void *ctx,
                                                    const gateway_device_record_t *devices,
                                                    size_t max_devices,
                                                    int device_count)
{
    (void)ctx;
    return gateway_persistence_devices_save(devices, max_devices, device_count);
}

static const device_service_repo_port_t s_self_tests_device_repo_port = {
    .load = self_tests_device_repo_load,
    .save = self_tests_device_repo_save,
    .ctx = NULL,
};

void app_main(void)
{
    device_service_handle_t device_service = NULL;
    gateway_state_handle_t gateway_state = NULL;
    zigbee_service_handle_t zigbee_service = NULL;
    gateway_wifi_system_handle_t wifi_system = NULL;
    gateway_wifi_system_init_params_t wifi_system_params = {0};
    zigbee_service_init_params_t zigbee_service_params = {0};
    device_service_init_params_t device_service_params = {
        .lock_port = NULL,
        .repo_port = &s_self_tests_device_repo_port,
        .notifier = NULL,
    };
    device_service_params.lock_port = device_service_lock_port_freertos();

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(gateway_status_to_esp_err(config_service_init_or_migrate()));
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_ERROR_CHECK(gateway_status_to_esp_err(device_service_create_with_params(&device_service_params, &device_service)));
    ESP_ERROR_CHECK(gateway_status_to_esp_err(gateway_state_create(&gateway_state)));
    ESP_ERROR_CHECK(gateway_status_to_esp_err(device_service_init(device_service)));
    ESP_ERROR_CHECK(gateway_status_to_esp_err(gateway_state_init(gateway_state)));
    gateway_state_set_now_ms_provider(self_tests_now_ms);
    zigbee_service_params.device_service = device_service;
    zigbee_service_params.gateway_state = gateway_state;
    zigbee_service_params.runtime_ops = NULL;
    ESP_ERROR_CHECK(zigbee_service_create(&zigbee_service_params, &zigbee_service));
    ESP_ERROR_CHECK(gateway_device_zigbee_bind_service(zigbee_service));
    wifi_system_params.gateway_state_handle = gateway_state;
    ESP_ERROR_CHECK(gateway_wifi_system_create(&wifi_system_params, &wifi_system));
    ESP_ERROR_CHECK(wifi_init_bind_state(gateway_state));
    (void)wifi_system;
    (void)zigbee_service;
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
