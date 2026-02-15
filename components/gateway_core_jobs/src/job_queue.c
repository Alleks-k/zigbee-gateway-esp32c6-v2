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
#include <string.h>

static const char *TAG = "JOB_QUEUE";

static SemaphoreHandle_t s_mutex = NULL;
static QueueHandle_t s_job_q = NULL;
static TaskHandle_t s_worker = NULL;
static zgw_job_slot_t s_jobs[ZGW_JOB_MAX];
static uint32_t s_next_id = 1;
static zgw_job_metrics_t s_metrics = {0};
static uint32_t s_latency_samples_ms[64];
static size_t s_latency_samples_count = 0;
static size_t s_latency_samples_next = 0;

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

static void execute_job(uint32_t job_id)
{
    zgw_job_type_t type = ZGW_JOB_TYPE_WIFI_SCAN;
    uint32_t reboot_delay_ms = 1000;

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    int idx = job_queue_find_slot_index_by_id(s_jobs, job_id);
    if (idx < 0 || !s_jobs[idx].used) {
        xSemaphoreGive(s_mutex);
        return;
    }
    s_jobs[idx].state = ZGW_JOB_STATE_RUNNING;
    s_jobs[idx].updated_ms = job_queue_now_ms();
    type = s_jobs[idx].type;
    reboot_delay_ms = s_jobs[idx].reboot_delay_ms;
    xSemaphoreGive(s_mutex);

    char result[ZGW_JOB_RESULT_MAX_LEN] = {0};
    esp_err_t exec_err = job_queue_policy_execute(type, reboot_delay_ms, result, sizeof(result));

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    idx = job_queue_find_slot_index_by_id(s_jobs, job_id);
    if (idx >= 0 && s_jobs[idx].used) {
        uint64_t finished_ms = job_queue_now_ms();
        s_jobs[idx].err = exec_err;
        s_jobs[idx].state = (exec_err == ESP_OK) ? ZGW_JOB_STATE_SUCCEEDED : ZGW_JOB_STATE_FAILED;
        s_jobs[idx].updated_ms = finished_ms;
        uint64_t latency_ms = (finished_ms >= s_jobs[idx].created_ms) ? (finished_ms - s_jobs[idx].created_ms) : 0;
        job_queue_push_latency_sample(
            s_latency_samples_ms, &s_latency_samples_count, &s_latency_samples_next,
            (latency_ms > UINT32_MAX) ? UINT32_MAX : (uint32_t)latency_ms);
        if (exec_err == ESP_OK) {
            s_metrics.completed_total++;
        } else {
            s_metrics.failed_total++;
        }
        if (exec_err == ESP_OK) {
            job_queue_set_result(&s_jobs[idx], result);
        } else {
            char fail_json[128];
            int written = snprintf(fail_json, sizeof(fail_json), "{\"error\":\"%s\"}", esp_err_to_name(exec_err));
            if (written > 0 && (size_t)written < sizeof(fail_json)) {
                job_queue_set_result(&s_jobs[idx], fail_json);
            }
        }
    }
    xSemaphoreGive(s_mutex);

    if (exec_err == ESP_OK && type == ZGW_JOB_TYPE_LQI_REFRESH) {
        esp_err_t post_ret = esp_event_post(GATEWAY_EVENT, GATEWAY_EVENT_LQI_STATE_CHANGED, NULL, 0, 0);
        if (post_ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to post LQI_STATE_CHANGED: %s", esp_err_to_name(post_ret));
        }
    }
}

static void job_worker_task(void *arg)
{
    (void)arg;
    for (;;) {
        uint32_t id = 0;
        if (xQueueReceive(s_job_q, &id, portMAX_DELAY) == pdTRUE) {
            execute_job(id);
        }
    }
}

esp_err_t job_queue_init(void)
{
    if (s_mutex && s_job_q && s_worker) {
        return ESP_OK;
    }

    if (!s_mutex) {
        s_mutex = xSemaphoreCreateMutex();
        if (!s_mutex) {
            return ESP_ERR_NO_MEM;
        }
    }

    if (!s_job_q) {
        s_job_q = xQueueCreate(ZGW_JOB_MAX, sizeof(uint32_t));
        if (!s_job_q) {
            return ESP_ERR_NO_MEM;
        }
    }

    if (!s_worker) {
        BaseType_t ok = xTaskCreate(job_worker_task, "zgw_jobs", 6144, NULL, 5, &s_worker);
        if (ok != pdPASS) {
            s_worker = NULL;
            return ESP_ERR_NO_MEM;
        }
    }
    return ESP_OK;
}

esp_err_t job_queue_submit(zgw_job_type_t type, uint32_t reboot_delay_ms, uint32_t *out_job_id)
{
    if (!out_job_id) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = job_queue_init();
    if (err != ESP_OK) {
        return err;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    job_queue_prune_completed_jobs(s_jobs, job_queue_now_ms());
    int inflight_idx = job_queue_find_inflight_slot_index(s_jobs, type, reboot_delay_ms);
    if (inflight_idx >= 0) {
        uint32_t inflight_id = s_jobs[inflight_idx].id;
        zgw_job_state_t inflight_state = s_jobs[inflight_idx].state;
        s_metrics.dedup_reused_total++;
        *out_job_id = inflight_id;
        xSemaphoreGive(s_mutex);
        ESP_LOGI(TAG, "Job single-flight reuse id=%" PRIu32 " type=%s state=%s",
                 inflight_id,
                 job_queue_type_to_string(type),
                 job_queue_state_to_string(inflight_state));
        return ESP_OK;
    }
    int idx = job_queue_alloc_slot_index(s_jobs, TAG);
    if (idx < 0) {
        xSemaphoreGive(s_mutex);
        return ESP_ERR_NO_MEM;
    }

    uint32_t id = s_next_id++;
    if (s_next_id == 0) {
        s_next_id = 1;
    }

    memset(&s_jobs[idx], 0, sizeof(s_jobs[idx]));
    s_jobs[idx].used = true;
    s_jobs[idx].id = id;
    s_jobs[idx].type = type;
    s_jobs[idx].state = ZGW_JOB_STATE_QUEUED;
    s_jobs[idx].err = ESP_OK;
    s_jobs[idx].created_ms = job_queue_now_ms();
    s_jobs[idx].updated_ms = s_jobs[idx].created_ms;
    s_jobs[idx].reboot_delay_ms = reboot_delay_ms;
    s_metrics.submitted_total++;
    s_metrics.queue_depth_current = job_queue_inflight_depth(s_jobs);
    if (s_metrics.queue_depth_current > s_metrics.queue_depth_peak) {
        s_metrics.queue_depth_peak = s_metrics.queue_depth_current;
    }
    xSemaphoreGive(s_mutex);

    if (xQueueSend(s_job_q, &id, 0) != pdTRUE) {
        xSemaphoreTake(s_mutex, portMAX_DELAY);
        s_jobs[idx].used = false;
        xSemaphoreGive(s_mutex);
        return ESP_ERR_NO_MEM;
    }

    *out_job_id = id;
    ESP_LOGI(TAG, "Job queued id=%" PRIu32 " type=%s", id, job_queue_type_to_string(type));
    return ESP_OK;
}

esp_err_t job_queue_get(uint32_t job_id, zgw_job_info_t *out_info)
{
    if (!out_info || job_id == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = job_queue_init();
    if (err != ESP_OK) {
        return err;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    int idx = job_queue_find_slot_index_by_id(s_jobs, job_id);
    if (idx < 0 || !s_jobs[idx].used) {
        xSemaphoreGive(s_mutex);
        return ESP_ERR_NOT_FOUND;
    }

    out_info->id = s_jobs[idx].id;
    out_info->type = s_jobs[idx].type;
    out_info->state = s_jobs[idx].state;
    out_info->err = s_jobs[idx].err;
    out_info->created_ms = s_jobs[idx].created_ms;
    out_info->updated_ms = s_jobs[idx].updated_ms;
    out_info->has_result = s_jobs[idx].has_result;
    strlcpy(out_info->result_json, s_jobs[idx].result_json, sizeof(out_info->result_json));
    xSemaphoreGive(s_mutex);
    return ESP_OK;
}

esp_err_t job_queue_get_metrics(zgw_job_metrics_t *out_metrics)
{
    if (!out_metrics) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = job_queue_init();
    if (err != ESP_OK) {
        return err;
    }
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_metrics.queue_depth_current = job_queue_inflight_depth(s_jobs);
    s_metrics.latency_p95_ms = job_queue_latency_p95(s_latency_samples_ms, s_latency_samples_count);
    *out_metrics = s_metrics;
    xSemaphoreGive(s_mutex);
    return ESP_OK;
}
