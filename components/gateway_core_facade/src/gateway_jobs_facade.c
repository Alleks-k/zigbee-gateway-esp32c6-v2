#include "gateway_jobs_facade.h"

#include <string.h>

#include "job_queue.h"

static zgw_job_type_t to_job_type(gateway_core_job_type_t type)
{
    switch (type) {
    case GATEWAY_CORE_JOB_TYPE_FACTORY_RESET:
        return ZGW_JOB_TYPE_FACTORY_RESET;
    case GATEWAY_CORE_JOB_TYPE_REBOOT:
        return ZGW_JOB_TYPE_REBOOT;
    case GATEWAY_CORE_JOB_TYPE_UPDATE:
        return ZGW_JOB_TYPE_UPDATE;
    case GATEWAY_CORE_JOB_TYPE_LQI_REFRESH:
        return ZGW_JOB_TYPE_LQI_REFRESH;
    case GATEWAY_CORE_JOB_TYPE_WIFI_SCAN:
    default:
        return ZGW_JOB_TYPE_WIFI_SCAN;
    }
}

static gateway_core_job_type_t from_job_type(zgw_job_type_t type)
{
    switch (type) {
    case ZGW_JOB_TYPE_FACTORY_RESET:
        return GATEWAY_CORE_JOB_TYPE_FACTORY_RESET;
    case ZGW_JOB_TYPE_REBOOT:
        return GATEWAY_CORE_JOB_TYPE_REBOOT;
    case ZGW_JOB_TYPE_UPDATE:
        return GATEWAY_CORE_JOB_TYPE_UPDATE;
    case ZGW_JOB_TYPE_LQI_REFRESH:
        return GATEWAY_CORE_JOB_TYPE_LQI_REFRESH;
    case ZGW_JOB_TYPE_WIFI_SCAN:
    default:
        return GATEWAY_CORE_JOB_TYPE_WIFI_SCAN;
    }
}

static gateway_core_job_state_t from_job_state(zgw_job_state_t state)
{
    switch (state) {
    case ZGW_JOB_STATE_RUNNING:
        return GATEWAY_CORE_JOB_STATE_RUNNING;
    case ZGW_JOB_STATE_SUCCEEDED:
        return GATEWAY_CORE_JOB_STATE_SUCCEEDED;
    case ZGW_JOB_STATE_FAILED:
        return GATEWAY_CORE_JOB_STATE_FAILED;
    case ZGW_JOB_STATE_QUEUED:
    default:
        return GATEWAY_CORE_JOB_STATE_QUEUED;
    }
}

esp_err_t gateway_core_facade_get_job_metrics(gateway_core_job_metrics_t *out_metrics)
{
    if (!out_metrics) {
        return ESP_ERR_INVALID_ARG;
    }

    zgw_job_metrics_t metrics = {0};
    esp_err_t err = job_queue_get_metrics(&metrics);
    if (err != ESP_OK) {
        return err;
    }

    out_metrics->submitted_total = metrics.submitted_total;
    out_metrics->dedup_reused_total = metrics.dedup_reused_total;
    out_metrics->completed_total = metrics.completed_total;
    out_metrics->failed_total = metrics.failed_total;
    out_metrics->queue_depth_current = metrics.queue_depth_current;
    out_metrics->queue_depth_peak = metrics.queue_depth_peak;
    out_metrics->latency_p95_ms = metrics.latency_p95_ms;
    return ESP_OK;
}

esp_err_t gateway_core_facade_job_submit(gateway_core_job_type_t type, uint32_t reboot_delay_ms, uint32_t *out_job_id)
{
    return job_queue_submit(to_job_type(type), reboot_delay_ms, out_job_id);
}

esp_err_t gateway_core_facade_job_get(uint32_t job_id, gateway_core_job_info_t *out_info)
{
    if (!out_info) {
        return ESP_ERR_INVALID_ARG;
    }

    zgw_job_info_t info = {0};
    esp_err_t err = job_queue_get(job_id, &info);
    if (err != ESP_OK) {
        return err;
    }

    out_info->id = info.id;
    out_info->type = from_job_type(info.type);
    out_info->state = from_job_state(info.state);
    out_info->err = info.err;
    out_info->created_ms = info.created_ms;
    out_info->updated_ms = info.updated_ms;
    out_info->has_result = info.has_result;
    memcpy(out_info->result_json, info.result_json, sizeof(out_info->result_json));
    return ESP_OK;
}

const char *gateway_core_facade_job_type_to_string(gateway_core_job_type_t type)
{
    return job_queue_type_to_string(to_job_type(type));
}

const char *gateway_core_facade_job_state_to_string(gateway_core_job_state_t state)
{
    switch (state) {
    case GATEWAY_CORE_JOB_STATE_RUNNING:
        return job_queue_state_to_string(ZGW_JOB_STATE_RUNNING);
    case GATEWAY_CORE_JOB_STATE_SUCCEEDED:
        return job_queue_state_to_string(ZGW_JOB_STATE_SUCCEEDED);
    case GATEWAY_CORE_JOB_STATE_FAILED:
        return job_queue_state_to_string(ZGW_JOB_STATE_FAILED);
    case GATEWAY_CORE_JOB_STATE_QUEUED:
    default:
        return job_queue_state_to_string(ZGW_JOB_STATE_QUEUED);
    }
}
