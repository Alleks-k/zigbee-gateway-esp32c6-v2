#include "job_queue_internal.h"

#include "gateway_events.h"
#include "job_queue_policy.h"

#include "esp_event.h"
#include "esp_log.h"

#include <stdio.h>

static const char *TAG = "JOB_QUEUE";

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

void job_queue_worker_task(void *arg)
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
