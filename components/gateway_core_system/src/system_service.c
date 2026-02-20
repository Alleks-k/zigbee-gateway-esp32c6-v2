#include "system_service.h"
#include "config_service.h"
#include "gateway_status_esp.h"
#include "esp_system.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "SYSTEM_SERVICE";

struct system_service {
    system_service_telemetry_impl_t telemetry_impl;
    SemaphoreHandle_t reboot_mutex;
    bool reboot_scheduled;
    uint32_t reboot_schedule_count;
};

static esp_err_t ensure_reboot_mutex(system_service_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    if (handle->reboot_mutex) {
        return ESP_OK;
    }
    handle->reboot_mutex = xSemaphoreCreateMutex();
    if (!handle->reboot_mutex) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t system_service_create(system_service_handle_t *out_handle)
{
    if (!out_handle) {
        return ESP_ERR_INVALID_ARG;
    }
    system_service_handle_t handle = (system_service_handle_t)calloc(1, sizeof(*handle));
    if (!handle) {
        return ESP_ERR_NO_MEM;
    }
    *out_handle = handle;
    return ESP_OK;
}

void system_service_destroy(system_service_handle_t handle)
{
    if (!handle) {
        return;
    }
    if (handle->reboot_mutex) {
        vSemaphoreDelete(handle->reboot_mutex);
        handle->reboot_mutex = NULL;
    }
    free(handle);
}

void system_service_register_telemetry_impl(system_service_handle_t handle, system_service_telemetry_impl_t impl)
{
    if (!handle) {
        return;
    }
    handle->telemetry_impl = impl;
}

static void reboot_task(void *arg)
{
    uint32_t delay_ms = (uint32_t)(uintptr_t)arg;
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
    esp_restart();
}

void system_service_reboot(system_service_handle_t handle)
{
    if (!handle) {
        return;
    }
    ESP_LOGI(TAG, "Reboot requested by system service");
    esp_restart();
}

esp_err_t system_service_schedule_reboot(system_service_handle_t handle, uint32_t delay_ms)
{
    esp_err_t lock_ret = ensure_reboot_mutex(handle);
    if (lock_ret != ESP_OK) {
        return lock_ret;
    }

    xSemaphoreTake(handle->reboot_mutex, portMAX_DELAY);
    if (handle->reboot_scheduled) {
        xSemaphoreGive(handle->reboot_mutex);
        ESP_LOGW(TAG, "Reboot already scheduled, skipping duplicate request");
        return ESP_OK;
    }
    handle->reboot_scheduled = true;
    xSemaphoreGive(handle->reboot_mutex);

    BaseType_t ok = xTaskCreate(reboot_task, "zgw_reboot", 2048, (void *)(uintptr_t)delay_ms, 5, NULL);
    if (ok != pdPASS) {
        xSemaphoreTake(handle->reboot_mutex, portMAX_DELAY);
        handle->reboot_scheduled = false;
        xSemaphoreGive(handle->reboot_mutex);
        return ESP_ERR_NO_MEM;
    }
    xSemaphoreTake(handle->reboot_mutex, portMAX_DELAY);
    handle->reboot_schedule_count++;
    xSemaphoreGive(handle->reboot_mutex);
    return ESP_OK;
}

esp_err_t system_service_factory_reset_and_reboot(system_service_handle_t handle, uint32_t reboot_delay_ms)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = gateway_status_to_esp_err(config_service_factory_reset());
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Factory reset failed: %s", esp_err_to_name(err));
        return err;
    }
    return system_service_schedule_reboot(handle, reboot_delay_ms);
}

esp_err_t system_service_get_last_factory_reset_report(system_service_handle_t handle, system_factory_reset_report_t *out_report)
{
    if (!handle || !out_report) {
        return ESP_ERR_INVALID_ARG;
    }

    config_service_factory_reset_report_t report = {0};
    esp_err_t err = gateway_status_to_esp_err(config_service_get_last_factory_reset_report(&report));
    if (err != ESP_OK) {
        return err;
    }

    out_report->wifi_err = gateway_status_to_esp_err(report.wifi_err);
    out_report->devices_err = gateway_status_to_esp_err(report.devices_err);
    out_report->zigbee_storage_err = gateway_status_to_esp_err(report.zigbee_storage_err);
    out_report->zigbee_fct_err = gateway_status_to_esp_err(report.zigbee_fct_err);
    return ESP_OK;
}

esp_err_t system_service_collect_telemetry(system_service_handle_t handle, system_telemetry_t *out)
{
    if (!handle || !out) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(out, 0, sizeof(*out));
    if (!handle->telemetry_impl) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    return handle->telemetry_impl(out);
}

#if CONFIG_GATEWAY_SELF_TEST_APP
bool system_service_is_reboot_scheduled_for_test(system_service_handle_t handle)
{
    if (ensure_reboot_mutex(handle) != ESP_OK) {
        return false;
    }
    xSemaphoreTake(handle->reboot_mutex, portMAX_DELAY);
    bool scheduled = handle->reboot_scheduled;
    xSemaphoreGive(handle->reboot_mutex);
    return scheduled;
}

uint32_t system_service_get_reboot_schedule_count_for_test(system_service_handle_t handle)
{
    if (ensure_reboot_mutex(handle) != ESP_OK) {
        return 0;
    }
    xSemaphoreTake(handle->reboot_mutex, portMAX_DELAY);
    uint32_t count = handle->reboot_schedule_count;
    xSemaphoreGive(handle->reboot_mutex);
    return count;
}

void system_service_reset_reboot_singleflight_for_test(system_service_handle_t handle)
{
    if (ensure_reboot_mutex(handle) != ESP_OK) {
        return;
    }
    xSemaphoreTake(handle->reboot_mutex, portMAX_DELAY);
    handle->reboot_scheduled = false;
    handle->reboot_schedule_count = 0;
    xSemaphoreGive(handle->reboot_mutex);
}
#endif
