#include "job_queue_internal.h"

#include "esp_log.h"

#include <inttypes.h>
#include <string.h>

static const char *TAG = "JOB_QUEUE";

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
