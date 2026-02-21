#include "job_queue_internal.h"

#include <stdlib.h>

const char *job_queue_type_to_string(zgw_job_type_t type)
{
    switch (type) {
    case ZGW_JOB_TYPE_WIFI_SCAN: return "scan";
    case ZGW_JOB_TYPE_FACTORY_RESET: return "factory_reset";
    case ZGW_JOB_TYPE_REBOOT: return "reboot";
    case ZGW_JOB_TYPE_UPDATE: return "update";
    case ZGW_JOB_TYPE_LQI_REFRESH: return "lqi_refresh";
    default: return "unknown";
    }
}

const char *job_queue_state_to_string(zgw_job_state_t state)
{
    switch (state) {
    case ZGW_JOB_STATE_QUEUED: return "queued";
    case ZGW_JOB_STATE_RUNNING: return "running";
    case ZGW_JOB_STATE_SUCCEEDED: return "succeeded";
    case ZGW_JOB_STATE_FAILED: return "failed";
    default: return "unknown";
    }
}

esp_err_t job_queue_create(job_queue_handle_t *out_handle)
{
    if (!out_handle) {
        return ESP_ERR_INVALID_ARG;
    }
    zgw_job_queue_t *handle = (zgw_job_queue_t *)calloc(1, sizeof(*handle));
    if (!handle) {
        return ESP_ERR_NO_MEM;
    }
    handle->next_id = 1;
    *out_handle = handle;
    return ESP_OK;
}

void job_queue_destroy(job_queue_handle_t handle)
{
    if (!handle) {
        return;
    }

    if (handle->worker) {
        vTaskDelete(handle->worker);
        handle->worker = NULL;
    }
    if (handle->job_q) {
        vQueueDelete(handle->job_q);
        handle->job_q = NULL;
    }
    if (handle->mutex) {
        vSemaphoreDelete(handle->mutex);
        handle->mutex = NULL;
    }
    free(handle);
}

esp_err_t job_queue_init_with_handle(job_queue_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    if (handle->mutex && handle->job_q && handle->worker) {
        return ESP_OK;
    }

    if (!handle->mutex) {
        handle->mutex = xSemaphoreCreateMutex();
        if (!handle->mutex) {
            return ESP_ERR_NO_MEM;
        }
    }

    if (!handle->job_q) {
        handle->job_q = xQueueCreate(ZGW_JOB_MAX, sizeof(uint32_t));
        if (!handle->job_q) {
            return ESP_ERR_NO_MEM;
        }
    }

    if (!handle->worker) {
        BaseType_t ok = xTaskCreate(job_queue_worker_task, "zgw_jobs", 6144, handle, 5, &handle->worker);
        if (ok != pdPASS) {
            handle->worker = NULL;
            return ESP_ERR_NO_MEM;
        }
    }
    return ESP_OK;
}

esp_err_t job_queue_set_zigbee_service_with_handle(job_queue_handle_t handle, zigbee_service_handle_t zigbee_service_handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = job_queue_init_with_handle(handle);
    if (err != ESP_OK) {
        return err;
    }
    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    handle->zigbee_service_handle = zigbee_service_handle;
    xSemaphoreGive(handle->mutex);
    return ESP_OK;
}

esp_err_t job_queue_set_platform_services_with_handle(job_queue_handle_t handle,
                                                      struct wifi_service *wifi_service_handle,
                                                      struct system_service *system_service_handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = job_queue_init_with_handle(handle);
    if (err != ESP_OK) {
        return err;
    }
    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    handle->wifi_service_handle = wifi_service_handle;
    handle->system_service_handle = system_service_handle;
    xSemaphoreGive(handle->mutex);
    return ESP_OK;
}
