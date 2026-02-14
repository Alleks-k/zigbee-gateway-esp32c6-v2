#include "system_service.h"
#include "settings_manager.h"
#include "esp_system.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <stdint.h>
#include <string.h>

static const char *TAG = "SYSTEM_SERVICE";

static system_service_telemetry_impl_t s_telemetry_impl = NULL;
static SemaphoreHandle_t s_reboot_mutex = NULL;
static bool s_reboot_scheduled = false;
static uint32_t s_reboot_schedule_count = 0;

static esp_err_t ensure_reboot_mutex(void)
{
    if (s_reboot_mutex) {
        return ESP_OK;
    }
    s_reboot_mutex = xSemaphoreCreateMutex();
    if (!s_reboot_mutex) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

void system_service_register_telemetry_impl(system_service_telemetry_impl_t impl)
{
    s_telemetry_impl = impl;
}

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
    esp_err_t lock_ret = ensure_reboot_mutex();
    if (lock_ret != ESP_OK) {
        return lock_ret;
    }

    xSemaphoreTake(s_reboot_mutex, portMAX_DELAY);
    if (s_reboot_scheduled) {
        xSemaphoreGive(s_reboot_mutex);
        ESP_LOGW(TAG, "Reboot already scheduled, skipping duplicate request");
        return ESP_OK;
    }
    s_reboot_scheduled = true;
    xSemaphoreGive(s_reboot_mutex);

    BaseType_t ok = xTaskCreate(reboot_task, "zgw_reboot", 2048, (void *)(uintptr_t)delay_ms, 5, NULL);
    if (ok != pdPASS) {
        xSemaphoreTake(s_reboot_mutex, portMAX_DELAY);
        s_reboot_scheduled = false;
        xSemaphoreGive(s_reboot_mutex);
        return ESP_ERR_NO_MEM;
    }
    xSemaphoreTake(s_reboot_mutex, portMAX_DELAY);
    s_reboot_schedule_count++;
    xSemaphoreGive(s_reboot_mutex);
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

esp_err_t system_service_get_last_factory_reset_report(system_factory_reset_report_t *out_report)
{
    if (!out_report) {
        return ESP_ERR_INVALID_ARG;
    }

    settings_manager_factory_reset_report_t report = {0};
    esp_err_t err = settings_manager_get_last_factory_reset_report(&report);
    if (err != ESP_OK) {
        return err;
    }

    out_report->wifi_err = report.wifi_err;
    out_report->devices_err = report.devices_err;
    out_report->zigbee_storage_err = report.zigbee_storage_err;
    out_report->zigbee_fct_err = report.zigbee_fct_err;
    return ESP_OK;
}

esp_err_t system_service_collect_telemetry(system_telemetry_t *out)
{
    if (!out) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(out, 0, sizeof(*out));
    if (!s_telemetry_impl) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    return s_telemetry_impl(out);
}

#if CONFIG_GATEWAY_SELF_TEST_APP
bool system_service_is_reboot_scheduled_for_test(void)
{
    if (ensure_reboot_mutex() != ESP_OK) {
        return false;
    }
    xSemaphoreTake(s_reboot_mutex, portMAX_DELAY);
    bool scheduled = s_reboot_scheduled;
    xSemaphoreGive(s_reboot_mutex);
    return scheduled;
}

uint32_t system_service_get_reboot_schedule_count_for_test(void)
{
    if (ensure_reboot_mutex() != ESP_OK) {
        return 0;
    }
    xSemaphoreTake(s_reboot_mutex, portMAX_DELAY);
    uint32_t count = s_reboot_schedule_count;
    xSemaphoreGive(s_reboot_mutex);
    return count;
}

void system_service_reset_reboot_singleflight_for_test(void)
{
    if (ensure_reboot_mutex() != ESP_OK) {
        return;
    }
    xSemaphoreTake(s_reboot_mutex, portMAX_DELAY);
    s_reboot_scheduled = false;
    s_reboot_schedule_count = 0;
    xSemaphoreGive(s_reboot_mutex);
}
#endif
