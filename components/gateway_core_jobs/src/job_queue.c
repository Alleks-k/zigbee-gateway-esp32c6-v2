#include "job_queue.h"

#include "gateway_events.h"
#include "job_queue_policy.h"
#include "job_queue_state.h"

#include "esp_event.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "JOB_QUEUE";

typedef struct zgw_job_queue {
    SemaphoreHandle_t mutex;
    QueueHandle_t job_q;
    TaskHandle_t worker;
    zgw_job_slot_t jobs[ZGW_JOB_MAX];
    uint32_t next_id;
    zgw_job_metrics_t metrics;
    uint32_t latency_samples_ms[64];
    size_t latency_samples_count;
    size_t latency_samples_next;
    zigbee_service_handle_t zigbee_service_handle;
    struct wifi_service *wifi_service_handle;
    struct system_service *system_service_handle;
} zgw_job_queue_t;

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

static void execute_job(job_queue_handle_t handle, uint32_t job_id)
{
    zgw_job_type_t type = ZGW_JOB_TYPE_WIFI_SCAN;
    uint32_t reboot_delay_ms = 1000;

    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    int idx = job_queue_find_slot_index_by_id(handle->jobs, job_id);
    if (idx < 0 || !handle->jobs[idx].used) {
        xSemaphoreGive(handle->mutex);
        return;
    }
    handle->jobs[idx].state = ZGW_JOB_STATE_RUNNING;
    handle->jobs[idx].updated_ms = job_queue_now_ms();
    type = handle->jobs[idx].type;
    reboot_delay_ms = handle->jobs[idx].reboot_delay_ms;
    xSemaphoreGive(handle->mutex);

    char result[ZGW_JOB_RESULT_MAX_LEN] = {0};
    esp_err_t exec_err =
        job_queue_policy_execute(type,
                                 reboot_delay_ms,
                                 handle->zigbee_service_handle,
                                 handle->wifi_service_handle,
                                 handle->system_service_handle,
                                 result,
                                 sizeof(result));

    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    idx = job_queue_find_slot_index_by_id(handle->jobs, job_id);
    if (idx >= 0 && handle->jobs[idx].used) {
        uint64_t finished_ms = job_queue_now_ms();
        handle->jobs[idx].err = exec_err;
        handle->jobs[idx].state = (exec_err == ESP_OK) ? ZGW_JOB_STATE_SUCCEEDED : ZGW_JOB_STATE_FAILED;
        handle->jobs[idx].updated_ms = finished_ms;
        uint64_t latency_ms = (finished_ms >= handle->jobs[idx].created_ms) ? (finished_ms - handle->jobs[idx].created_ms) : 0;
        job_queue_push_latency_sample(handle->latency_samples_ms, &handle->latency_samples_count, &handle->latency_samples_next,
                                      (latency_ms > UINT32_MAX) ? UINT32_MAX : (uint32_t)latency_ms);
        if (exec_err == ESP_OK) {
            handle->metrics.completed_total++;
        } else {
            handle->metrics.failed_total++;
        }
        if (exec_err == ESP_OK) {
            job_queue_set_result(&handle->jobs[idx], result);
        } else {
            char fail_json[128];
            int written = snprintf(fail_json, sizeof(fail_json), "{\"error\":\"%s\"}", esp_err_to_name(exec_err));
            if (written > 0 && (size_t)written < sizeof(fail_json)) {
                job_queue_set_result(&handle->jobs[idx], fail_json);
            }
        }
    }
    xSemaphoreGive(handle->mutex);

    if (exec_err == ESP_OK && type == ZGW_JOB_TYPE_LQI_REFRESH) {
        esp_err_t post_ret = esp_event_post(GATEWAY_EVENT, GATEWAY_EVENT_LQI_STATE_CHANGED, NULL, 0, 0);
        if (post_ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to post LQI_STATE_CHANGED: %s", esp_err_to_name(post_ret));
        }
    }
}

static void job_worker_task(void *arg)
{
    job_queue_handle_t handle = (job_queue_handle_t)arg;
    if (!handle) {
        vTaskDelete(NULL);
        return;
    }
    for (;;) {
        uint32_t id = 0;
        if (xQueueReceive(handle->job_q, &id, portMAX_DELAY) == pdTRUE) {
            execute_job(handle, id);
        }
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
        BaseType_t ok = xTaskCreate(job_worker_task, "zgw_jobs", 6144, handle, 5, &handle->worker);
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

esp_err_t job_queue_submit_with_handle(job_queue_handle_t handle, zgw_job_type_t type, uint32_t reboot_delay_ms,
                                       uint32_t *out_job_id)
{
    if (!handle || !out_job_id) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = job_queue_init_with_handle(handle);
    if (err != ESP_OK) {
        return err;
    }

    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    job_queue_prune_completed_jobs(handle->jobs, job_queue_now_ms());
    int inflight_idx = job_queue_find_inflight_slot_index(handle->jobs, type, reboot_delay_ms);
    if (inflight_idx >= 0) {
        uint32_t inflight_id = handle->jobs[inflight_idx].id;
        zgw_job_state_t inflight_state = handle->jobs[inflight_idx].state;
        (void)inflight_state;
        handle->metrics.dedup_reused_total++;
        *out_job_id = inflight_id;
        xSemaphoreGive(handle->mutex);
        ESP_LOGI(TAG, "Job single-flight reuse id=%" PRIu32 " type=%s state=%s", inflight_id, job_queue_type_to_string(type),
                 job_queue_state_to_string(inflight_state));
        return ESP_OK;
    }
    int idx = job_queue_alloc_slot_index(handle->jobs, TAG);
    if (idx < 0) {
        xSemaphoreGive(handle->mutex);
        return ESP_ERR_NO_MEM;
    }

    uint32_t id = handle->next_id++;
    if (handle->next_id == 0) {
        handle->next_id = 1;
    }

    memset(&handle->jobs[idx], 0, sizeof(handle->jobs[idx]));
    handle->jobs[idx].used = true;
    handle->jobs[idx].id = id;
    handle->jobs[idx].type = type;
    handle->jobs[idx].state = ZGW_JOB_STATE_QUEUED;
    handle->jobs[idx].err = ESP_OK;
    handle->jobs[idx].created_ms = job_queue_now_ms();
    handle->jobs[idx].updated_ms = handle->jobs[idx].created_ms;
    handle->jobs[idx].reboot_delay_ms = reboot_delay_ms;
    handle->metrics.submitted_total++;
    handle->metrics.queue_depth_current = job_queue_inflight_depth(handle->jobs);
    if (handle->metrics.queue_depth_current > handle->metrics.queue_depth_peak) {
        handle->metrics.queue_depth_peak = handle->metrics.queue_depth_current;
    }
    xSemaphoreGive(handle->mutex);

    if (xQueueSend(handle->job_q, &id, 0) != pdTRUE) {
        xSemaphoreTake(handle->mutex, portMAX_DELAY);
        handle->jobs[idx].used = false;
        xSemaphoreGive(handle->mutex);
        return ESP_ERR_NO_MEM;
    }

    *out_job_id = id;
    ESP_LOGI(TAG, "Job queued id=%" PRIu32 " type=%s", id, job_queue_type_to_string(type));
    return ESP_OK;
}

esp_err_t job_queue_get_with_handle(job_queue_handle_t handle, uint32_t job_id, zgw_job_info_t *out_info)
{
    if (!handle || !out_info || job_id == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = job_queue_init_with_handle(handle);
    if (err != ESP_OK) {
        return err;
    }

    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    int idx = job_queue_find_slot_index_by_id(handle->jobs, job_id);
    if (idx < 0 || !handle->jobs[idx].used) {
        xSemaphoreGive(handle->mutex);
        return ESP_ERR_NOT_FOUND;
    }

    out_info->id = handle->jobs[idx].id;
    out_info->type = handle->jobs[idx].type;
    out_info->state = handle->jobs[idx].state;
    out_info->err = handle->jobs[idx].err;
    out_info->created_ms = handle->jobs[idx].created_ms;
    out_info->updated_ms = handle->jobs[idx].updated_ms;
    out_info->has_result = handle->jobs[idx].has_result;
    strlcpy(out_info->result_json, handle->jobs[idx].result_json, sizeof(out_info->result_json));
    xSemaphoreGive(handle->mutex);
    return ESP_OK;
}

esp_err_t job_queue_get_metrics_with_handle(job_queue_handle_t handle, zgw_job_metrics_t *out_metrics)
{
    if (!handle || !out_metrics) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = job_queue_init_with_handle(handle);
    if (err != ESP_OK) {
        return err;
    }
    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    handle->metrics.queue_depth_current = job_queue_inflight_depth(handle->jobs);
    handle->metrics.latency_p95_ms = job_queue_latency_p95(handle->latency_samples_ms, handle->latency_samples_count);
    *out_metrics = handle->metrics;
    xSemaphoreGive(handle->mutex);
    return ESP_OK;
}
