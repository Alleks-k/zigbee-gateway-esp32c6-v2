#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

typedef struct {
    uint32_t submitted_total;
    uint32_t dedup_reused_total;
    uint32_t completed_total;
    uint32_t failed_total;
    uint32_t queue_depth_current;
    uint32_t queue_depth_peak;
    uint32_t latency_p95_ms;
} gateway_core_job_metrics_t;

typedef enum {
    GATEWAY_CORE_JOB_TYPE_WIFI_SCAN = 0,
    GATEWAY_CORE_JOB_TYPE_FACTORY_RESET,
    GATEWAY_CORE_JOB_TYPE_REBOOT,
    GATEWAY_CORE_JOB_TYPE_UPDATE,
    GATEWAY_CORE_JOB_TYPE_LQI_REFRESH,
} gateway_core_job_type_t;

typedef enum {
    GATEWAY_CORE_JOB_STATE_QUEUED = 0,
    GATEWAY_CORE_JOB_STATE_RUNNING,
    GATEWAY_CORE_JOB_STATE_SUCCEEDED,
    GATEWAY_CORE_JOB_STATE_FAILED,
} gateway_core_job_state_t;

typedef struct {
    uint32_t id;
    gateway_core_job_type_t type;
    gateway_core_job_state_t state;
    esp_err_t err;
    uint64_t created_ms;
    uint64_t updated_ms;
    bool has_result;
    char result_json[2048];
} gateway_core_job_info_t;

esp_err_t gateway_jobs_get_metrics(gateway_core_job_metrics_t *out_metrics);
esp_err_t gateway_jobs_submit(gateway_core_job_type_t type, uint32_t reboot_delay_ms, uint32_t *out_job_id);
esp_err_t gateway_jobs_get(uint32_t job_id, gateway_core_job_info_t *out_info);
const char *gateway_jobs_type_to_string(gateway_core_job_type_t type);
const char *gateway_jobs_state_to_string(gateway_core_job_state_t state);
